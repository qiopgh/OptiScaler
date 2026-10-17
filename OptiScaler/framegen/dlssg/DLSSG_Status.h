#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace DLSSGStatus
{
    enum class Phase : uint8_t { Off, Waiting, Active, QueryError, BackendError };

    struct Snapshot
    {
        Phase phase = Phase::Off;
        uint8_t generatedFrames = 0;
        uint8_t presentedFrames = 0;
        uint32_t code = 0;
    };

    // 呈现线程更新，菜单只读取一个原子快照；启动请求不能作为实际补帧证据。
    class Monitor
    {
        std::atomic<uint64_t> _snapshot { 0 };
        std::atomic<uint64_t> _lastConfirmed { 0 };
        std::atomic<bool> _needsBaseline { true };

        void Publish(Snapshot value)
        {
            const auto packed = static_cast<uint64_t>(value.phase) |
                                (static_cast<uint64_t>(value.generatedFrames) << 8) |
                                (static_cast<uint64_t>(value.presentedFrames) << 16) |
                                (static_cast<uint64_t>(value.code) << 24);
            _snapshot.store(packed, std::memory_order_release);
        }

      public:
        Snapshot Read() const
        {
            const auto packed = _snapshot.load(std::memory_order_acquire);
            return { static_cast<Phase>(packed & 0xff), static_cast<uint8_t>(packed >> 8),
                     static_cast<uint8_t>(packed >> 16), static_cast<uint32_t>(packed >> 24) };
        }

        void Reset(bool requested = false)
        {
            _lastConfirmed.store(0, std::memory_order_relaxed);
            _needsBaseline.store(true, std::memory_order_relaxed);
            Publish({ requested ? Phase::Waiting : Phase::Off });
        }

        void Observe(uint64_t now, bool requested, bool paused, bool queryOk, uint32_t queryCode,
                     uint32_t backendCode, uint32_t presented, uint32_t generated)
        {
            if (!requested || paused)
            {
                Reset(requested);
                return;
            }
            if (!queryOk || backendCode != 0)
            {
                _lastConfirmed.store(0, std::memory_order_relaxed);
                _needsBaseline.store(true, std::memory_order_relaxed);
                Publish({ queryOk ? Phase::BackendError : Phase::QueryError, 0, 0,
                          queryOk ? backendCode : queryCode });
                return;
            }
            // 首次查询以及暂停、错误恢复后的累计计数可能包含多帧旧真实画面，先消耗基线。
            const bool hasBaseline = !_needsBaseline.exchange(false, std::memory_order_relaxed);
            if (hasBaseline && presented > 1 && generated > 0)
                _lastConfirmed.store(now, std::memory_order_relaxed);
            const auto confirmed = _lastConfirmed.load(std::memory_order_relaxed);
            // 计数自上次查询起累计；短暂的异步零样本保留一秒，错误、暂停和关闭立即清除。
            const bool active = confirmed != 0 && now >= confirmed && now - confirmed <= 1000;
            Publish({ active ? Phase::Active : Phase::Waiting,
                      static_cast<uint8_t>(active ? std::min(generated, 255u) : 0),
                      static_cast<uint8_t>(std::min(presented, 255u)), 0 });
        }
    };
}
