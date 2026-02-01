//
// pch.h
// 표준 시스템 포함 파일을 위한 헤더입니다.
//

#pragma once // 이 파일이 컴파일 시 단 한 번만 포함되도록 합니다 (중복 포함 방지).

#include <winsdkver.h> // Windows SDK 버전을 설정하기 위한 매크로들을 포함합니다.

// 타겟 윈도우 버전을 정의합니다.
// 0x0603은 Windows 8.1을 의미합니다. 
// DirectX 11.1 기능을 사용하기 위한 최소 요구 버전입니다.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0603 
#endif

#include <sdkddkver.h> // 위에서 설정한 버전에 맞춰 실제 SDK 버전을 정의합니다.

// C++ 표준 템플릿의 min/max 함수를 사용하겠다는 선언입니다.
// Windows.h에는 min, max 매크로가 이미 있는데, 이것이 std::min, std::max와 충돌하는 것을 막아줍니다.
#define NOMINMAX

// DirectX 앱은 오래된 GDI(Graphics Device Interface) 기능이 필요 없으므로 제외하여 빌드 속도를 높입니다.
#define NODRAWTEXT // DrawText() 함수 제외
#define NOGDI      // 기본적인 GDI 함수 제외
#define NOBITMAP   // 비트맵 처리 함수 제외

// 모뎀 제어 확장 기능 제외 (필요하면 주석 해제)
#define NOMCX

// 윈도우 서비스 관련 기능 제외 (필요하면 주석 해제)
#define NOSERVICE

// WinHelp(도움말) 기능은 더 이상 사용되지 않으므로 제외
#define NOHELP

// Windows.h에서 잘 쓰지 않는 방대한 API들을 제외하여 컴파일 속도를 획기적으로 높입니다.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // 윈도우 프로그래밍의 핵심 헤더 (Win32 API)

// COM(Component Object Model) 스마트 포인터인 Microsoft::WRL::ComPtr을 사용하기 위함입니다.
// DirectX 객체들의 메모리 관리를 자동으로 해줍니다.
#include <wrl/client.h>

#include <d3d11_1.h> // Direct3D 11.1 인터페이스 헤더
#include <dxgi1_6.h> // DXGI (DirectX Graphics Infrastructure) 1.6 헤더 (어댑터, 모니터, 스왑체인 관리)

#include <DirectXMath.h>   // DirectX용 고성능 수학 라이브러리 (벡터, 행렬 연산)
#include <DirectXColors.h> // 자주 쓰는 색상값(Colors::Red 등) 정의

// C++ 표준 라이브러리들 (STL)
#include <algorithm>    // 알고리즘 (std::sort 등)
#include <cmath>        // 수학 함수 (sin, cos 등)
#include <cstdint>      // 고정 크기 정수형 (int32_t, uint64_t 등)
#include <cstdio>       // C 스타일 입출력 (printf 등)
#include <cwchar>       // 와이드 문자 처리
#include <exception>    // 예외 처리
#include <iterator>     // 반복자
#include <memory>       // 스마트 포인터 (std::unique_ptr, std::shared_ptr)
#include <stdexcept>    // 표준 예외 클래스
#include <system_error> // 시스템 에러 처리
#include <tuple>        // 튜플 (여러 값을 묶음)

// 디버그 빌드(_DEBUG)일 때만 포함합니다.
#ifdef _DEBUG
#include <dxgidebug.h> // DXGI 디버그 인터페이스 (메모리 누수 보고 등)
#endif

// 편의를 위한 DX 네임스페이스 (에러 처리 헬퍼)
namespace DX
{
    // COM 에러(HRESULT)를 C++ 예외(Exception)로 변환해주는 헬퍼 클래스입니다.
    // std::exception을 상속받아 표준 예외 처리 방식과 호환됩니다.
    class com_exception : public std::exception
    {
    public:
        // 생성자: 실패한 HRESULT 코드를 받아서 저장합니다.
        com_exception(HRESULT hr) noexcept : result(hr) {}

        // 예외 내용을 문자열로 반환하는 함수 (std::exception 오버라이드)
        const char* what() const noexcept override
        {
            static char s_str[64] = {};
            // 에러 코드를 16진수 문자열로 포맷팅하여 반환합니다.
            sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
            return s_str;
        }

    private:
        HRESULT result; // 발생한 에러 코드 저장
    };

    // DirectX 함수 실행 결과를 검사하는 헬퍼 함수입니다.
    // HRESULT가 실패(FAILED)라면 즉시 예외(com_exception)를 던집니다.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }
}