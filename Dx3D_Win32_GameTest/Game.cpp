//
// Game.cpp
//

#include "pch.h"      // 미리 컴파일된 헤더 (컴파일 속도 향상용 표준 헤더들이 들어있음)
#include "Game.h"     // Game 클래스 선언 헤더 포함
#include "directxtk/WICTextureLoader.h" // WIC(Windows Imaging Component)를 이용해 텍스처(PNG, JPG 등)를 로드하는 헬퍼


extern void ExitGame() noexcept; // 외부에서 정의된 게임 종료 함수 선언 (필요시 호출)

using namespace DirectX; // DirectX 네임스페이스 사용 (SimpleMath, Colors 등 사용 용이)

using Microsoft::WRL::ComPtr; // COM 객체용 스마트 포인터 (메모리 자동 관리)

// 생성자: 게임 인스턴스가 생성될 때 호출됨
Game::Game() noexcept(false)
{
    // DeviceResources 객체 생성 (D3D 디바이스, 컨텍스트, 스왑체인 관리자)
    m_deviceResources = std::make_unique<DX::DeviceResources>();

    // TODO: 스왑체인 포맷이나 백버퍼 개수 등을 변경하려면 여기서 설정합니다.
    // DX::DeviceResources::c_AllowTearing: 가변 주사율 모니터(G-Sync/FreeSync) 지원 옵션
    // DX::DeviceResources::c_EnableHDR: HDR10 디스플레이 지원 옵션

    // 디바이스 소실/복구 시 알림을 받기 위해 'this'(Game 클래스)를 리스너로 등록합니다.
    m_deviceResources->RegisterDeviceNotify(this);
}

// 초기화: 실행에 필요한 Direct3D 리소스를 초기화합니다.
void Game::Initialize(HWND window, int width, int height)
{
    // 윈도우 핸들과 크기 정보를 DeviceResources에 전달합니다.
    m_deviceResources->SetWindow(window, width, height);

    // D3D 디바이스와 스왑체인을 실제로 생성합니다.
    m_deviceResources->CreateDeviceResources();

    // 디바이스에 의존적인 리소스(텍스처, 쉐이더 등)를 생성합니다. -> 여기서 고양이 그림 로딩
    CreateDeviceDependentResources();

    // 윈도우 크기에 의존적인 리소스(백버퍼, 뷰포트 등)를 생성합니다.
    m_deviceResources->CreateWindowSizeDependentResources();

    // 윈도우 크기에 맞춰 우리 게임의 설정(화면 중앙 좌표 등)도 업데이트합니다.
    CreateWindowSizeDependentResources();

    // TODO: 고정 프레임(예: 60FPS)을 원한다면 타이머 설정을 여기서 변경합니다.
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

#pragma region Frame Update
// 기본 게임 루프를 실행합니다. (WinMain 등에서 계속 호출됨)
void Game::Tick()
{
    // 타이머를 갱신하고, 그 시간 값으로 람다 함수(Update)를 실행합니다.
    m_timer.Tick([&]()
        {
            Update(m_timer);
        });

    // 화면을 그립니다.
    Render();
}

// 월드(게임 로직)를 업데이트합니다.
void Game::Update(DX::StepTimer const& timer)
{
    // 지난 프레임과의 시간 차이(Delta Time)를 가져옵니다.
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: 여기에 캐릭터 이동, 충돌 처리 등 게임 로직을 작성합니다.
    elapsedTime; // (경고 방지용: 사용하지 않는 변수 처리)
}
#pragma endregion

#pragma region Frame Render
// 장면(Scene)을 그립니다.
void Game::Render()
{
    // 첫 번째 Update가 실행되기 전에는 아무것도 그리지 않습니다. (안전 장치)
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // 화면(백버퍼)을 지웁니다.
    Clear();

    // PIX 툴(그래픽 디버거)에서 프레임을 식별하기 위한 시작 마커입니다.
    m_deviceResources->PIXBeginEvent(L"Render");

    // TODO: 렌더링 코드를 여기에 작성합니다.
    auto context = m_deviceResources->GetD3DDeviceContext(); // 디바이스 컨텍스트 가져오기

    float time = float(m_timer.GetTotalSeconds());

    // 스프라이트 배치를 시작합니다. (그리기 준비)
    m_spriteBatch->Begin(
        SpriteSortMode_Deferred, 
        m_states->NonPremultiplied(), 
        m_states->LinearWrap());

    // 텍스처(고양이)를 그립니다.
    m_spriteBatch->Draw(
        m_texture.Get(),    // 그릴 텍스처 리소스 (ID3D11ShaderResourceView*)
        m_screenPos,        // 화면상 위치 (화면 중앙)
        &m_tileRect,        // 소스 사각형 (nullptr이면 이미지 전체 그림)
        Colors::White,      // 틴트 색상 (White면 원본 색상 그대로)
        0.f,                // 회전 각도 (라디안 단위, 0이면 회전 없음)
        m_origin,           // 회전/크기변환의 기준점 (이미지의 중심점)
        1.f                 // 스프라이트 크기 조절
    );

    // 스프라이트 배치를 종료합니다. (실제로 GPU에 그리기 명령 전송)
    m_spriteBatch->End();

    // PIX 이벤트 종료 마커
    m_deviceResources->PIXEndEvent();

    // 백버퍼를 화면에 표시합니다. (Present/SwapBuffer)
    m_deviceResources->Present();
}

// 백버퍼를 지우는 헬퍼 메서드입니다.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear"); // 디버깅용 마커

    // 뷰(렌더 타겟, 깊이 스텐실)를 가져옵니다.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    // 렌더 타겟(화면)을 'CornflowerBlue' 색상으로 채웁니다.
    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);

    // 깊이/스텐실 버퍼를 초기화합니다.
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 그릴 대상을 백버퍼로 설정합니다.
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // 뷰포트(그릴 영역)를 설정합니다.
    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent(); // 디버깅용 마커 종료
}
#pragma endregion

#pragma region Message Handlers
// 메시지 핸들러들 (운영체제 이벤트를 처리)
void Game::OnActivated()
{
    // TODO: 창이 활성화될 때 처리 (예: 게임 일시정지 해제)
    m_isActive = true;
}

void Game::OnDeactivated()
{
    // TODO: 창이 비활성화될 때 처리 (예: 게임 자동 일시정지)
    m_isActive = false;
}

void Game::OnSuspending()
{
    // TODO: 절전 모드 진입 시 처리 (상태 저장 등)
}

void Game::OnResuming()
{
    // 타이머의 경과 시간을 리셋합니다. (중단된 시간 동안 게임 시간이 튀는 것을 방지)
    m_timer.ResetElapsedTime();

    // TODO: 절전 모드 복귀 시 처리
}

void Game::OnWindowMoved()
{
    // 창 이동 시 모니터 변경 등을 감지하여 처리합니다.
    const auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    // 디스플레이 설정 변경 시 색상 공간 등을 업데이트합니다.
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    // 크기가 실제로 변경되지 않았으면 리턴
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    // 변경된 창 크기에 맞춰 리소스를 다시 생성합니다.
    CreateWindowSizeDependentResources();

    // TODO: 창 크기 변경 시 추가 게임 로직 처리
}

// 속성 (기본 창 크기 설정)
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: 원하는 기본 해상도를 설정하세요. (최소 320x200)
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// 디바이스에 의존적인 리소스 생성 (창 크기와 무관하게 유지되는 것들: 텍스처, 모델 등)
void Game::CreateDeviceDependentResources()
{
    // D3D 디바이스와 컨텍스트를 가져옵니다.
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    // SpriteBatch 객체를 생성합니다 (2D 그리기 도구).
    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    // 텍스처 로딩을 위한 임시 리소스 포인터
    ComPtr<ID3D11Resource> resource;

    // DirectXTK 함수를 사용해 "cat.png" 파일을 로드합니다.
    DX::ThrowIfFailed(
        CreateWICTextureFromFile(
            device,
            L"cat.dds", // 파일명 (프로젝트 폴더에 이 파일이 있어야 함)
            resource.GetAddressOf(), // 로드된 텍스처 리소스(정보 확인용)
            m_texture.ReleaseAndGetAddressOf() // 셰이더 리소스 뷰(실제 그릴 때 사용)
        ));

    // 리소스 인터페이스를 Texture2D 인터페이스로 변환합니다 (너비/높이를 알기 위해).
    ComPtr<ID3D11Texture2D> cat;
    DX::ThrowIfFailed(resource.As(&cat));

    // 텍스처의 상세 정보(너비, 높이 등)를 가져옵니다.
    CD3D11_TEXTURE2D_DESC catDesc;
    cat->GetDesc(&catDesc);

    // 텍스처의 중심점을 계산합니다 (너비*2, 높이*2).
    m_origin.x = float(catDesc.Width * 2);
    m_origin.y = float(catDesc.Height * 2);

    // 스프라이트 타일링
    m_tileRect.left = catDesc.Width * 2;
    m_tileRect.right = catDesc.Width * 6;
    m_tileRect.top = catDesc.Height * 2;
    m_tileRect.bottom = catDesc.Height * 6;

    m_states = std::make_unique<CommonStates>(device);
}

// 윈도우 크기가 변경될 때마다 다시 계산해야 하는 리소스 생성
void Game::CreateWindowSizeDependentResources()
{
    // 현재 윈도우 크기를 가져옵니다.
    auto size = m_deviceResources->GetOutputSize();

    // 이미지를 그릴 위치를 화면의 정중앙으로 설정합니다.
    m_screenPos.x = float(size.right) / 2.f;
    m_screenPos.y = float(size.bottom) / 2.f;
}

// 디바이스(그래픽 카드)가 소실되었을 때 호출 (예: 드라이버 업데이트, TDR)
void Game::OnDeviceLost()
{
    // 생성했던 디바이스 의존적 리소스들을 해제합니다.
    m_texture.Reset();
    m_spriteBatch.reset();
    m_states.reset();
}

// 디바이스가 복구되었을 때 호출
void Game::OnDeviceRestored()
{
    // 리소스들을 다시 생성합니다.
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion