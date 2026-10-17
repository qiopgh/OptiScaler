// 验证真实呈现、启动等待、错误和关闭之间的转换，不需要游戏或显卡。
#include "pch.h"
#include "../OptiScaler/framegen/dlssg/DLSSG_Status.h"
#include <cstdio>
#include <cstdlib>

void Check(bool condition, const char* name)
{
    std::printf("%s=%s\n", name, condition ? "PASS" : "FAIL");
    if (!condition)
        std::exit(1);
}

int main()
{
    using namespace DLSSGStatus;
    Monitor monitor;
    Check(monitor.Read().phase == Phase::Off, "DEFAULT_OFF");
    monitor.Observe(90, true, false, true, 0, 0, 40, 1);
    Check(monitor.Read().phase == Phase::Waiting, "COUNTER_BASELINE_DISCARDS_OLD_PRESENTS");
    monitor.Observe(100, true, false, true, 0, 0, 0, 1);
    Check(monitor.Read().phase == Phase::Waiting, "REQUEST_ALONE_IS_NOT_ACTIVE");
    monitor.Observe(110, true, false, true, 0, 0, 1, 1);
    Check(monitor.Read().phase == Phase::Waiting, "REAL_PRESENT_ALONE_IS_NOT_GENERATION");
    monitor.Observe(120, true, false, true, 0, 0, 2, 1);
    Check(monitor.Read().phase == Phase::Active && monitor.Read().generatedFrames == 1,
          "GENERATED_PRESENT_CONFIRMS_ACTIVE");
    monitor.Observe(200, true, false, true, 0, 0, 0, 1);
    Check(monitor.Read().phase == Phase::Active, "SHORT_ASYNC_GAP_RETAINS_CONFIRMATION");
    monitor.Observe(1200, true, false, true, 0, 0, 1, 1);
    Check(monitor.Read().phase == Phase::Waiting && monitor.Read().generatedFrames == 0,
          "STALE_CONFIRMATION_EXPIRES");
    monitor.Observe(1300, true, false, true, 0, 0, 4, 3);
    Check(monitor.Read().phase == Phase::Active && monitor.Read().generatedFrames == 3,
          "MFG_REQUEST_REPORTED_SEPARATELY");
    monitor.Observe(1310, true, false, true, 0, 2, 4, 3);
    Check(monitor.Read().phase == Phase::BackendError && monitor.Read().code == 2,
          "REFLEX_BACKEND_ERROR_IS_NOT_ON");
    monitor.Observe(1320, true, false, false, 31, 0, 4, 3);
    Check(monitor.Read().phase == Phase::QueryError && monitor.Read().code == 31,
          "QUERY_FAILURE_IS_NOT_OFF_OR_ON");
    monitor.Observe(1330, true, false, true, 0, 0, 1, 1);
    Check(monitor.Read().phase == Phase::Waiting, "ERROR_CLEARS_OLD_CONFIRMATION");
    monitor.Observe(1400, true, false, true, 0, 0, 2, 1);
    monitor.Observe(1410, true, true, true, 0, 0, 2, 1);
    Check(monitor.Read().phase == Phase::Waiting, "PAUSE_CLEARS_ACTIVE");
    monitor.Observe(1415, true, false, true, 0, 0, 40, 1);
    Check(monitor.Read().phase == Phase::Waiting, "RESUME_IGNORES_PAUSED_PRESENT_BACKLOG");
    monitor.Observe(1420, false, false, true, 0, 0, 4, 3);
    Check(monitor.Read().phase == Phase::Off && monitor.Read().generatedFrames == 0,
          "DISABLE_CLEARS_ACTIVE_IMMEDIATELY");
    monitor.Observe(1500, true, false, true, 0, 0, 0, 1);
    Check(monitor.Read().phase == Phase::Waiting, "REENABLE_NEEDS_NEW_EVIDENCE");
    std::printf("DLSSG_STATUS_TEST_DONE=1\n");
}
