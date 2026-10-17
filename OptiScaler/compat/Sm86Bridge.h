#pragma once

#include <Windows.h>
#include <filesystem>

namespace Sm86Bridge
{
    // 本进程启动时锁定后端；菜单中的配置变更需重启才能生效。
    bool IsSelected();
    bool Prepare();
    bool IsReady();
    std::filesystem::path Root();
    // 只记录代理实际选择的运行库，不约束其版本、内部布局或配置。
    HMODULE Runtime();
}
