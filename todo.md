# TODO: UI Bridge AppController Module

## Next Step

下一步实现 `source/UI/Bridge/AppController.h/.cpp` 的最小版本，把已经完成的 `ZApplicationBootstrap` 暴露给 QML。`ZAppController` 只负责应用级命令和运行时状态，不直接读取手柄、不直接发送键鼠、不承载设备列表或输入状态模型。

优先做这个模块的理由：

- [x] `ZApplicationBootstrap` 已经能创建后端、加载 profile、构造并启动 `ZRuntimeHost`。
- [x] 当前 `Main.cpp` 仍只加载静态 QML，UI 没有 runtime 状态入口。
- [x] QML 后续的设备面板、输入面板、日志面板都需要一个稳定的 C++ bridge 根对象。
- [x] 先做 AppController 可以把 Qt/QML 生命周期和 runtime 生命周期分开，避免 QML 直接拼后端对象。

## Scope

本轮只做 UI Bridge 根控制器，不做设备列表 model、不做输入状态 model、不做绑定编辑保存。

包含：

- [x] `source/UI/Bridge/AppController.h`
- [x] `source/UI/Bridge/AppController.cpp`
- [x] `tests/UI/Bridge/AppControllerTests.cpp`
- [x] CMake 增加 `MappyZUIBridge` 静态库和 `MappyZUIBridgeTests`
- [x] `ZAppController` 继承 `QObject`
- [x] 内部拥有 `ZApplicationBootstrap`
- [x] 暴露 runtime 状态、消息、mapping enabled、last pump summary 基础属性
- [x] 提供 `initializeRuntime()` / `startRuntime()` / `stopRuntime()` / `pumpOnce()` invokable
- [x] 提供可选 `QTimer` pump 控制：`startPumpTimer()` / `stopPumpTimer()`
- [x] `Main.cpp` 注册或注入 `ZAppController` 到 QML context

不做：

- [x] 不实现 `ZDeviceModel`
- [x] 不实现 `ZInputStateModel`
- [x] 不实现 `ZLogModel`
- [x] 不实现 profile 目录扫描或保存
- [x] 不修改 Core、Runtime、Backend 接口
- [x] 不让 QML 直接访问 `IInputBackend`、`IOutputBackend` 或 `ZRuntimeHost`
- [x] 不复制或移植第三方项目代码结构

## Architecture Boundary

- [x] `ZAppController` 属于 UI Bridge 层。
- [x] 可以依赖 Qt Core、`ZApplicationBootstrap` 和 Runtime public types。
- [x] 不包含 SDL 头，不包含 Win32 头。
- [x] 不直接处理 backend callback；只调用 `ZApplicationBootstrap` / `ZRuntimeHost` 的 public API。
- [x] QML 只绑定 `ZAppController` 暴露的属性和 invokable，不直接持有 runtime 对象。
- [x] 运行时 pump 在 UI 线程同步调用，后续如有性能问题再引入单独 runtime thread。

## Proposed Types

- [x] 新增 `class ZAppController final : public QObject`
- [x] 构造函数：
  - `explicit ZAppController(QObject* Parent = nullptr)`
  - `ZAppController(TInputBackendFactory InputFactory, TOutputBackendFactory OutputFactory, QObject* Parent = nullptr)`
- [x] QML 属性：
  - `Q_PROPERTY(QString runtimeState READ RuntimeState NOTIFY RuntimeStatusChanged)`
  - `Q_PROPERTY(QString runtimeMessage READ RuntimeMessage NOTIFY RuntimeStatusChanged)`
  - `Q_PROPERTY(QString outputState READ OutputState NOTIFY RuntimeStatusChanged)`
  - `Q_PROPERTY(bool mappingEnabled READ IsMappingEnabled WRITE SetMappingEnabled NOTIFY MappingEnabledChanged)`
  - `Q_PROPERTY(bool pumpTimerRunning READ IsPumpTimerRunning NOTIFY PumpTimerRunningChanged)`
  - `Q_PROPERTY(int lastDrainedEventCount READ LastDrainedEventCount NOTIFY LastPumpSummaryChanged)`
  - `Q_PROPERTY(int lastInputEventCount READ LastInputEventCount NOTIFY LastPumpSummaryChanged)`
  - `Q_PROPERTY(int lastMappedInputCount READ LastMappedInputCount NOTIFY LastPumpSummaryChanged)`
  - `Q_PROPERTY(int lastDispatchedInputCount READ LastDispatchedInputCount NOTIFY LastPumpSummaryChanged)`
- [x] Public invokable：
  - `Q_INVOKABLE bool initializeRuntime(bool useNullOutput = false)`
  - `Q_INVOKABLE bool startRuntime()`
  - `Q_INVOKABLE void stopRuntime()`
  - `Q_INVOKABLE void pumpOnce()`
  - `Q_INVOKABLE void startPumpTimer(int intervalMs = 16)`
  - `Q_INVOKABLE void stopPumpTimer()`
- [x] Signals：
  - `void RuntimeStatusChanged()`
  - `void MappingEnabledChanged()`
  - `void PumpTimerRunningChanged()`
  - `void LastPumpSummaryChanged()`
  - `void RuntimeError(QString message)`

## Dependency Injection

- [x] 生产构造使用默认 `ZApplicationBootstrap`。
- [x] 测试构造允许注入 `TInputBackendFactory` 和 `TOutputBackendFactory`，复用 ApplicationBootstrap 的 fake/null 测试路径。
- [x] controller 构造后不自动 initialize、不自动 start，避免测试和 UI 加载时产生真实输入/输出副作用。
- [x] `Main.cpp` 可以选择只创建 controller 并注入 QML，不自动启动 runtime。

## Runtime Behavior

- [x] `initializeRuntime()` 调用 `ZApplicationBootstrap::Initialize()`。
- [x] `initializeRuntime(useNullOutput=true)` 传递 `SApplicationBootstrapOptions::bUseNullOutput = true`。
- [x] `startRuntime()` 如果尚未 initialize，先返回 false 并发出 `RuntimeError`，不隐式 initialize。
- [x] `startRuntime()` 成功后更新 runtime status 属性。
- [x] `stopRuntime()` 安全幂等，停止 timer 后调用 bootstrap `StopRuntime()`。
- [x] `pumpOnce()` 只调用 bootstrap `PumpOnce()`，并缓存 summary 到属性。
- [x] `startPumpTimer()` 创建或启动内部 `QTimer`，timeout 连接到 `pumpOnce()`。
- [x] `stopPumpTimer()` 停止 timer，重复调用安全。
- [x] 析构时停止 timer，并调用 bootstrap `StopRuntime()`。

## Mapping Behavior

- [x] `mappingEnabled` 默认从 runtime host/session 状态读取；未 initialize 时缓存期望值。
- [x] `SetMappingEnabled(bool)` 更新缓存值；如果 runtime 已 initialize，则透传到 `GetRuntimeHost().SetMappingEnabled()`。
- [x] `initializeRuntime()` 创建 `SApplicationBootstrapOptions` 时必须设置 `bEnableMapping = CachedMappingEnabled`，避免 `ZRuntimeHost::Start()` 用默认 true 覆盖用户选择。
- [x] `initializeRuntime()` 成功后仍将缓存的 mapping enabled 应用到 host，保证 stopped host snapshot 与 UI 状态一致。
- [x] `startRuntime()` 成功后再调用一次 `GetRuntimeHost().SetMappingEnabled(CachedMappingEnabled)`，覆盖 initialize 与 start 之间用户再次切换 mapping enabled 的场景。
- [x] 仅控制启停，不新增或保存 mapping rule。

## Status Mapping

- [x] `runtimeState` 将 `EApplicationBootstrapState` 转成稳定字符串：
  - `created`
  - `ready`
  - `running`
  - `error`
- [x] `outputState` 将 `EOutputBackendState` 转成稳定字符串：
  - `unavailable`
  - `ready`
  - `error`
- [x] `runtimeMessage` 直接来自 `SApplicationBootstrapStatus::Message`。
- [x] 失败路径保留最后错误消息，并发出 `RuntimeError(message)`。
- [x] 仅暴露 UI 友好的字符串，不把 enum 直接暴露给 QML。

## Main.cpp Integration

- [x] `Main.cpp` 创建 `ZAppController AppController;`
- [x] 在 `Engine.loadFromModule()` 前通过 root context 注入：
  - context property 名称建议为 `appController`
- [x] 本轮不强制 QML 使用它；可以先只完成注入和编译链路。
- [x] 后续 QML 再把按钮、状态栏和日志绑定到 `appController`。

## CMake Plan

- [x] 生产构建继续使用 `find_package(Qt6 REQUIRED COMPONENTS Quick)`。
- [x] 仅在 `if(BUILD_TESTING)` 内追加 `find_package(Qt6 REQUIRED COMPONENTS Test)`，避免非测试构建依赖 Qt Test 模块。
- [x] 新增 `MappyZUIBridge` 静态库：
  - `source/UI/Bridge/AppController.cpp`
- [x] `MappyZUIBridge` 链接：
  - `Qt6::Core`
  - `MappyZApp`
- [x] `MappyZ` 可执行文件链接 `MappyZUIBridge`。
- [x] 新增 `MappyZUIBridgeTests`：
  - `tests/UI/Bridge/Main.cpp`
  - `tests/UI/Bridge/AppControllerTests.cpp`
  - 链接 `MappyZUIBridge`、`MappyZInputBackends`、`MappyZOutputBackends`、`Qt6::Test`、`Catch2::Catch2`。
- [x] `MappyZUIBridgeTests` 不使用 `Catch2::Catch2WithMain`，因为 `QSignalSpy` 需要进程中已有 `QCoreApplication`。
- [x] `tests/UI/Bridge/Main.cpp` 自定义测试入口：
  - 先创建 `QCoreApplication App(argc, argv)`
  - 再调用 `Catch::Session().run(argc, argv)`

## Tests

- [x] 默认构造状态为 `created`，timer 未运行。
- [x] fake/null factory 下 `initializeRuntime(true)` 成功后状态为 `ready`。
- [x] `startRuntime()` 未 initialize 时返回 false 并发出 `RuntimeError`。
- [x] initialize 后 `startRuntime()` 成功，状态变为 `running`。
- [x] `stopRuntime()` 将 running 状态停回 `ready`，重复调用安全。
- [x] `pumpOnce()` 在 running 状态更新 last summary 属性。
- [x] `pumpOnce()` 在非 running 状态 summary 保持空。
- [x] `startPumpTimer()` 启动 timer，`stopPumpTimer()` 停止 timer。
- [x] 析构时停止 timer 和 runtime，不崩溃。
- [x] `SetMappingEnabled(false)` 在 initialize 前缓存；initialize 后 host mapping disabled。
- [x] `SetMappingEnabled()` 在 initialize 后透传到 host。
- [x] `SetMappingEnabled(false)` 在 initialize 前设置后，`startRuntime()` 不会把 mapping enabled 恢复成 true。
- [x] initialize 后、start 前再次切换 `mappingEnabled`，`startRuntime()` 后 host 使用最新缓存值。
- [x] 使用 `QSignalSpy` 验证 `RuntimeStatusChanged`、`MappingEnabledChanged`、`RuntimeError` 等关键信号。
- [x] input factory failure 时 `initializeRuntime()` 返回 false，状态为 `error`，发出 `RuntimeError`。
- [x] output factory failure 时 `initializeRuntime(false)` 返回 false；`initializeRuntime(true)` 使用 Null output 不调用 output factory。
- [x] header 不包含 SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZAppController` public API 不暴露 Core/Runtime 复杂对象给 QML。
- [x] QML engine 能拿到 `appController` context property。

## Follow-Up Module

- [ ] 后续实现 `ZDeviceModel`，把设备列表和热插拔状态暴露给 QML。
- [ ] 后续实现 `ZInputStateModel`，显示最近输入和控件状态。
- [ ] 后续把 `ui/Main.qml` 的状态栏、按钮和日志绑定到 `appController`。
- [ ] 后续实现 profile directory/active profile 管理。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
