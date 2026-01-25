//
// DeviceResources.cpp - Direct3D 11 디바이스와 스왑 체인을 감싸는 래퍼(Wrapper) 구현 파일
//                        (DirectX 11.1 런타임 필요)
//

#include "pch.h"             // 미리 컴파일된 헤더 포함
#include "DeviceResources.h" // DeviceResources 클래스 선언 포함

using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr; // COM 스마트 포인터 사용

// Clang 컴파일러 경고 무시 설정
#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

// switch 문에서 모든 열거형 케이스를 다루지 않았다는 경고(4061) 무시
#pragma warning(disable : 4061)

namespace
{
    // 디버그 빌드(_DEBUG)에서만 사용하는 헬퍼 함수
#if defined(_DEBUG)
    // SDK 디버그 레이어(Debug Layer)가 설치되어 있는지 확인하는 함수
    inline bool SdkLayersAvailable() noexcept
    {
        // NULL 드라이버로 디버그 디바이스 생성을 시도해 봅니다.
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_NULL,       // 실제 하드웨어 장치를 만들지 않음
            nullptr,
            D3D11_CREATE_DEVICE_DEBUG,  // 디버그 레이어 활성화 요청 플래그
            nullptr,                    // 기능 수준 상관없음
            0,
            D3D11_SDK_VERSION,
            nullptr,                    // 디바이스 포인터 필요 없음
            nullptr,                    // 기능 수준 확인 필요 없음
            nullptr                     // 컨텍스트 포인터 필요 없음
        );

        return SUCCEEDED(hr); // 성공하면 디버그 레이어가 설치된 것임
    }
#endif

    // sRGB 포맷을 일반 UNORM 포맷으로 변환해주는 헬퍼 함수
    // (Flip 모델 스왑 체인은 sRGB 포맷을 직접 지원하지 않을 수 있음)
    inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8X8_UNORM;
        default:                                return fmt;
        }
    }

    // 두 사각형(윈도우와 모니터 화면)이 겹치는 영역의 넓이를 계산하는 함수
    // (창이 어느 모니터에 가장 많이 걸쳐있는지 판단할 때 사용)
    inline long ComputeIntersectionArea(
        long ax1, long ay1, long ax2, long ay2,
        long bx1, long by1, long bx2, long by2) noexcept
    {
        return std::max(0l, std::min(ax2, bx2) - std::max(ax1, bx1)) * std::max(0l, std::min(ay2, by2) - std::max(ay1, by1));
    }
}

// DeviceResources 생성자: 멤버 변수 초기화
DeviceResources::DeviceResources(
    DXGI_FORMAT backBufferFormat,
    DXGI_FORMAT depthBufferFormat,
    UINT backBufferCount,
    D3D_FEATURE_LEVEL minFeatureLevel,
    unsigned int flags) noexcept :
    m_screenViewport{},
    m_backBufferFormat(backBufferFormat),
    m_depthBufferFormat(depthBufferFormat),
    m_backBufferCount(backBufferCount),
    m_d3dMinFeatureLevel(minFeatureLevel),
    m_window(nullptr),
    m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
    m_outputSize{ 0, 0, 1, 1 },
    m_colorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709), // 기본 SDR 색공간
    m_options(flags | c_FlipPresent), // 기본적으로 Flip Present 방식 사용
    m_deviceNotify(nullptr)
{
}

// Direct3D 디바이스와 컨텍스트를 생성하는 함수 (창 크기와 무관한 초기화)
void DeviceResources::CreateDeviceResources()
{
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // Direct2D 상호 운용성을 위해 BGRA 지원 필수

#if defined(_DEBUG)
    if (SdkLayersAvailable())
    {
        // 디버그 빌드이고 SDK 레이어가 있다면 디버그 플래그 추가
        // (메모리 누수나 API 오류를 자세히 알려줌)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
    else
    {
        OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
    }
#endif

    CreateFactory(); // DXGI 팩토리 생성

    // 가변 주사율(Tearing/FreeSync) 지원 여부 확인
    if (m_options & c_AllowTearing)
    {
        BOOL allowTearing = FALSE;

        ComPtr<IDXGIFactory5> factory5;
        HRESULT hr = m_dxgiFactory.As(&factory5); // DXGI 1.5 인터페이스 요청
        if (SUCCEEDED(hr))
        {
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        // 지원하지 않으면 옵션 끄기
        if (FAILED(hr) || !allowTearing)
        {
            m_options &= ~c_AllowTearing;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
    }

    // HDR 지원 여부 확인 (OS가 Flip 모델을 지원해야 함)
    if (m_options & c_EnableHDR)
    {
        ComPtr<IDXGIFactory5> factory5;
        if (FAILED(m_dxgiFactory.As(&factory5)))
        {
            m_options &= ~c_EnableHDR; // 지원 안 하면 끔
#ifdef _DEBUG
            OutputDebugStringA("WARNING: HDR swap chains not supported");
#endif
        }
    }

    // Flip 모델 지원 여부 확인 (Windows 10 이상)
    if (m_options & c_FlipPresent)
    {
        ComPtr<IDXGIFactory4> factory4;
        if (FAILED(m_dxgiFactory.As(&factory4)))
        {
            m_options &= ~c_FlipPresent; // 지원 안 하면 끔 (Windows 7 등)
#ifdef _DEBUG
            OutputDebugStringA("INFO: Flip swap effects not supported");
#endif
        }
    }

    // 앱이 지원할 DirectX 기능 수준 목록 정의 (내림차순)
    static const D3D_FEATURE_LEVEL s_featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    UINT featLevelCount = 0;
    // 요청한 최소 기능 수준(m_d3dMinFeatureLevel)보다 높은 것들만 추림
    for (; featLevelCount < static_cast<UINT>(std::size(s_featureLevels)); ++featLevelCount)
    {
        if (s_featureLevels[featLevelCount] < m_d3dMinFeatureLevel)
            break;
    }

    if (!featLevelCount)
    {
        throw std::out_of_range("minFeatureLevel too high"); // 최소 요구사항 만족 불가 시 예외 발생
    }

    ComPtr<IDXGIAdapter1> adapter;
    GetHardwareAdapter(adapter.GetAddressOf()); // 최적의 그래픽 카드(어댑터) 가져오기

    // Direct3D 11 디바이스와 컨텍스트 생성
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    HRESULT hr = E_FAIL;
    if (adapter)
    {
        // 하드웨어 어댑터를 사용하여 디바이스 생성
        hr = D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN, // 어댑터를 직접 지정했으므로 UNKNOWN 사용
            nullptr,
            creationFlags,
            s_featureLevels,
            featLevelCount,
            D3D11_SDK_VERSION,
            device.GetAddressOf(),  // 생성된 디바이스 반환
            &m_d3dFeatureLevel,     // 결정된 기능 수준 반환
            context.GetAddressOf()  // 생성된 컨텍스트 반환
        );
    }
#if defined(NDEBUG)
    else
    {
        throw std::runtime_error("No Direct3D hardware device found");
    }
#else
    if (FAILED(hr))
    {
        // 하드웨어 가속 실패 시, WARP(소프트웨어 렌더러) 드라이버로 시도 (주로 VM이나 구형 PC용)
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP, // WARP 드라이버 사용
            nullptr,
            creationFlags,
            s_featureLevels,
            featLevelCount,
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &m_d3dFeatureLevel,
            context.GetAddressOf()
        );

        if (SUCCEEDED(hr))
        {
            OutputDebugStringA("Direct3D Adapter - WARP\n");
        }
    }
#endif

    ThrowIfFailed(hr); // 디바이스 생성 실패 시 예외 발생

    // 디버그 레이어 메시지 필터링 설정 (너무 사소한 경고 숨김)
#ifndef NDEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(device.As(&d3dDebug)))
    {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            // 에러나 데이터 손상 발생 시 즉시 중단점(Break) 걸기
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
            // 무시할 특정 메시지 ID 목록
            D3D11_MESSAGE_ID hide[] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    // ID3D11Device1 / ID3D11DeviceContext1 인터페이스(DX11.1)로 업그레이드하여 멤버 변수에 저장
    ThrowIfFailed(device.As(&m_d3dDevice));
    ThrowIfFailed(context.As(&m_d3dContext));
    ThrowIfFailed(context.As(&m_d3dAnnotation)); // 디버그 주석용 인터페이스
}

// 윈도우 크기가 변경될 때마다 다시 생성해야 하는 리소스들 (스왑체인, 렌더타겟 뷰 등)
void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_window)
    {
        throw std::logic_error("Call SetWindow with a valid Win32 window handle");
    }

    // 기존 렌더 타겟과 뷰들을 모두 해제하고 컨텍스트를 비웁니다.
    m_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_d3dRenderTargetView.Reset();
    m_d3dDepthStencilView.Reset();
    m_renderTarget.Reset();
    m_depthStencil.Reset();
    m_d3dContext->Flush(); // 대기 중인 명령 강제 실행

    // 렌더 타겟 크기 결정 (최소 1x1 픽셀)
    const UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(m_outputSize.right - m_outputSize.left), 1u);
    const UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(m_outputSize.bottom - m_outputSize.top), 1u);
    // Flip 모델 사용 시 sRGB 제거
    const DXGI_FORMAT backBufferFormat = (m_options & (c_FlipPresent | c_AllowTearing | c_EnableHDR)) ? NoSRGB(m_backBufferFormat) : m_backBufferFormat;

    if (m_swapChain)
    {
        // 스왑체인이 이미 있다면 크기만 조절(Resize)합니다. (새로 만드는 것보다 효율적)
        HRESULT hr = m_swapChain->ResizeBuffers(
            m_backBufferCount,
            backBufferWidth,
            backBufferHeight,
            backBufferFormat,
            (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
        );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // 리사이즈 중 디바이스가 소실되었다면 복구 절차 진행
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            HandleDeviceLost();
            return;
        }
        else
        {
            ThrowIfFailed(hr);
        }
    }
    else
    {
        // 스왑체인이 없다면 새로 생성합니다.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더링 대상으로 사용
        swapChainDesc.BufferCount = m_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1; // 멀티샘플링(MSAA) 끄기 (Flip 모델 필수)
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        // Flip Discard 모드가 최신 방식 (없으면 일반 Discard 사용)
        swapChainDesc.SwapEffect = (m_options & (c_FlipPresent | c_AllowTearing | c_EnableHDR)) ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE; // 창 모드로 시작

        // 윈도우용 스왑체인 생성
        ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr, m_swapChain.ReleaseAndGetAddressOf()
        ));

        // Alt+Enter 전체화면 전환 기능을 DXGI가 자동으로 처리하지 못하게 막음 (직접 처리하기 위해)
        ThrowIfFailed(m_dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // HDR 등을 위한 색공간 설정 업데이트
    UpdateColorSpace();

    // 1. 스왑체인의 백버퍼를 가져옵니다 (텍스처).
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));

    // 2. 백버퍼를 가리키는 렌더 타겟 뷰(RTV)를 생성합니다.
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, m_backBufferFormat);
    ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(
        m_renderTarget.Get(),
        &renderTargetViewDesc,
        m_d3dRenderTargetView.ReleaseAndGetAddressOf()
    ));

    if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        // 3D 렌더링에 필요한 깊이 스텐실 버퍼 생성
        CD3D11_TEXTURE2D_DESC depthStencilDesc(
            m_depthBufferFormat,
            backBufferWidth,
            backBufferHeight,
            1, // 배열 크기 1
            1, // 밉맵 레벨 1
            D3D11_BIND_DEPTH_STENCIL // 깊이 스텐실로 바인딩
        );

        // 깊이 버퍼 텍스처 생성
        ThrowIfFailed(m_d3dDevice->CreateTexture2D(
            &depthStencilDesc,
            nullptr,
            m_depthStencil.ReleaseAndGetAddressOf()
        ));

        // 깊이 스텐실 뷰(DSV) 생성
        ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(
            m_depthStencil.Get(),
            nullptr,
            m_d3dDepthStencilView.ReleaseAndGetAddressOf()
        ));
    }

    // 뷰포트 설정 (화면 전체를 다 쓰도록)
    m_screenViewport = { 0.0f, 0.0f, static_cast<float>(backBufferWidth), static_cast<float>(backBufferHeight), 0.f, 1.f };
}

// 윈도우 생성/재생성 시 호출되어 핸들과 크기를 저장
void DeviceResources::SetWindow(HWND window, int width, int height) noexcept
{
    m_window = window;

    m_outputSize.left = m_outputSize.top = 0;
    m_outputSize.right = static_cast<long>(width);
    m_outputSize.bottom = static_cast<long>(height);
}

// 윈도우 크기가 변경되었을 때 호출 (Main.cpp의 WM_SIZE에서 호출)
bool DeviceResources::WindowSizeChanged(int width, int height)
{
    if (!m_window)
        return false;

    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = static_cast<long>(width);
    newRc.bottom = static_cast<long>(height);

    // 크기가 이전과 똑같다면 리소스를 재생성하지 않고 리턴
    if (newRc.right == m_outputSize.right && newRc.bottom == m_outputSize.bottom)
    {
        // 다만 창 위치가 바뀌어 HDR 모니터로 이동했을 수 있으니 색공간은 확인
        UpdateColorSpace();
        return false;
    }

    m_outputSize = newRc;
    CreateWindowSizeDependentResources(); // 크기가 바뀌었으니 리소스 재생성
    return true;
}

// 디바이스 소실(Device Lost) 처리: 모든 리소스를 해제하고 다시 만듦
void DeviceResources::HandleDeviceLost()
{
    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceLost(); // 게임 클래스에 알림 (게임 자체 리소스 해제 유도)
    }

    // 모든 D3D 객체 해제
    m_d3dDepthStencilView.Reset();
    m_d3dRenderTargetView.Reset();
    m_renderTarget.Reset();
    m_depthStencil.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
    m_d3dAnnotation.Reset();

#ifdef _DEBUG
    // 디버그 모드에서 해제되지 않은 객체(메모리 누수)가 있는지 리포트 출력
    {
        ComPtr<ID3D11Debug> d3dDebug;
        if (SUCCEEDED(m_d3dDevice.As(&d3dDebug)))
        {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
        }
    }
#endif

    m_d3dDevice.Reset();
    m_dxgiFactory.Reset();

    // 처음부터 다시 생성
    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceRestored(); // 게임 클래스에 복구 알림
    }
}

// 화면 출력 (Present)
void DeviceResources::Present()
{
    HRESULT hr = E_FAIL;
    if (m_options & c_AllowTearing)
    {
        // Tearing(가변 주사율) 허용 시 동기화 없이 즉시 출력 (인자 0)
        hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    else
    {
        // 일반적인 경우 VSync 대기 (인자 1) -> 여기서 60FPS 제한이 걸림
        // CPU가 다음 VSync까지 대기(Sleep) 상태로 들어감
        hr = m_swapChain->Present(1, 0);
    }

    // 렌더 타겟 뷰 내용 폐기 (다음 프레임에 덮어쓸 것이므로 최적화)
    m_d3dContext->DiscardView(m_d3dRenderTargetView.Get());

    if (m_d3dDepthStencilView)
    {
        m_d3dContext->DiscardView(m_d3dDepthStencilView.Get());
    }

    // 출력 중 디바이스가 소실되었다면(드라이버 업데이트 등) 복구 절차 수행
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
            static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
        OutputDebugStringA(buff);
#endif
        HandleDeviceLost();
    }
    else
    {
        ThrowIfFailed(hr);

        // DXGI 팩토리 정보가 낡았다면(모니터 설정 변경 등) 색공간 업데이트
        if (!m_dxgiFactory->IsCurrent())
        {
            UpdateColorSpace();
        }
    }
}

// DXGI 팩토리 생성 함수
void DeviceResources::CreateFactory()
{
#if defined(_DEBUG) && !defined(__MINGW32__)
    // 디버그 모드일 때 DXGI 디버깅 활성화 시도
    bool debugDXGI = false;
    {
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            debugDXGI = true;

            // 디버그용 팩토리 생성
            ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            // 특정 경고 메시지 무시 설정
            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* 스왑체인 소유권 관련 경고 무시 */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
    }

    if (!debugDXGI)
#endif
        // 릴리즈 모드거나 디버그 인터페이스 실패 시 일반 팩토리 생성
        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
}

// 사용 가능한 하드웨어 중 가장 성능 좋은 어댑터(GPU)를 찾는 함수
void DeviceResources::GetHardwareAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = m_dxgiFactory.As(&factory6);

    // Windows 10 RS4 이상 (DXGI 1.6) 지원 시 고성능 GPU 우선 검색
    if (SUCCEEDED(hr))
    {
        for (UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, // 고성능 GPU 선호
                IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
                adapterIndex++)
        {
            DXGI_ADAPTER_DESC1 desc;
            ThrowIfFailed(adapter->GetDesc1(&desc));

            // 소프트웨어 렌더러(Microsoft Basic Render Driver)는 제외
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }
            // 찾았으면 루프 종료
            break;
        }
    }

    // DXGI 1.6을 지원하지 않는 구형 윈도우라면 일반적인 순서로 검색
    if (!adapter)
    {
        for (UINT adapterIndex = 0;
            SUCCEEDED(m_dxgiFactory->EnumAdapters1(
                adapterIndex,
                adapter.ReleaseAndGetAddressOf()));
                adapterIndex++)
        {
            DXGI_ADAPTER_DESC1 desc;
            ThrowIfFailed(adapter->GetDesc1(&desc));

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }
            break;
        }
    }

    *ppAdapter = adapter.Detach(); // 찾은 어댑터 반환
}

// HDR 출력을 위한 색상 공간 설정 함수
void DeviceResources::UpdateColorSpace()
{
    if (!m_dxgiFactory) return;

    if (!m_dxgiFactory->IsCurrent())
    {
        CreateFactory(); // 팩토리가 낡았으면 재생성
    }

    // 기본은 SDR (Standard Dynamic Range)
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    bool isDisplayHDR10 = false;

    if (m_swapChain)
    {
        // 현재 창이 위치한 모니터가 HDR을 지원하는지 확인
        RECT windowBounds;
        GetWindowRect(m_window, &windowBounds);

        // ... (창과 모니터의 교차 영역 계산 로직 생략: 주석 참고) ...
        // 가장 많이 겹치는 모니터(Output)를 찾아서 그 모니터의 색상 공간을 확인합니다.
        // 만약 HDR10(BT.2020)을 지원하면 isDisplayHDR10 = true;
    }

    // HDR 옵션이 켜져 있고 모니터도 지원한다면
    if ((m_options & c_EnableHDR) && isDisplayHDR10)
    {
        switch (m_backBufferFormat)
        {
        case DXGI_FORMAT_R10G10B10A2_UNORM: // 10비트 정수 포맷
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020; // HDR10 표준
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: // 16비트 실수 포맷
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709; // 선형 scRGB
            break;
        }
    }

    m_colorSpace = colorSpace;

    // 스왑체인에 최종 결정된 색상 공간을 설정
    ComPtr<IDXGISwapChain3> swapChain3;
    if (m_swapChain && SUCCEEDED(m_swapChain.As(&swapChain3)))
    {
        UINT colorSpaceSupport = 0;
        if (SUCCEEDED(swapChain3->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
            && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
        {
            ThrowIfFailed(swapChain3->SetColorSpace1(colorSpace));
        }
    }
}