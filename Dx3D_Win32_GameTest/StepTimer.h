//
// StepTimer.h - 경과 시간 정보를 제공하는 간단한 타이머 클래스
//

#pragma once // 컴파일 시 헤더 중복 포함 방지

#include <cmath>    // 수학 함수 (abs 등)
#include <cstdint>  // 정수형 타입 (uint64_t 등)
#include <exception>// 예외 처리

namespace DX
{
    // 애니메이션 및 시뮬레이션 타이밍을 위한 헬퍼 클래스
    class StepTimer
    {
    public:
        // 생성자: 멤버 변수들을 초기화합니다.
        StepTimer() noexcept(false) :
            m_elapsedTicks(0),          // 지난 프레임 이후 경과된 시간 (Tick 단위)
            m_totalTicks(0),            // 프로그램 시작 후 총 경과 시간
            m_leftOverTicks(0),         // 고정 시간 모드에서 처리가 남은 자투리 시간
            m_frameCount(0),            // 총 프레임 수
            m_framesPerSecond(0),       // 현재 FPS (초당 프레임)
            m_framesThisSecond(0),      // 이번 1초 동안 계산된 프레임 수
            m_qpcSecondCounter(0),      // 1초를 세기 위한 QPC 카운터
            m_isFixedTimeStep(false),   // 고정 시간 모드 사용 여부 (기본값: false = 가변 시간)
            m_targetElapsedTicks(TicksPerSecond / 60) // 고정 모드 목표 시간 (기본값: 1/60초 = 60FPS)
        {
            // 고해상도 타이머의 주파수(초당 진동수)를 가져옵니다. 실패 시 예외 발생.
            if (!QueryPerformanceFrequency(&m_qpcFrequency))
            {
                throw std::exception();
            }

            // 현재의 고해상도 타이머 값(현재 시간)을 가져옵니다.
            if (!QueryPerformanceCounter(&m_qpcLastTime))
            {
                throw std::exception();
            }

            // 최대 델타 타임을 1/10초로 설정합니다.
            // 디버깅 등으로 인해 게임이 멈췄다가 풀렸을 때, 갑자기 엄청난 시간이 흘러
            // 물리 엔진 등이 망가지는 것을 방지하는 상한선(Clamp)입니다.
            m_qpcMaxDelta = static_cast<uint64_t>(m_qpcFrequency.QuadPart / 10);
        }

        // 이전 Update 호출 이후 경과된 시간(Delta Time)을 반환합니다.
        uint64_t GetElapsedTicks() const noexcept { return m_elapsedTicks; }
        double GetElapsedSeconds() const noexcept { return TicksToSeconds(m_elapsedTicks); }

        // 프로그램 시작 이후 총 경과 시간을 반환합니다.
        uint64_t GetTotalTicks() const noexcept { return m_totalTicks; }
        double GetTotalSeconds() const noexcept { return TicksToSeconds(m_totalTicks); }

        // 프로그램 시작 이후 총 업데이트 횟수를 반환합니다.
        uint32_t GetFrameCount() const noexcept { return m_frameCount; }

        // 현재 프레임 레이트(FPS)를 반환합니다.
        uint32_t GetFramesPerSecond() const noexcept { return m_framesPerSecond; }

        // 고정 시간(Fixed) 모드와 가변 시간(Variable) 모드를 설정합니다.
        // true: 물리 시뮬레이션 등에 유리 (일정한 간격 업데이트)
        // false: 부드러운 화면 렌더링에 유리 (최대 성능)
        void SetFixedTimeStep(bool isFixedTimestep) noexcept { m_isFixedTimeStep = isFixedTimestep; }

        // 고정 시간 모드일 때 목표로 할 업데이트 간격을 설정합니다. (기본값: 60FPS)
        void SetTargetElapsedTicks(uint64_t targetElapsed) noexcept { m_targetElapsedTicks = targetElapsed; }
        void SetTargetElapsedSeconds(double targetElapsed) noexcept { m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

        // 내부적으로 시간을 표현할 때 사용하는 단위입니다. (초당 10,000,000 틱)
        // 1 틱 = 100 나노초 (Windows API 표준과 유사)
        static constexpr uint64_t TicksPerSecond = 10000000;

        // 틱(Ticks) 단위를 초(Seconds) 단위로 변환하는 정적 헬퍼 함수
        static constexpr double TicksToSeconds(uint64_t ticks) noexcept { return static_cast<double>(ticks) / TicksPerSecond; }
        // 초(Seconds) 단위를 틱(Ticks) 단위로 변환하는 정적 헬퍼 함수
        static constexpr uint64_t SecondsToTicks(double seconds) noexcept { return static_cast<uint64_t>(seconds * TicksPerSecond); }

        // 의도적인 시간 불연속(예: 로딩 화면, 긴 파일 입출력)이 발생한 후 호출합니다.
        // 고정 시간 모드에서 로딩 시간 동안 밀린 업데이트를 한꺼번에 처리하려고 시도(Catch-up)하는 것을 막아줍니다.
        void ResetElapsedTime()
        {
            if (!QueryPerformanceCounter(&m_qpcLastTime))
            {
                throw std::exception();
            }

            m_leftOverTicks = 0;
            m_framesPerSecond = 0;
            m_framesThisSecond = 0;
            m_qpcSecondCounter = 0;
        }

        // [핵심] 타이머 상태를 갱신하고, 설정에 따라 Update 함수를 적절한 횟수만큼 호출합니다.
        // 템플릿을 사용하여 Update 함수(람다 등)를 인자로 받습니다.
        template<typename TUpdate>
        void Tick(const TUpdate& update)
        {
            // 현재 시간을 측정합니다.
            LARGE_INTEGER currentTime;

            if (!QueryPerformanceCounter(&currentTime))
            {
                throw std::exception();
            }

            // 지난번 측정 시간과의 차이(Delta Time)를 구합니다 (QPC 단위).
            uint64_t timeDelta = static_cast<uint64_t>(currentTime.QuadPart - m_qpcLastTime.QuadPart);

            m_qpcLastTime = currentTime;        // 현재 시간을 마지막 시간으로 갱신
            m_qpcSecondCounter += timeDelta;    // FPS 계산을 위해 시간 누적

            // 델타 타임이 너무 크면(예: 디버거 중단) 최대치(1/10초)로 자릅니다.
            if (timeDelta > m_qpcMaxDelta)
            {
                timeDelta = m_qpcMaxDelta;
            }

            // QPC 단위를 표준 틱(TicksPerSecond) 단위로 변환합니다.
            // 위에서 Clamp를 했기 때문에 오버플로우 위험은 없습니다.
            timeDelta *= TicksPerSecond;
            timeDelta /= static_cast<uint64_t>(m_qpcFrequency.QuadPart);

            const uint32_t lastFrameCount = m_frameCount;

            // ===== 고정 시간 간격 (Fixed Time Step) 로직 =====
            if (m_isFixedTimeStep)
            {
                // 목표 시간(예: 60FPS)과 실제 시간의 차이가 매우 미세하면(1/4000초 미만),
                // 그냥 목표 시간과 똑같은 것으로 간주합니다.
                // 이는 59.94Hz 모니터 등에서 발생하는 미세한 오차가 누적되어 
                // 프레임이 튀는 것을 방지합니다.
                if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))) < TicksPerSecond / 4000)
                {
                    timeDelta = m_targetElapsedTicks;
                }

                m_leftOverTicks += timeDelta; // 자투리 시간을 누적합니다.

                // 누적된 시간이 목표 시간(1 프레임 시간)보다 크거나 같으면 Update를 실행합니다.
                // 컴퓨터가 느려서 시간이 많이 지났다면, while 루프를 통해 
                // 여러 번 Update를 실행하여 게임 논리 시간을 따라잡습니다 (Catch-up Logic).
                while (m_leftOverTicks >= m_targetElapsedTicks)
                {
                    m_elapsedTicks = m_targetElapsedTicks;
                    m_totalTicks += m_targetElapsedTicks;
                    m_leftOverTicks -= m_targetElapsedTicks;
                    m_frameCount++;

                    update(); // 실제 게임 로직(Update) 호출
                }
            }
            // ===== 가변 시간 간격 (Variable Time Step) 로직 =====
            else
            {
                // 단순하게 지난 시간을 그대로 적용합니다.
                m_elapsedTicks = timeDelta;
                m_totalTicks += timeDelta;
                m_leftOverTicks = 0;
                m_frameCount++;

                update(); // 실제 게임 로직(Update) 호출
            }

            // ===== FPS 계산 로직 =====
            // 이번 Tick에서 프레임이 진행되었다면 카운트 증가
            if (m_frameCount != lastFrameCount)
            {
                m_framesThisSecond++;
            }

            // 1초가 지났는지 확인 (QPC 단위 비교)
            if (m_qpcSecondCounter >= static_cast<uint64_t>(m_qpcFrequency.QuadPart))
            {
                m_framesPerSecond = m_framesThisSecond; // 현재 FPS 저장
                m_framesThisSecond = 0;                 // 카운터 초기화
                m_qpcSecondCounter %= static_cast<uint64_t>(m_qpcFrequency.QuadPart); // 1초를 뺀 나머지만 남김
            }
        }

    private:
        // QPC(QueryPerformanceCounter) 단위의 원본 타이밍 데이터
        LARGE_INTEGER m_qpcFrequency; // 타이머 주파수
        LARGE_INTEGER m_qpcLastTime;  // 마지막 측정 시간
        uint64_t m_qpcMaxDelta;       // 델타 타임 제한값

        // 표준 Ticks 단위로 변환된 타이밍 데이터
        uint64_t m_elapsedTicks;  // 지난 프레임 시간
        uint64_t m_totalTicks;    // 총 시간
        uint64_t m_leftOverTicks; // 고정 스텝용 누적 시간

        // FPS 측정을 위한 변수들
        uint32_t m_frameCount;       // 총 프레임 수
        uint32_t m_framesPerSecond;  // FPS
        uint32_t m_framesThisSecond; // 1초 동안 센 프레임 수
        uint64_t m_qpcSecondCounter; // 1초 경과 측정용 누적 시간

        // 고정 시간 스텝 설정 변수
        bool m_isFixedTimeStep;        // 고정 스텝 사용 여부
        uint64_t m_targetElapsedTicks; // 목표 시간 간격 (예: 1/60초)
    };
}