//
// DeviceResources.h - Direct3D 11 디바이스와 스왑 체인을 감싸는 래퍼(Wrapper) 클래스입니다.
//

#pragma once // 이 헤더 파일이 컴파일 시 중복 포함되는 것을 방지합니다.

namespace DX
{
    // DeviceResources를 소유한 애플리케이션(Game 클래스 등)에게 
    // 디바이스 소실(Device Lost)이나 복구 이벤트를 알리기 위한 인터페이스입니다.
    interface IDeviceNotify
    {
        // 그래픽 장치 연결이 끊겼을 때(드라이버 업데이트, 충돌 등) 호출됩니다.
        virtual void OnDeviceLost() = 0;

    // 그래픽 장치가 다시 복구되었을 때 호출됩니다.
    virtual void OnDeviceRestored() = 0;

protected:
    // 소멸자입니다. 인터페이스이므로 구현부는 없습니다.
    ~IDeviceNotify() = default;
    };

    // 모든 DirectX 장치 리소스를 제어하고 관리하는 클래스입니다.
    class DeviceResources
    {
    public:
        // 플래그 상수 정의
        static constexpr unsigned int c_FlipPresent = 0x1;  // 최신 윈도우(Flip Model) 방식 사용 (성능 향상)
        static constexpr unsigned int c_AllowTearing = 0x2; // 가변 주사율(FreeSync, G-Sync) 지원 허용
        static constexpr unsigned int c_EnableHDR = 0x4;    // HDR10(High Dynamic Range) 디스플레이 지원

        // 생성자: 백버퍼 포맷, 깊이 버퍼 포맷, 버퍼 개수, 최소 기능 수준, 옵션 플래그를 설정합니다.
        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM, // 기본 색상 포맷 (BGRA)
            DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,     // 기본 깊이 포맷 (32비트 실수)
            UINT backBufferCount = 2,                                  // 더블 버퍼링 (2개)
            D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_10_0,// 최소 지원 하드웨어 (DX10 이상)
            unsigned int flags = c_FlipPresent) noexcept;              // 기본적으로 Flip 모델 사용

        ~DeviceResources() = default; // 기본 소멸자

        // 이동 생성자 및 이동 대입 연산자 (리소스 소유권 이전 가능)
        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        // 복사 생성자 및 복사 대입 연산자 삭제 (리소스의 유일성을 보장하기 위함)
        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        // 초기화 및 관리 함수들
        void CreateDeviceResources();              // 창 크기와 무관한 장치(Device, Context)를 생성합니다.
        void CreateWindowSizeDependentResources(); // 창 크기에 의존적인 리소스(SwapChain, RenderTarget)를 생성합니다.
        void SetWindow(HWND window, int width, int height) noexcept; // 렌더링할 윈도우 핸들과 크기를 설정합니다.
        bool WindowSizeChanged(int width, int height); // 창 크기가 변경되었을 때 호출되어 리소스를 갱신합니다.
        void HandleDeviceLost();                   // 장치 소실 발생 시 복구 절차를 수행합니다.

        // 디바이스 상태 변화를 알릴 리스너(Game 클래스)를 등록합니다.
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }

        void Present();           // 그려진 화면(백버퍼)을 모니터에 출력(Swap)합니다.
        void UpdateColorSpace();  // HDR 모니터 연결 등의 변경 시 색상 공간을 업데이트합니다.

        // 디바이스 접근자 (Getter) - 화면 크기를 반환
        RECT GetOutputSize() const noexcept { return m_outputSize; }

        // Direct3D 핵심 객체 접근자 (Getter)
        // 스마트 포인터(.Get())를 통해 원본 포인터를 반환합니다.
        auto                    GetD3DDevice() const noexcept { return m_d3dDevice.Get(); }                  // 리소스 생성 공장(Device)
        auto                    GetD3DDeviceContext() const noexcept { return m_d3dContext.Get(); }          // 그리기 명령 수행자(Context)
        auto                    GetSwapChain() const noexcept { return m_swapChain.Get(); }                  // 화면 버퍼 관리자(SwapChain)
        auto                    GetDXGIFactory() const noexcept { return m_dxgiFactory.Get(); }              // DXGI 팩토리 (어댑터 등 관리)
        HWND                    GetWindow() const noexcept { return m_window; }                              // 연결된 윈도우 핸들
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const noexcept { return m_d3dFeatureLevel; }         // 현재 하드웨어의 기능 수준
        ID3D11Texture2D*        GetRenderTarget() const noexcept { return m_renderTarget.Get(); }            // 그림이 그려지는 종이(텍스처)
        ID3D11Texture2D*        GetDepthStencil() const noexcept { return m_depthStencil.Get(); }            // 깊이 정보(Z-buffer) 텍스처
        ID3D11RenderTargetView* GetRenderTargetView() const noexcept { return m_d3dRenderTargetView.Get(); } // 렌더 타겟 뷰(RTV)
        ID3D11DepthStencilView* GetDepthStencilView() const noexcept { return m_d3dDepthStencilView.Get(); } // 깊이 스텐실 뷰(DSV)
        DXGI_FORMAT             GetBackBufferFormat() const noexcept { return m_backBufferFormat; }          // 백버퍼 색상 포맷
        DXGI_FORMAT             GetDepthBufferFormat() const noexcept { return m_depthBufferFormat; }        // 깊이 버퍼 포맷
        D3D11_VIEWPORT          GetScreenViewport() const noexcept { return m_screenViewport; }              // 화면 뷰포트 설정
        UINT                    GetBackBufferCount() const noexcept { return m_backBufferCount; }            // 백버퍼 개수
        DXGI_COLOR_SPACE_TYPE   GetColorSpace() const noexcept { return m_colorSpace; }                      // 현재 색상 공간(SDR/HDR)
        unsigned int            GetDeviceOptions() const noexcept { return m_options; }                      // 설정된 옵션 플래그

        // 성능 프로파일링 및 디버깅을 위한 PIX 이벤트 함수들
        // 그래픽 디버거에서 구간을 표시할 때 사용합니다.
        void PIXBeginEvent(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->BeginEvent(name); // 이벤트 구간 시작
        }

        void PIXEndEvent()
        {
            m_d3dAnnotation->EndEvent();       // 이벤트 구간 종료
        }

        void PIXSetMarker(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->SetMarker(name);  // 타임라인에 단일 마커 표시
        }

    private:
        // 내부 헬퍼 함수
        void CreateFactory(); // DXGI 팩토리를 생성합니다.
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter); // 사용 가능한 최적의 그래픽 카드(어댑터)를 찾습니다.

        // Direct3D 핵심 객체들 (ComPtr: 스마트 포인터로 메모리 자동 관리)
        Microsoft::WRL::ComPtr<IDXGIFactory2>               m_dxgiFactory;   // DXGI 객체 생성 팩토리
        Microsoft::WRL::ComPtr<ID3D11Device1>               m_d3dDevice;     // GPU 가상 어댑터 (리소스 생성 담당)
        Microsoft::WRL::ComPtr<ID3D11DeviceContext1>        m_d3dContext;    // 렌더링 파이프라인 제어 (명령 전달 담당)
        Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_swapChain;     // 전면/후면 버퍼 교체 관리자
        Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>   m_d3dAnnotation; // 디버깅용 주석 도구

        // 렌더링에 필요한 리소스들
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;        // 백버퍼 텍스처 자체
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil;        // 깊이 버퍼 텍스처 자체
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView; // 백버퍼를 렌더링 타겟으로 쓰기 위한 뷰
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView; // 깊이 버퍼를 쓰기 위한 뷰
        D3D11_VIEWPORT                                  m_screenViewport;      // 화면의 어느 영역에 그릴지 정의

        // Direct3D 속성값 저장
        DXGI_FORMAT                                     m_backBufferFormat;   // 백버퍼 포맷 저장
        DXGI_FORMAT                                     m_depthBufferFormat;  // 깊이 버퍼 포맷 저장
        UINT                                            m_backBufferCount;    // 백버퍼 개수 저장
        D3D_FEATURE_LEVEL                               m_d3dMinFeatureLevel; // 최소 요구 기능 수준

        // 캐시된 디바이스 속성들 (빈번한 접근을 위해 저장)
        HWND                                            m_window;          // 윈도우 핸들
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel; // 현재 확정된 기능 수준
        RECT                                            m_outputSize;      // 렌더링 영역 크기

        // HDR 지원 관련
        DXGI_COLOR_SPACE_TYPE                           m_colorSpace;      // 색상 공간 정보

        // 옵션 플래그 저장
        unsigned int                                    m_options;

        // IDeviceNotify는 DeviceResources를 소유한 객체(Game)이므로
        // 스마트 포인터가 아닌 일반 포인터(Raw Pointer)로 참조합니다. (순환 참조 방지)
        IDeviceNotify* m_deviceNotify;
    };
}