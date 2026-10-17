#include "pch.h"
#include "Sm86Bridge.h"
#include <Config.h>
#include <Util.h>
#include <proxies/KernelBase_Proxy.h>
#include <proxies/Ntdll_Proxy.h>

namespace
{
    HMODULE g_proxy = nullptr;
    bool g_attempted = false;
    std::filesystem::path g_root;

    std::filesystem::path ModulePath(HMODULE module)
    {
        if (module == nullptr)
            return {};
        wchar_t path[32768] {};
        const auto length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
        return length != 0 && length < std::size(path) ? std::filesystem::path(path) : std::filesystem::path {};
    }

    bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        return !left.empty() && !right.empty() &&
               _wcsicmp(std::filesystem::weakly_canonical(left).c_str(),
                        std::filesystem::weakly_canonical(right).c_str()) == 0;
    }

    bool RuntimeDoesNotConflict()
    {
        const auto loaded = GetModuleHandleW(L"nvngx_dlssg.dll");
        const auto loadedPath = ModulePath(loaded);
        const auto root = Sm86Bridge::Root().wstring() + L"\\";
        const auto resolved = loadedPath.empty() ? std::wstring {} : std::filesystem::weakly_canonical(loadedPath).wstring();
        if (loaded != nullptr && _wcsnicmp(resolved.c_str(), root.c_str(), root.size()) != 0)
        {
            LOG_ERROR(L"SM75/SM86 后端拒绝混用已加载的另一份 DLSS-G：{}；请退出游戏并选择单一后端。", ModulePath(loaded).wstring());
            return false;
        }
        return true;
    }
}

bool Sm86Bridge::IsSelected()
{
    static const bool selected = Config::Instance()->FGDLSSGSm86Compatibility.value_or_default() &&
                                 State::Instance().activeFgOutput == FGOutput::DLSSG;
    return selected;
}

std::filesystem::path Sm86Bridge::Root()
{
    if (g_root.empty())
    {
        const auto config = Config::Instance();
        const auto base = std::filesystem::path(config->MainDllPath.value());
        g_root = config->FGDLSSGSm86Path.has_value() ? std::filesystem::path(config->FGDLSSGSm86Path.value())
                                                   : std::filesystem::path(L"dlssg_sm86_proxy");
        if (g_root.is_relative())
            g_root = base / g_root;
        g_root = std::filesystem::weakly_canonical(g_root);
    }
    return g_root;
}

bool Sm86Bridge::Prepare()
{
    if (!IsSelected())
        return true;
    if (g_attempted)
        return g_proxy != nullptr;
    g_attempted = true;

    try
    {
        if (State::Instance().activeFgNvngx != FGNvngxReplacement::None)
        {
            LOG_ERROR("SM75/SM86 后端不能同时启用 Nukem、Enabler 或 FFX 的 NGX 帧生成替换。请选择 None 后重启。");
            return false;
        }
        const auto root = Root();
        // 只检查外部入口，不替上游枚举内部文件或解释它的 INI；布局和参数由代理自行管理。
        if (!std::filesystem::is_regular_file(root / L"version.dll"))
        {
            LOG_ERROR(L"SM75/SM86 组件缺失：{}；不改用官方 DLSS-G 或其他替换后端。", (root / L"version.dll").wstring());
            return false;
        }
        if (!RuntimeDoesNotConflict())
            return false;
        const auto existingProxy = GetModuleHandleW(L"version.dll");
        const auto gameProxy = Util::ExePath().parent_path() / L"version.dll";
        if (existingProxy != nullptr && SamePath(ModulePath(existingProxy), gameProxy) &&
            !SamePath(gameProxy, root / L"version.dll"))
        {
            LOG_ERROR("游戏根目录已加载 version.dll 代理；请只保留 OptiScaler 子目录中的 SM75/SM86 代理。");
            return false;
        }
        // 外部代理必须先于本组件的加载钩子与 Streamline 初始化。
        g_proxy = NtdllProxy::LoadLibraryExW_Ldr((root / L"version.dll").c_str(), nullptr, 0);
        if (g_proxy == nullptr)
        {
            LOG_ERROR("SM75/SM86 代理加载失败，Win32={}", GetLastError());
            return false;
        }
        // 架构适配由外部后端负责，避免两套实现同时改写 NVAPI。
        Config::Instance()->StreamlineSpoofing.set_volatile_value(false);
        Config::Instance()->UseFakenvapi.set_volatile_value(false);
        if (!Config::Instance()->FGDLSSGUseGamesReflexMarkers.has_value())
            Config::Instance()->FGDLSSGUseGamesReflexMarkers.set_volatile_value(false);
        LOG_INFO(L"SM75/SM86 DLSS-G 后端已选择，组件目录：{}；实际生成状态仍需后续查询。", root.wstring());
        return true;
    }
    catch (const std::exception& error)
    {
        LOG_ERROR("SM75/SM86 初始化失败：{}", error.what());
        return false;
    }
}

bool Sm86Bridge::IsReady() { return g_proxy != nullptr; }

HMODULE Sm86Bridge::Runtime()
{
    if (!IsSelected())
        return nullptr;
    // Streamline 按普通请求加载，由代理决定 bundled、游戏或其他受支持的运行库策略。
    const auto module = KernelBaseProxy::GetModuleHandleW_()(L"nvngx_dlssg.dll");
    if (module != nullptr)
        LOG_INFO(L"SM75/SM86 代理实际选择的 DLSS-G 运行库：{}", ModulePath(module).wstring());
    return module;
}
