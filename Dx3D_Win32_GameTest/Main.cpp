//
// Main.cpp
//

#include "pch.h"      // 미리 컴파일된 헤더 (컴파일 속도 향상)
#include "Game.h"     // Game 클래스 헤더 포함

using namespace DirectX;

// Clang 컴파일러 사용 시 특정 경고를 무시하도록 설정 (MSVC에서는 무시됨)
#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

// 4061 경고(열거형 switch문에서 일부 케이스 누락 경고)를 끕니다.
#pragma warning(disable : 4061)

namespace
{
    // 게임 인스턴스를 관리하는 전역 스마트 포인터입니다.
    // std::unique_ptr을 사용하여 메모리 관리를 자동화합니다.
    std::unique_ptr<Game> g_game;
}

// 윈도우 창의 제목 표시줄에 나타날 이름입니다.
LPCWSTR g_szAppName = L"Dx3D_Win32_GameTest";

// 윈도우 프로시저(메시지 처리 함수)의 전방 선언입니다.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ExitGame() noexcept;

// 노트북 등에서 내장 그래픽 대신 고성능 외장 그래픽(NVIDIA/AMD)을 
// 우선 사용하도록 드라이버에 힌트를 주는 코드입니다.
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; // NVIDIA Optimus 활성화
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // AMD PowerXpress 활성화
}

// 윈도우 프로그램의 진입점(Entry Point)입니다. (콘솔의 main 함수 역할)
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // 사용하지 않는 매개변수 경고 방지
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // CPU가 DirectX Math 라이브러리에 필요한 명령어(SSE2 등)를 지원하는지 확인합니다.
    if (!XMVerifyCPUSupport())
        return 1;

    // COM(Component Object Model) 라이브러리를 초기화합니다. (DirectX, WIC 사용에 필요)
    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr))
        return 1;

    // Game 클래스의 인스턴스를 생성합니다. (Game 생성자 호출)
    g_game = std::make_unique<Game>();

    // 윈도우 클래스 등록 및 창 생성 블록
    {
        // 1. 윈도우 클래스(창의 속성) 정의
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW; // 창 크기 변경 시 다시 그리기
        wcex.lpfnWndProc = WndProc;           // 메시지 처리 함수 지정 (중요!)
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON"); // 아이콘 설정
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW); // 커서 설정
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); // 배경색
        wcex.lpszClassName = L"Dx3D_Win32_GameTestWindowClass"; // 클래스 식별 이름
        wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");

        // 정의한 클래스를 운영체제에 등록합니다. 실패 시 종료.
        if (!RegisterClassExW(&wcex))
            return 1;

        // 2. 윈도우 창 생성
        int w, h;
        g_game->GetDefaultSize(w, h); // Game 클래스에서 원하는 해상도(800x600) 가져오기

        RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };

        // 윈도우 스타일(타이틀바, 테두리 등)을 고려해 실제 창 크기를 계산합니다.
        // (클라이언트 영역이 정확히 800x600이 되도록 창 전체 크기를 키움)
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        // 실제 창을 생성합니다.
        HWND hwnd = CreateWindowExW(0, L"Dx3D_Win32_GameTestWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance,
            g_game.get()); // 생성 시 g_game 포인터를 전달 (나중에 꺼내 쓰기 위해)

        if (!hwnd)
            return 1;

        // 창을 화면에 보여줍니다.
        ShowWindow(hwnd, nCmdShow);

        // 현재 창의 클라이언트 영역(그림 그려질 부분) 크기를 다시 잽니다.
        GetClientRect(hwnd, &rc);

        // Game 객체를 초기화합니다. (DirectX 장치 생성 등)
        g_game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top);
    }

    // ------------------------------------------------------------
    // [핵심] Main Message Loop (메인 메시지 루프)
    // ------------------------------------------------------------
    MSG msg = {};
    while (WM_QUIT != msg.message) // WM_QUIT 메시지가 올 때까지 무한 반복
    {
        // 큐에 메시지가 있는지 확인합니다 (PeekMessage).
        // PM_REMOVE: 메시지가 있으면 확인 후 큐에서 제거합니다.
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // 메시지가 있으면 처리합니다.
            TranslateMessage(&msg); // 키보드 입력 번역
            DispatchMessage(&msg);  // WndProc 함수로 메시지 배달
        }
        else
        {
            // 게임이 활성화 상태일 때만 맹렬히 돌린다
            if (g_game->IsActive())
            {
                // 처리할 메시지가 없으면(Idle 상태), 게임을 한 프레임 실행합니다.
                // 여기가 게임이 멈추지 않고 계속 돌아가게 하는 핵심입니다.
                g_game->Tick();
            }
            else
            {
                // 게임이 최소화되었거나 뒤로 갔으면, 일반 앱처럼 
                // 메시지가 올 때까지 재워서 CPU를 아낀다.
                WaitMessage();
            }

        }
    }

    // 루프를 빠져나오면 게임 객체를 정리합니다.
    g_game.reset();

    // COM 라이브러리 사용 종료
    CoUninitialize();

    return static_cast<int>(msg.wParam);
}

// 윈도우 프로시저: OS에서 보내온 메시지를 처리하는 함수
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // 정적 변수로 상태들을 기억합니다.
    static bool s_in_sizemove = false; // 창 드래그/리사이징 중인지 여부
    static bool s_in_suspend = false;  // 일시정지 상태인지 여부
    static bool s_minimized = false;   // 최소화 상태인지 여부
    static bool s_fullscreen = false;  // 전체화면 상태인지 여부

    // 창 생성 시 저장해둔 Game 객체 포인터를 꺼내옵니다.
    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE: // 창이 처음 생성될 때
        if (lParam)
        {
            // CreateWindowExW에서 넘긴 g_game 포인터를 받아서 창의 유저 데이터에 저장해둡니다.
            auto params = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
        }
        break;

    case WM_PAINT: // 화면을 다시 그려야 할 때 (창이 가려졌다 드러날 때 등)
        if (s_in_sizemove && game)
        {
            // 리사이징 중에도 화면을 갱신해서 부드럽게 보이게 합니다.
            game->Tick();
        }
        else
        {
            // 일반적인 경우, GDI 페인팅 처리를 마무리합니다.
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DISPLAYCHANGE: // 모니터 해상도 변경 시
        if (game) game->OnDisplayChange();
        break;

    case WM_MOVE: // 창 위치 이동 시
        if (game) game->OnWindowMoved();
        break;

    case WM_SIZE: // 창 크기 변경 시
        if (wParam == SIZE_MINIMIZED) // 최소화 버튼 클릭 시
        {
            if (!s_minimized)
            {
                s_minimized = true;
                if (!s_in_suspend && game) game->OnSuspending(); // 게임 일시정지
                s_in_suspend = true;
            }
        }
        else if (s_minimized) // 최소화에서 복귀 시
        {
            s_minimized = false;
            if (s_in_suspend && game) game->OnResuming(); // 게임 재개
            s_in_suspend = false;
        }
        else if (!s_in_sizemove && game) // 일반적인 크기 변경
        {
            // Game 객체에 변경된 크기를 알립니다.
            game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_ENTERSIZEMOVE: // 창 테두리를 잡고 드래그 시작 시
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE: // 드래그 종료 시
        s_in_sizemove = false;
        if (game)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            game->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;

    case WM_GETMINMAXINFO: // 창의 최소/최대 크기 제한
        if (lParam)
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320; // 가로 최소 320
            info->ptMinTrackSize.y = 200; // 세로 최소 200
        }
        break;

    case WM_ACTIVATEAPP: // 앱이 포커스를 받거나 잃을 때
        if (game)
        {
            if (wParam) game->OnActivated();
            else game->OnDeactivated();
        }
        break;

    case WM_POWERBROADCAST: // 전원 상태 변경 (절전 모드 등)
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (!s_in_suspend && game) game->OnSuspending();
            s_in_suspend = true;
            return TRUE;
        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                if (s_in_suspend && game) game->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;
        default: break;
        }
        break;

    case WM_DESTROY: // 창이 닫힐 때
        PostQuitMessage(0); // 메시지 루프를 종료하도록 WM_QUIT을 보냅니다.
        break;

    case WM_SYSKEYDOWN: // Alt + Enter 키 처리 (전체화면 전환)
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
        {
            // 전체화면 <-> 창모드 토글 로직
            if (s_fullscreen)
            {
                // 창 모드로 복귀
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);
                int width = 800; int height = 600;
                if (game) game->GetDefaultSize(width, height);
                ShowWindow(hWnd, SW_SHOWNORMAL);
                SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            else
            {
                // 전체화면으로 전환 (테두리 없는 팝업창으로 변경 후 최대화)
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);
                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }
            s_fullscreen = !s_fullscreen;
        }
        break;

    case WM_MENUCHAR: // 메뉴 단축키 소리 끄기
        return MAKELRESULT(0, MNC_CLOSE);

    default:
        break;
    }

    // 처리하지 않은 나머지 메시지는 기본 윈도우 처리 함수에 맡깁니다.
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// 게임 종료 함수
void ExitGame() noexcept
{
    PostQuitMessage(0);
}