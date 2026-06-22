# TODO: Runtime Host Lifecycle Module

## Next Step

下一步实现 `Runtime/RuntimeHost.h/.cpp` 的最小版本，作为运行时组合入口。`ZRuntimeHost` 不直接依赖 SDL、Win32 或 UI，而是通过传入的 `IInputBackend` 和 `IOutputBackend` 引用组装 `ZBackendEventQueue`、`ZActionDispatcher`、`ZMappingSession` 和 `ZRuntimeEventPump`，对外提供启动、停止、pump 和 profile 替换接口。

优先做这个模块的理由：

- [ ] `ZBackendEventQueue` 已经能安全接收后端 worker 回调。
- [ ] `ZRuntimeEventPump` 已经能把队列事件送进 `ZMappingSession`。
- [ ] `ZActionDispatcher` 和 `ZMappingSession` 已经能完成输入到输出的运行链路。
- [ ] 当前缺少一个统一生命周期入口，App bootstrap 还不能干净地组合这些模块。

## Scope

本轮只做 Runtime 组合和生命周期，不接真实 App UI。

包含：

- [x] `source/Runtime/RuntimeHost.h`
- [x] `source/Runtime/RuntimeHost.cpp`
- [x] `tests/Runtime/RuntimeHostTests.cpp`
- [x] CMake target 和 CTest 接入
- [x] 非拥有式引用 `IInputBackend` 和 `IOutputBackend`
- [x] 内部组合 event queue、dispatcher、mapping session、event pump
- [x] `Start()` / `Stop()` / `IsRunning()`
- [x] `PumpOnce()`
- [x] `ReplaceProfile()` 和 mapping enabled 开关
- [x] 设备/输入事件 handler 转发
- [x] 为 `ZFakeInputBackend` 增加 start failure 测试注入接口，便于覆盖 host 启动失败回滚路径

不做：

- [x] 不创建 `ZSdlInputBackend` 或 `ZWindowsSendInputBackend`。
- [x] 不选择平台后端。
- [x] 不读取 profile 文件或扫描 profile 目录。
- [x] 不实现 Qt/QML bridge。
- [x] 不修改 `Main.cpp`。
- [x] 不实现定时器或后台 pump 线程。
- [x] 不实现 pressed-state cleanup 或退出时释放补偿。
- [x] 不实现多 profile、layer、mode shift。
- [x] 不修改 `IInputBackend`、`IOutputBackend` 或 Core 接口；只允许扩展 fake backend 测试辅助方法。

## Architecture Boundary

- [x] `ZRuntimeHost` 属于 Runtime 层。
- [x] 依赖 `IInputBackend`、`IOutputBackend` 和现有 Runtime 组件。
- [x] 不依赖 Qt、QML、SDL、Win32 或 nlohmann_json。
- [x] public header 不包含平台头。
- [x] host 负责生命周期顺序，不负责平台后端构造。
- [x] host 不绕过 `ZRuntimeEventPump` 直接处理 backend callback。

## Proposed Types

- [x] 新增 `enum class ERuntimeHostState`：
  - `Stopped`
  - `Running`
  - `Error`
- [x] 新增 `struct SRuntimeHostStatus`：
  - `ERuntimeHostState State = ERuntimeHostState::Stopped`
  - `StdString Message`
  - `SOutputBackendStatus OutputStatus`
- [x] 新增 `struct SRuntimeHostStartOptions`：
  - `bool bAttachEventQueue = true`
  - `bool bStartInputBackend = true`
  - `bool bEnableMapping = true`
- [x] 扩展 `ZFakeInputBackend`：
  - `void SetStartError(StdString Message)`
  - `void ClearStartError()`
- [x] 新增 `class ZRuntimeHost`：
  - `ZRuntimeHost(IInputBackend& InputBackend, IOutputBackend& OutputBackend)`
  - `TResult<void> Start(SRuntimeHostStartOptions Options = {})`
  - `void Stop()`
  - `bool IsRunning() const noexcept`
  - `SRuntimeHostStatus GetStatus() const`
  - `SRuntimeEventPumpSummary PumpOnce()`
  - `void ReplaceProfile(SMappingProfile Profile)`
  - `SMappingProfile GetProfileSnapshot() const`
  - `void SetMappingEnabled(bool bEnabled)`
  - `bool IsMappingEnabled() const noexcept`
  - `ZBackendEventQueue& GetEventQueue()`
  - `ZRuntimeEventPump& GetEventPump()`
  - `ZMappingSession& GetMappingSession()`
  - `ZActionDispatcher& GetActionDispatcher()`

## Behavior

- [x] 默认构造后状态为 Stopped，输入后端未启动，event queue 未 attached。
- [x] `Start()` 默认先 attach event queue，再调用 input backend `Start()`，最后设置 mapping enabled。
- [x] `bStartInputBackend = false` 时 host 不调用 `IInputBackend::Start()`；该模式用于外部已经管理输入后端生命周期的场景。
- [x] `bAttachEventQueue = true && bStartInputBackend = false` 合法，但 stopped backend 是否产生事件由具体 backend 决定，host 不额外保证。
- [x] `Start()` 成功后状态为 Running。
- [x] 重复 `Start()` 返回 Ok，不重复 attach、不重复启动后端、不清空 runtime 状态。
- [x] 如果 input backend `Start()` 失败，host 状态变为 Error，返回原始错误。
- [x] input backend 启动失败时，如果本次 Start 已 attach event queue，则需要 detach 回滚。
- [x] `Stop()` 先停止 input backend，再 detach event queue，状态变为 Stopped。
- [x] 重复 `Stop()` 安全。
- [x] 析构时调用 `Stop()`，避免后端保留回调。
- [x] `PumpOnce()` 仅在 Running 状态处理事件；非 Running 状态返回空 summary，并输出调试日志。
- [x] `ReplaceProfile()` 替换 `ZMappingSession` 内的 profile 快照。
- [x] `SetMappingEnabled()` 透传到 `ZMappingSession::SetEnabled()`。
- [x] `GetStatus()` 返回 host 状态、消息和当前 output backend 状态。
- [x] `GetEventQueue()` / `GetEventPump()` 等访问器用于测试和后续 UI bridge，不转移所有权。

## Handler Semantics

- [x] host 不新增自己的 handler 类型，调用方通过 `GetEventPump()` 设置 handler。
- [x] handler 仍由 `ZRuntimeEventPump` 在 `PumpOnce()` 调用线程同步执行。
- [x] host 不捕获 handler 异常。
- [x] 空 handler 是正常状态。

## Error Semantics

- [x] `Start()` 返回 `TResult<void>`，因为后端启动可能失败。
- [x] `ZFakeInputBackend::SetStartError()` 设置后，`Start()` 返回失败结果，Message 使用注入文本。
- [x] `ZFakeInputBackend::ClearStartError()` 清除注入错误，后续 `Start()` 恢复成功。
- [x] `Stop()` 不返回错误，保持幂等。
- [x] `PumpOnce()` 不返回 `TResult`，沿用 `ZRuntimeEventPump` 的 summary 语义。
- [x] input backend start failure 使用原始 `SError`，host 只补充状态消息。
- [x] output backend 错误不阻止 host Running；由 dispatcher/session/pump summary 表达派发失败。

## Lifecycle Order

- [x] 构造成员顺序固定为：event queue、dispatcher、mapping session、event pump。
- [x] event pump 引用 event queue 和 mapping session，必须在它们之后构造。
- [x] Stop 顺序固定为：input backend stop -> event queue detach。
- [x] host 不清空 event queue pending events；调用方可通过 `GetEventQueue().Clear()` 控制。
- [x] host 不清空 pump/session/dispatcher recent records；调用方可按需清理。

## CMake Plan

- [x] 将 `source/Runtime/RuntimeHost.cpp` 加入 `MappyZRuntime`。
- [x] 新增 `tests/Runtime/RuntimeHostTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 测试使用 `ZFakeInputBackend` 和 `ZNullOutputBackend`。
- [x] 将 `FakeInputBackendTests.cpp` 增加 start failure 注入覆盖。
- [x] 不修改主应用 target。
- [x] 不修改 Core、InputBackend、OutputBackend 接口。

## Tests

- [x] 默认状态为 Stopped，host 未 running。
- [x] `Start()` 启动 input backend 并 attach event queue。
- [x] `Start()` 后 fake input 事件经 `PumpOnce()` 触发 mapping dispatch。
- [x] 重复 `Start()` 幂等，不重复清空 pending events 或 records。
- [x] `Stop()` 停止 input backend 并 detach event queue。
- [x] 重复 `Stop()` 安全。
- [x] 析构时清空 backend callback，后续 fake backend 注入不崩溃。
- [x] input backend start failure 返回错误，host 状态为 Error，event queue 回滚 detach。
- [x] fake backend `SetStartError()` 后 `Start()` 返回错误，`ClearStartError()` 后恢复成功。
- [x] `Start({ .bStartInputBackend = false })` 不调用 input backend start，但可按选项 attach event queue。
- [x] `PumpOnce()` 在 Stopped 状态不处理事件。
- [x] `ReplaceProfile()` 更新 active profile 快照。
- [x] `SetMappingEnabled(false)` 后输入事件不会产生动作。
- [x] output backend Error 时 host 仍 Running，pump summary 记录 dispatch failure。
- [x] `GetStatus()` 反映 host 状态和 output status。
- [x] 通过 `GetEventPump()` 设置 input handler 能被调用。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZRuntimeHost` 不依赖 UI、SDL 或平台层。
- [x] 可以用 fake input、test profile、null output 验证 host 级输入到输出链路。

## Follow-Up Module

- [ ] 后续实现 App bootstrap，选择 `ZSdlInputBackend` 和 `ZWindowsSendInputBackend`，加载默认测试 profile，并驱动 `ZRuntimeHost`。
- [ ] 后续实现基础 QML 输入状态面板，显示设备和最近输入事件。
- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现退出时 pressed-state cleanup。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
