# TODO: Application Bootstrap Module

## Next Step

下一步实现 `source/App/ApplicationBootstrap.h/.cpp` 的最小版本，作为应用层运行时装配入口。它负责根据编译期开关选择输入/输出后端，加载启动 profile，构造 `ZRuntimeHost`，并向 `Main.cpp` 或后续 UI Bridge 暴露启动、停止和 pump 接口。

优先做这个模块的理由：

- [x] `ZRuntimeHost` 已经提供统一生命周期入口，但当前 `Main.cpp` 仍只加载 QML。
- [x] SDL 输入后端、Windows SendInput 输出后端、ProfileManager 和 Runtime Host 已经具备组合条件。
- [x] UI Bridge 后续需要一个稳定的 App 级对象，而不是直接拼后端和 Runtime 组件。
- [x] 先把 App bootstrap 做成可测试的纯 C++ 组合层，可以避免 QML 层过早承担生命周期责任。

## Scope

本轮只做 App 层 bootstrap，不实现 QML model，不做绑定编辑 UI。

包含：

- [x] `source/App/ApplicationBootstrap.h`
- [x] `source/App/ApplicationBootstrap.cpp`
- [x] `tests/App/ApplicationBootstrapTests.cpp`
- [x] CMake 增加 `MappyZApp` 静态库和 `MappyZAppTests`
- [x] App bootstrap 拥有输入后端、输出后端和 `ZRuntimeHost`
- [x] 启动时可选加载 profile 文件
- [x] 无 profile 文件时使用空的默认 profile snapshot
- [x] 公开 `StartRuntime()` / `StopRuntime()` / `PumpOnce()`
- [x] 公开只读 runtime status 查询
- [x] 公开 `ZRuntimeHost&` 访问器，供后续 UI Bridge 绑定

不做：

- [x] 不实现 QML `ZAppController`
- [x] 不实现 `DeviceModel` / `InputStateModel` / `LogModel`
- [x] 不新增 profile 编辑或保存逻辑
- [x] 不实现定时器；`Main.cpp` 或 UI Bridge 后续负责定期调用 `PumpOnce()`
- [x] 不实现系统托盘、设置页、打包逻辑
- [x] 不复制或移植第三方项目代码结构
- [x] 不把本地路径、机器信息或私有参考项目细节写入提交

## Architecture Boundary

- [x] `ApplicationBootstrap` 属于 App 层。
- [x] 可以依赖 Runtime、InputBackends、OutputBackends。
- [x] 不依赖 QML 类型，不包含 QML 文件。
- [x] 不直接处理 SDL 事件，不直接调用 Win32 SendInput。
- [x] 不绕过 `ZRuntimeHost` 处理 mapping 或 dispatch。
- [x] 不让 UI 直接持有后端对象。
- [x] 参考成熟映射软件的分层思路：App 层集中拥有运行时对象，UI 只观察状态并发起命令。

## Proposed Types

- [x] 新增 factory 类型别名：
  - `using TInputBackendFactory = std::function<TResult<TUniquePtr<IInputBackend>>()>;`
  - `using TOutputBackendFactory = std::function<TResult<TUniquePtr<IOutputBackend>>()>;`
- [x] 新增 `struct SApplicationBootstrapOptions`：
  - `StdPath ProfilePath`
  - `bool bUseNullOutput = false`
  - `bool bStartInputBackend = true`
  - `bool bEnableMapping = true`
- [x] 新增 `enum class EApplicationBootstrapState`：
  - `Created`
  - `Ready`
  - `Running`
  - `Error`
- [x] 新增 `struct SApplicationBootstrapStatus`：
  - `EApplicationBootstrapState State = EApplicationBootstrapState::Created`
  - `StdString Message`
  - `SRuntimeHostStatus RuntimeStatus`
- [x] 新增 `class ZApplicationBootstrap`：
  - `ZApplicationBootstrap()`
  - `ZApplicationBootstrap(TInputBackendFactory InputFactory, TOutputBackendFactory OutputFactory)`
  - `~ZApplicationBootstrap()`
  - `TResult<void> Initialize(SApplicationBootstrapOptions Options = {})`
  - `TResult<void> StartRuntime()`
  - `void StopRuntime()`
  - `SRuntimeEventPumpSummary PumpOnce()`
  - `SApplicationBootstrapStatus GetStatus() const`
  - `ZRuntimeHost& GetRuntimeHost()`
  - `const ZRuntimeHost& GetRuntimeHost() const`
- [x] `GetRuntimeHost()` 有前置条件：必须在 `Initialize()` 成功后调用；实现中使用 assert 或同等调试检查，不返回 optional。

## Backend Selection

- [x] 输入后端：
  - 如果定义 `MAPPYZ_HAS_SDL3_INPUT`，创建 `ZSdlInputBackend`。
  - 否则 `Initialize()` 返回错误，说明当前构建没有可用真实输入后端。
  - 测试不依赖 SDL，可通过测试专用构造或 factory 注入 fake backend。
- [x] 输出后端：
  - 如果 `Options.bUseNullOutput = true`，创建 `ZNullOutputBackend`。
  - `Options.bUseNullOutput = true` 时不调用 output factory。
  - Windows 且定义 `MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT` 时，默认创建 `ZWindowsSendInputBackend`。
  - 无真实输出后端可用时回退到 `ZNullOutputBackend`，状态消息说明当前为调试输出。
- [x] bootstrap 不暴露具体后端类型给 UI。

## Profile Startup Behavior

- [x] `Options.ProfilePath` 为空时，创建默认空 profile：
  - `ProfileName = "Default"`
  - `Rules` 为空
  - `bEnabled` 使用现有 profile 默认值
- [x] `Options.ProfilePath` 非空时，调用 `ZProfileManager::LoadProfile()`。
- [x] profile 加载失败时：
  - `Initialize()` 返回原始错误
  - 状态变为 `Error`
  - 不创建 running host
- [x] profile 加载成功后，调用 `ZRuntimeHost::ReplaceProfile()`。
- [x] 本轮不保存 profile，不扫描目录。

## Lifecycle Behavior

- [x] 默认构造后状态为 `Created`，未创建后端，未创建 host。
- [x] `Initialize()` 成功后状态为 `Ready`。
- [x] `Created` 状态下 `Initialize()` 执行完整 setup。
- [x] `Error` 状态下允许重新 `Initialize()`，重新创建后端、重新加载 profile、重新构造 host。
- [x] `Ready` 状态下重复 `Initialize()` 返回 Ok，不重复创建后端，不清空 runtime 状态。
- [x] `Running` 状态下重复 `Initialize()` 返回 Ok，不停止 host，不重复创建后端，不重新加载 profile。
- [x] `StartRuntime()` 要求已 `Initialize()`。
- [x] `StartRuntime()` 调用 `ZRuntimeHost::Start()`，使用 options 透传 mapping enabled 和 start input backend。
- [x] `StartRuntime()` 成功后状态为 `Running`。
- [x] `StartRuntime()` 失败时状态为 `Error`，保留错误消息。
- [x] `StopRuntime()` 安全幂等。
- [x] `StopRuntime()` 在 `Created` 状态且 host 未构造时 no-op，状态保持 `Created`。
- [x] `StopRuntime()` 在 `Error` 状态且 host 未构造时 no-op，状态保持 `Error`。
- [x] `StopRuntime()` 在 `Ready` 状态调用 host `Stop()`，状态保持 `Ready`。
- [x] `StopRuntime()` 在 `Running` 状态调用 host `Stop()`，状态回到 `Ready`。
- [x] 析构时调用 `StopRuntime()`。
- [x] `PumpOnce()` 仅在 `Running` 状态委托给 host；其他状态返回空 summary。

## Factory And Testability

- [x] 为测试提供后端 factory 注入点，避免 App 测试依赖 SDL 或真实 SendInput。
- [x] factory 返回 `TUniquePtr<IInputBackend>` 和 `TUniquePtr<IOutputBackend>`。
- [x] bootstrap 用 `TUniquePtr` 持有后端所有权，再解引用传给 `ZRuntimeHost(IInputBackend&, IOutputBackend&)`。
- [x] 成员声明顺序必须保证后端生命周期长于 host：
  - `TUniquePtr<IInputBackend> InputBackend`
  - `TUniquePtr<IOutputBackend> OutputBackend`
  - `TUniquePtr<ZRuntimeHost> RuntimeHost`
- [x] 析构顺序依赖成员声明顺序：`RuntimeHost` 先析构，后端对象后析构，避免 host 持有悬垂引用。
- [x] 生产默认 factory 根据编译期开关选择真实后端。
- [x] 测试 factory 使用 `ZFakeInputBackend` 和 `ZNullOutputBackend`。
- [x] factory 创建失败使用 `TResult` 返回清晰错误，不使用异常作为常规控制流。

## Initialization Notes

- [x] `ZProfileManager` 当前是无状态可默认构造类型，bootstrap 可在 `Initialize()` 中创建局部实例或作为普通成员持有。
- [x] `Initialize()` 是 heavy setup：创建后端、加载 profile、构造 stopped 状态的 `ZRuntimeHost`。
- [x] `StartRuntime()` 是薄封装：只调用已经构造好的 host `Start()`，不重新创建后端或重新加载 profile。
- [x] 输出后端默认不让 Initialize 因缺少真实输出失败；无 Windows SendInput 时回退 `ZNullOutputBackend` 并通过状态消息说明。

## CMake Plan

- [x] 新增 `MappyZApp` 静态库：
  - `source/App/ApplicationBootstrap.cpp`
- [x] `MappyZApp` 链接：
  - `MappyZRuntime`
  - `MappyZInputBackends`
  - `MappyZOutputBackends`
- [x] `MappyZ` 可执行文件链接 `MappyZApp`。
- [x] 本轮可以不修改 `Main.cpp` 的行为；如果修改，只做构造 bootstrap 的最小接入。
- [x] 新增 `MappyZAppTests`：
  - `tests/App/ApplicationBootstrapTests.cpp`
  - 链接 `MappyZApp`、`MappyZInputBackends`、`MappyZOutputBackends`、Catch2。

## Tests

- [x] 默认构造状态为 `Created`。
- [x] 使用 fake/null factory 的 `Initialize()` 成功后状态为 `Ready`。
- [x] `Initialize()` 创建 runtime host 并加载默认空 profile。
- [x] 指定 profile JSON 路径时加载 profile 并应用到 host。
- [x] profile 加载失败时返回错误，状态为 `Error`。
- [x] 重复 `Initialize()` 幂等。
- [x] `Initialize()` 失败进入 Error 后，修复 factory/profile 条件再调用 `Initialize()` 可以成功进入 Ready。
- [x] `StartRuntime()` 在未 Initialize 时返回错误。
- [x] `StartRuntime()` 成功启动 fake input backend 和 host。
- [x] `StopRuntime()` 停止 host，状态回到 `Ready`。
- [x] `StopRuntime()` 在 Created/Error 且 host 未构造时 no-op。
- [x] 重复 `StopRuntime()` 安全。
- [x] `GetRuntimeHost()` 只在 Initialize 成功后使用；测试不在未初始化状态调用。
- [x] `PumpOnce()` 在 Running 时委托 host 并处理输入事件。
- [x] `PumpOnce()` 在非 Running 状态返回空 summary。
- [x] 析构时停止 host 并清理 backend callback。
- [x] factory 创建输入后端失败时 Initialize 返回错误。
- [x] factory 创建输出后端失败时 Initialize 返回错误。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ApplicationBootstrap` 不包含 QML、SDL 或 Win32 public 类型。
- [x] App 层可以在测试中通过 fake input + null output 启动完整 runtime 链路。

## Follow-Up Module

- [ ] 后续实现 `Main.cpp` 的实际 runtime 接入和 Qt `QTimer` pump。
- [ ] 后续实现 UI Bridge：`ZAppController`、`DeviceModel`、`InputStateModel`、`LogModel`。
- [ ] 后续实现 QML 输入状态面板，显示设备、最近输入和输出状态。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
- [ ] 后续实现 profile directory/active profile 管理。
- [ ] 后续实现退出时 pressed-state cleanup。
