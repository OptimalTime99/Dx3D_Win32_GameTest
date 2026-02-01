//
// Game.h
//

#pragma once // 이 헤더 파일이 컴파일 시 단 한 번만 포함되도록 합니다 (중복 포함 방지).

// 필요한 헤더 파일들을 포함합니다.
#include "DeviceResources.h"    // D3D11 디바이스, 컨텍스트, 스왑 체인 등을 관리하는 클래스입니다.
#include "StepTimer.h"          // 게임 루프의 시간(Delta Time)을 계산하고 관리하는 타이머 클래스입니다.
#include <DirectXTK/SimpleMath.h>   // 벡터(Vector2, Vector3), 행렬 등을 쉽게 쓰기 위한 수학 라이브러리입니다.
#include <DirectXTK/SpriteBatch.h>  // 2D 이미지를 효율적으로 그리기 위한 스프라이트 배칭 클래스입니다.
#include "AnimatedTexture.h"
#include "ScrollingBackground.h"


#include <memory> // std::unique_ptr 같은 스마트 포인터를 사용하기 위한 표준 라이브러리입니다.


// D3D11 디바이스를 생성하고 게임 루프를 제공하는 기본 게임 구현 클래스입니다.
// DX::IDeviceNotify를 상속받아 디바이스 소실/복구 이벤트를 처리합니다.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false); // 생성자입니다. 초기화 실패 시 예외를 던질 수 있습니다.
    ~Game() = default;      // 소멸자입니다. 기본 소멸자를 사용합니다.

    Game(Game&&) = default;              // 이동 생성자를 기본값으로 설정합니다.
    Game& operator= (Game&&) = default;  // 이동 대입 연산자를 기본값으로 설정합니다.

    Game(Game const&) = delete;             // 복사 생성자를 삭제합니다 (게임 인스턴스 복제 방지).
    Game& operator= (Game const&) = delete; // 복사 대입 연산자를 삭제합니다.

    // 초기화 및 관리 함수
    void Initialize(HWND window, int width, int height); // 윈도우 핸들과 크기를 받아 게임을 초기화합니다.

    // 기본 게임 루프
    void Tick(); // 매 프레임 호출되는 함수로, Update와 Render를 실행합니다.

    // IDeviceNotify 인터페이스 구현 (그래픽 카드 드라이버 업데이트나 Alt-Tab 등으로 장치가 끊겼을 때 처리)
    void OnDeviceLost() override;     // 디바이스가 손실되었을 때 리소스를 정리하는 함수입니다.
    void OnDeviceRestored() override; // 디바이스가 복구되었을 때 리소스를 다시 생성하는 함수입니다.

    // 윈도우 메시지 처리 (Win32 API 메시지 펌프에서 호출됨)
    void OnActivated();     // 창이 활성화(포커스 됨)되었을 때 호출됩니다.
    void OnDeactivated();   // 창이 비활성화되었을 때 호출됩니다.
    void OnSuspending();    // 앱이 절전 모드 등으로 중단될 때 호출됩니다.
    void OnResuming();      // 앱이 다시 시작될 때 호출됩니다.
    void OnWindowMoved();   // 창의 위치가 바뀌었을 때 호출됩니다.
    void OnDisplayChange(); // 디스플레이 해상도나 설정이 바뀌었을 때 호출됩니다.
    void OnWindowSizeChanged(int width, int height); // 창의 크기가 변경되었을 때 호출됩니다.

    // 속성 (Getter)
    void GetDefaultSize(int& width, int& height) const noexcept; // 게임의 기본 해상도 크기를 반환합니다.

    // [추가] 외부에서 활성화 상태를 확인할 수 있는 함수 (Getter)
    bool IsActive() const { return m_isActive; }

private:

    void Update(DX::StepTimer const& timer); // 게임 로직(이동, 입력 처리 등)을 수행합니다.
    void Render();                           // 게임 화면(그래픽)을 그립니다.

    void Clear(); // 화면(백버퍼)을 깨끗하게 지웁니다. (보통 프레임 시작 시 호출)

    void CreateDeviceDependentResources();       // 디바이스가 생성될 때 필요한 리소스(텍스처, 쉐이더 등)를 만듭니다.
    void CreateWindowSizeDependentResources();   // 창 크기에 의존적인 리소스(후면 버퍼, 뷰포트 등)를 설정합니다.

    // 디바이스 리소스 관리자
    std::unique_ptr<DX::DeviceResources>    m_deviceResources; // D3D11 디바이스 관리를 전담하는 객체입니다.

    // 렌더링 루프 타이머
    DX::StepTimer                           m_timer; // 프레임 간의 시간 간격을 측정하는 타이머입니다.

    // [추가] 게임이 활성화(Focus) 상태인지 저장하는 변수
    // 초기값은 true로 설정
    bool m_isActive = true;

    // 스프라이트 애니메이션 변수
    std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
    std::unique_ptr<AnimatedTexture> m_ship;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;
    DirectX::SimpleMath::Vector2 m_shipPos;

    // 스크롤링 배경 변수
    std::unique_ptr<ScrollingBackground> m_stars;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_backgroundTex;
};