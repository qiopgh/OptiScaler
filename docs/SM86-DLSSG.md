# SM75/SM86 DLSS-G 后端

此功能让 OptiScaler 通过 [dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86) 在该项目支持的显卡上使用 DLSS-G。当前实测基线为 0.4.0，只支持原生 D3D12。兼容层不锁上游版本、哈希、内部目录布局或模型参数，支持与否由实际 Streamline 初始化与真实设备能力查询决定。

默认使用 OptiScaler 原有的官方 DLSS-G 路径。特殊后端需要显式启用，两个后端在一次进程运行中只能选择一个。切换后端需要完全退出并重启游戏。

## 安装组件

先按 OptiScaler 原有方式安装，并保留 `OptiScaler/streamline` 中的 Streamline 组件。将外部项目的完整发布包放入独立子目录，保持该版本自己的布局。下面以已实测的 0.4.0 为例，列出的内部文件不是 OptiScaler 的强制要求：

```text
游戏渲染程序所在目录/
  dxgi.dll                         ← OptiScaler，文件名按游戏的正常安装要求选择
  OptiScaler.ini
  OptiScaler/
    streamline/                    ← OptiScaler 使用的 Streamline 插件
    dlssg_sm86_proxy/
      version.dll                  ← 外部项目的代理
      dlssg_sm86.ini               ← 外部项目的配置
      dlssg_sm86/
        backend/sm86_backend.dll
        kernels/fg_kernels.dll
        runtime/nvngx_dlssg.dll
```

不要把这份代理同时放到游戏根目录，也不要覆盖游戏或官方后端原有的 `nvngx_dlssg.dll`。本仓库不包含外部项目的运行库与内核；应从其正式发布包获取，并遵循各组件的许可。

## 启用

没有原生帧生成控制的游戏，可使用现有的 OptiFG 超分输入路径：

```ini
[FrameGen]
Enabled=true
FGInput=Upscaler
FGOutput=DLSSG
FGNvngxReplacement=None

[DLSSG]
Sm86Compatibility=true
Sm86Path=auto
UseGamesReflexMarkers=false
InterpolationCount=1
```

`FGInput=Upscaler` 需要 OptiScaler 已经捕获到受支持的超分输入、深度与运动矢量。仅安装 DLL 不能为没有这些输入的游戏凭空提供帧生成。关闭游戏自己的帧生成输出，避免同时运行两条呈现链。有原生帧生成输入的游戏也可使用 OptiScaler 原有的输入方式，但仍应只保留一个输出后端。

菜单的帧生成选择卡片中提供 `DLSS-G SM75/SM86 (restart required)` 开关。它只保存后端选择，不在运行中卸载或热切换代理。保存设置、选择 DLSSG 输出和 `None` 的 NGX 替换后，完全退出并重启游戏。

`Sm86Path=auto` 使用上述默认目录。自定义相对路径以 `[Libraries] OptiDllPath` 为基准，绝对路径也可使用。外部项目的 INI 路径规则仍以其代理目录为准。

OptiScaler 只要求所选目录中有用于加载的 `version.dll` 入口。代理自己的配置、后端、内核、运行库和默认值均由上游管理；不读取、生成、迁移或覆盖它的 INI。升级时退出游戏，按上游说明更新该子目录中的整套文件，并保留或迁移你自己的设置即可。只要代理的公开加载方式及 Streamline 接口保持兼容，无需因为版本号、文件哈希或内部布局变化重新编译 OptiScaler。

`InterpolationCount=1` 表示每真实帧请求一帧，即 2X；它与 DLSS 超分比例无关。实际允许的最大生成帧数由外部运行库和 Streamline 能力查询决定。

外部项目的模型预设在自己的 `dlssg_sm86.ini` 中设置。例如使用其 FG Lite C 预设：

```ini
[DLSSG]
Preset=C
Optimized=2
```

该配置只是当前上游版本的可选示例，属于外部后端，不是 OptiScaler 的 DLSS 超分预设，也不是兼容要求。OptiScaler 不强制 C 或任何其他模型，不设置 `Optimized`、`MaxGeneratedFrames`、`Runtime.Mode` 或超分选项；更新后以对应版本的上游说明为准。

## 六倍及其他倍率

菜单按活动 Streamline 插件返回的 `numFramesToGenerateMax` 生成倍率选项，没有固定的四倍菜单上限。这个值表示额外生成帧数，显示倍率为该值加一。

例如当前上游版本配置 `MaxGeneratedFrames=3` 时，最多三个生成帧，对应 4X。若上游组件及活动插件支持五个生成帧，可由用户在上游 INI 中设置 `MaxGeneratedFrames=5`，完全退出并重启游戏后，菜单应显示到 6X。OptiScaler 不会代改这一配置，也不会把插件实际不支持的档位强行加进菜单。

OptiScaler 自身的 `InterpolationCount` 表示当前请求的生成帧数，`5` 对应请求 6X；可直接在菜单中选择。开启 `Force Dynamic MFG` 后，运行库按目标帧率动态选择倍率，允许 6X 不代表始终使用 6X；需要手动固定倍率时关闭该选项，再选择对应档位。

## 与官方后端隔离

- `Sm86Compatibility=false` 时不加载外部代理，也不修改原有 NVAPI 选项或运行库搜索路径；子目录中的文件保持未加载状态。
- 启用特殊后端时加载所选代理，运行库位置与策略由代理自己的配置决定，OptiScaler 只记录实际结果。
- 已加载另一位置的 DLSS-G 运行库、根目录代理或选用了 Nukem、Enabler、FFX NGX 替换时，拒绝组合使用并记录原因。
- 缺文件、初始化失败、设备能力查询失败或使用 Vulkan 等不支持的 API 时，不会悄悄改成官方 DLSS-G、其他后端或伪造能力查询成功。
- 回到官方路径时，将 `Sm86Compatibility=false`，完全退出并重启游戏。无需替换官方运行库。

特殊后端未显式配置 `UseGamesReflexMarkers` 时，采用 OptiScaler 自己的 Reflex 标记；显式配置仍被尊重。官方后端原有默认值不变。

特殊模式初始化 Streamline 时，保留原生 NGX 与 NVAPI 的加载请求，使外部代理能完成后端安装；否则 NGX 请求可能被截成 OptiScaler 自身，导致插件判断硬件不受支持。兼容层不预加载代理内部的 bundled DLL，Streamline 的普通运行库搜索目录保持原样，由代理自行完成重定向和运行库选择。

## 状态显示

D3D12 DLSS-G 输出在呈现线程通过活动 Streamline 插件的 `slDLSSGGetState` 更新状态，菜单读取原子快照，不再依赖 NGX Evaluate 钩子是否被调用。

| 显示 | 含义 |
|---|---|
| `OFF` | 输出已关闭或上下文已退出 |
| `WAITING FOR GENERATED PRESENTS` | 已请求开启，尚未观察到额外呈现帧，或者输出正在暂停 |
| `ON (requested 2x)` | 后端近期报告了额外呈现帧；括号内是请求倍率 |
| `ERROR (Streamline 0x...)` | 状态查询或选项提交失败 |
| `ERROR (DLSSG 0x...)` | 后端报告错误，例如 Reflex 未生效或输入常量不合法 |

`numFramesActuallyPresented` 是自上次查询以来的呈现计数，不是 FPS。首次查询及暂停、错误恢复后的第一份累计计数只用于清空旧基线，避免把暂停期间积累的真实帧当成补帧。短暂的异步零样本最多保留一秒确认状态；错误、关闭和暂停会清除确认。请求倍率不应被当作实测 FPS 倍率。

## 验证

独立状态测试覆盖初始化不冒充运行、真实帧不冒充生成帧、额外呈现确认、异步间隔、超时、错误、暂停、关闭和重新开启。使用 VS2022 C++ 工具链构建 `tests/DlssgStatusTests.vcxproj`，运行生成的测试程序。

`tests/sm86_host_probe.cpp` 可作为独立 D3D12 宿主构建，不加载游戏、不提交画面。特殊模式检查活动插件函数、真实设备能力和状态查询，输出实际运行库路径与倍率上限，既不指定内部路径也不固定上限。`tests/run_sm86_host_tests.py` 面向预先准备好的四组诊断副本，验证默认关闭、特殊后端、入口缺失与另一运行库冲突，并保留当前上游日志中可识别的安装与模型信息作为辅助观察；可选的 `--expected-preset` 只核对该次诊断明确选择的模型。此脚本只能用于诊断副本，不能把实际游戏目录作为参数。

完整 OptiScaler 构建沿用项目现有流程。构建、单元测试和独立初始化通过，只证明对应检查通过；各游戏的全屏行为、补帧画面、延迟和稳定性仍需实际运行验证。此通用功能不包含游戏专用地址补丁、分辨率覆盖或固定超分比例。
