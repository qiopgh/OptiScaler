#include "pch.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <filesystem>
#include "../external/streamline/sl.h"
#include "../external/streamline/sl_dlss_g.h"

// 在独立进程中检查后端隔离与空闲交换链，不加载游戏、不提交画面。
int wmain(int argc, wchar_t** argv)
{
    if (argc < 3)
        return 2;
    const auto dll = std::filesystem::absolute(argv[1]);
    const bool expectSm86 = wcscmp(argv[2], L"sm86") == 0;
    const bool expectConflict = wcscmp(argv[2], L"conflict") == 0;
    if (expectConflict)
    {
        if (argc != 4 || LoadLibraryW(std::filesystem::absolute(argv[3]).c_str()) == nullptr)
            return 3;
    }
    else if (expectSm86 && argc == 4)
    {
        if (LoadLibraryW(std::filesystem::absolute(argv[3]).c_str()) == nullptr)
            return 3;
    }
    const auto module = LoadLibraryW(dll.c_str());
    std::printf("OPTI_LOADED=%d\n", module != nullptr);
    if (!module)
        return 4;
    using CreateFactory = HRESULT (WINAPI*)(REFIID, void**);
    const auto create = reinterpret_cast<CreateFactory>(GetProcAddress(module, "CreateDXGIFactory1"));
    IDXGIFactory4* factory = nullptr;
    if (create == nullptr || FAILED(create(IID_PPV_ARGS(&factory))))
        return 5;
    ID3D12Device* device = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return 6;
    D3D12_COMMAND_QUEUE_DESC queueDesc {};
    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
        return 7;
    WNDCLASSW wc {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"OptiScalerSm86BackendProbe";
    RegisterClassW(&wc);
    const auto window = CreateWindowW(wc.lpszClassName, L"OptiScaler 独立后端检查", WS_OVERLAPPEDWINDOW,
                                     0, 0, 1280, 720, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window)
        return 8;
    DXGI_SWAP_CHAIN_DESC1 desc {};
    desc.Width = 1280;
    desc.Height = 720;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* chain = nullptr;
    const auto result = factory->CreateSwapChainForHwnd(queue, window, &desc, nullptr, nullptr, &chain);
    std::printf("OPTI_SWAPCHAIN=0x%08lx\n", static_cast<unsigned long>(result));
    const auto proxyPath = dll.parent_path() / L"OptiScaler/dlssg_sm86_proxy/version.dll";
    const bool proxyLoaded = GetModuleHandleW(proxyPath.c_str()) != nullptr;
    wchar_t actualProxy[32768] {};
    if (proxyLoaded)
        GetModuleFileNameW(GetModuleHandleW(proxyPath.c_str()), actualProxy, static_cast<DWORD>(std::size(actualProxy)));
    const auto runtime = GetModuleHandleW(L"nvngx_dlssg.dll");
    wchar_t actual[32768] {};
    if (runtime)
        GetModuleFileNameW(runtime, actual, static_cast<DWORD>(std::size(actual)));
    std::printf("SM86_PROXY_LOADED=%d\nDLSSG_RUNTIME_LOADED=%d\n", proxyLoaded, runtime != nullptr);
    std::printf("SM86_PROXY_PATH=%s\n", std::filesystem::path(actualProxy).string().c_str());
    std::printf("DLSSG_RUNTIME_PATH=%s\n", std::filesystem::path(actual).string().c_str());
    bool correct = SUCCEEDED(result);
    if (expectSm86)
    {
        // 普通交换链可能是初始化失败后的回退结果，必须另外检查活动插件与真实设备能力。
        const auto interposerPath = dll.parent_path() / L"OptiScaler/streamline/sl.interposer.dll";
        const auto interposer = GetModuleHandleW(interposerPath.c_str());
        const auto getFunction = reinterpret_cast<decltype(&slGetFeatureFunction)>(
            interposer ? GetProcAddress(interposer, "slGetFeatureFunction") : nullptr);
        const auto isSupported = reinterpret_cast<decltype(&slIsFeatureSupported)>(
            interposer ? GetProcAddress(interposer, "slIsFeatureSupported") : nullptr);
        using GetState = sl::Result (*)(const sl::ViewportHandle&, sl::DLSSGState&, const sl::DLSSGOptions*);
        void* query = nullptr;
        const auto binding = getFunction ? getFunction(sl::kFeatureDLSS_G, "slDLSSGGetState", query)
                                         : sl::Result::eErrorFeatureMissing;
        sl::DLSSGState backend {};
        const auto stateResult = binding == sl::Result::eOk && query
                                     ? reinterpret_cast<GetState>(query)(sl::ViewportHandle { 0 }, backend, nullptr)
                                     : sl::Result::eErrorFeatureMissing;
        auto luid = device->GetAdapterLuid();
        sl::AdapterInfo adapter {};
        adapter.deviceLUID = reinterpret_cast<uint8_t*>(&luid);
        adapter.deviceLUIDSizeInBytes = sizeof(luid);
        const auto support = isSupported ? isSupported(sl::kFeatureDLSS_G, adapter)
                                         : sl::Result::eErrorFeatureMissing;
        std::printf("DLSSG_BINDING=%d\nDLSSG_STATE_QUERY=%d\nDLSSG_DEVICE_SUPPORT=%d\nDLSSG_MAX_GENERATED=%u\n",
                    static_cast<int>(binding), static_cast<int>(stateResult), static_cast<int>(support),
                    backend.numFramesToGenerateMax);
        correct = correct && proxyLoaded && runtime != nullptr && binding == sl::Result::eOk &&
                  stateResult == sl::Result::eOk && support == sl::Result::eOk && backend.numFramesToGenerateMax > 0;
    }
    else
        correct = correct && !proxyLoaded && ((runtime != nullptr) == expectConflict);
    if (expectConflict)
        correct = correct && _wcsicmp(std::filesystem::weakly_canonical(argv[3]).c_str(), actual) == 0;
    if (chain)
        chain->Release();
    DestroyWindow(window);
    queue->Release();
    device->Release();
    factory->Release();
    std::printf("BACKEND_ISOLATION=%s\nHOST_DONE=1\n", correct ? "PASS" : "FAIL");
    return correct ? 0 : 9;
}
