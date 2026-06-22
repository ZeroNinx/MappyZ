# TODO: Runtime Event Pump Module

## Next Step

下一步实现 `Runtime/RuntimeEventPump.h/.cpp` 的最小版本，负责从 `ZBackendEventQueue` 批量取走后端事件，并在调用线程内按 FIFO 分发。输入事件会送入 `ZMappingSession` 执行映射和输出派发；设备事件和输入事件也提供轻量回调，供后续 RuntimeHost 或 UI Bridge 更新状态。

优先做这个模块的理由：

- [ ] `ZBackendEventQueue` 已能安全接收 SDL worker 线程回调。
- [ ] `ZMappingSession` 已能处理单个 `SInputEvent` 并派发输出动作。
- [ ] 现在缺少一个主线程 pump，把队列事件转成 Runtime 可执行流程。
- [ ] 先做 event pump，可以在不启动真实 App 的情况下用 fake/null 组件测试“输入事件 -> 映射 -> 输出”的近端到端链路。

## Scope

本轮只做队列消费和事件分发，不做完整 RuntimeHost 或 App bootstrap。

包含：

- [x] `source/Runtime/RuntimeEventPump.h`
- [x] `source/Runtime/RuntimeEventPump.cpp`
- [x] `tests/Runtime/RuntimeEventPumpTests.cpp`
- [x] CMake target 和 CTest 接入
- [x] 从 `ZBackendEventQueue` drain 事件
- [x] 设备连接/断开事件回调分发
- [x] 输入事件回调分发
- [x] 输入事件驱动 `ZMappingSession::HandleInputEvent()`
- [x] pump summary 和最近处理记录

不做：

- [ ] 不启动或停止输入后端。
- [ ] 不 attach/detach `ZBackendEventQueue`。
- [ ] 不读取或保存 profile。
- [ ] 不选择 active profile。
- [ ] 不直接拥有 `IInputBackend` 或 `IOutputBackend`。
- [ ] 不实现 UI Bridge 或 QML model。
- [ ] 不修改 `Main.cpp`。
- [ ] 不实现事件 coalescing、去抖、限速或优先级。

## Architecture Boundary

- [x] `ZRuntimeEventPump` 属于 Runtime 层。
- [x] 依赖 `ZBackendEventQueue`、`ZMappingSession` 和 Core 数据类型。
- [x] 不依赖 Qt、QML、SDL、Win32 或 nlohmann_json。
- [x] public header 不包含平台头。
- [x] pump 只在调用 `PumpOnce()` 的线程执行 Runtime 分发；跨线程输入只通过 `ZBackendEventQueue` 进入。
- [x] 不绕过 `ZMappingSession` 直接调用 `ZMappingEngine` 或 `ZActionDispatcher`。

## Proposed Types

- [x] 新增 `struct SRuntimeEventPumpSummary`：
  - `uint32 DrainedEventCount = 0`
  - `uint32 DeviceConnectedCount = 0`
  - `uint32 DeviceDisconnectedCount = 0`
  - `uint32 InputEventCount = 0`
  - `uint32 MappedInputCount = 0`
  - `uint32 DispatchedInputCount = 0`
  - `uint32 FailedDispatchInputCount = 0`
  - `uint32 InvalidEventCount = 0`
  - `StdString Message`
- [x] 新增 `struct SRuntimeEventPumpRecord`：
  - `SBackendEvent Event`
  - `bool bHandled = false`
  - `SMappingSessionResult MappingResult`
  - `StdString Message`
- [x] `SRuntimeEventPumpRecord::MappingResult` 仅在 `Event.Type == Input` 且 payload 有效时有业务意义；设备事件和 invalid 事件的该字段保持默认值，消费方不应读取。
- [x] 新增 `class ZRuntimeEventPump`：
  - `static constexpr uint32 MaxRecentRecords = 128`
  - `ZRuntimeEventPump(ZBackendEventQueue& EventQueue, ZMappingSession& MappingSession)`
  - `SRuntimeEventPumpSummary PumpOnce()`
  - `void SetDeviceConnectedHandler(std::function<void(const SDeviceInfo&)> Handler)`
  - `void SetDeviceDisconnectedHandler(std::function<void(const SDeviceId&)> Handler)`
  - `void SetInputEventHandler(std::function<void(const SInputEvent&)> Handler)`
  - `TVector<SRuntimeEventPumpRecord> ListRecentRecords() const`
  - `uint32 GetRecentRecordCount() const noexcept`
  - `void ClearRecentRecords()`

## Behavior

- [x] `PumpOnce()` 调用 `ZBackendEventQueue::DrainEvents()`。
- [x] 没有事件时返回空 summary，不调用任何 handler，不调用 mapping session。
- [x] 所有事件按 drain 返回的 FIFO 顺序处理。
- [x] `DeviceConnected` 事件 payload 必须是 `SDeviceInfo`，匹配时调用 device connected handler。
- [x] `DeviceDisconnected` 事件 payload 必须是 `SDeviceId`，匹配时调用 device disconnected handler。
- [x] `Input` 事件 payload 必须是 `SInputEvent`，匹配时先调用 input event handler，再调用 `ZMappingSession::HandleInputEvent()`。
- [x] handler 未设置时安全静默跳过，不输出日志，避免正常运行模式刷 stderr。
- [x] event type 和 payload 不匹配时跳过该事件，`InvalidEventCount` 增加，并输出警告日志。
- [x] mapping session disabled 或无动作不是 pump 错误，summary 仍表示事件已处理。
- [x] `SMappingSessionResult::bMapped` 为 true 时增加 `MappedInputCount`。
- [x] `SMappingSessionResult::bDispatched` 为 true 时增加 `DispatchedInputCount`。
- [x] `SMappingSessionResult::bMapped == true && bDispatched == false` 时增加 `FailedDispatchInputCount`；无映射命中不算失败。
- [x] 每个 drain 到的事件都追加 recent record，包括 invalid event。
- [x] recent records 容量固定 128，超过容量丢弃最旧记录。
- [x] `ClearRecentRecords()` 只清空 pump 记录，不影响 queue、session、handlers。

## Handler Semantics

- [x] handlers 由调用方设置，pump 不拥有 UI 或状态存储。
- [x] handlers 在 `PumpOnce()` 调用线程同步执行。
- [x] handler 抛异常不作为普通错误路径处理；项目代码不应从 handler 抛异常。
- [x] handler 可以为空，空 handler 静默跳过，不影响 mapping session 处理输入事件。
- [x] 输入 handler 先于 mapping session 执行，便于 UI/状态层看到原始输入。

## Error Semantics

- [x] `PumpOnce()` 不返回 `TResult`，因为单批事件可能部分有效、部分无效。
- [x] 无事件不是错误。
- [x] invalid payload 不是致命错误，记录到 summary 和 recent record 后继续处理后续事件。
- [x] mapping dispatch 失败由 `SMappingSessionResult` 表达，pump 只汇总计数。
- [x] 本轮不捕获 handler 异常。

## CMake Plan

- [x] 将 `source/Runtime/RuntimeEventPump.cpp` 加入 `MappyZRuntime`。
- [x] 新增 `tests/Runtime/RuntimeEventPumpTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 测试使用 `ZFakeInputBackend`、`ZBackendEventQueue`、`ZNullOutputBackend`、`ZActionDispatcher`、`ZMappingSession`。
- [x] 不修改主应用 target。
- [x] 不修改 Core、InputBackend、OutputBackend 接口。

## Tests

- [x] 默认无事件时 summary 全零。
- [x] 设备连接事件调用 connected handler 并计数。
- [x] 设备断开事件调用 disconnected handler 并计数。
- [x] 输入事件调用 input handler。
- [x] 输入事件触发 `ZMappingSession`，Button -> Keyboard 可派发到 Null 后端。
- [x] 空 handler 不影响输入映射。
- [x] 多事件按 FIFO 顺序处理。
- [x] invalid event type/payload mismatch 会跳过并增加 `InvalidEventCount`。
- [x] invalid event 不阻止后续有效事件处理。
- [x] mapping session disabled 时 input 仍计数，但 mapped/dispatched 计数为 0。
- [x] dispatch 失败时 `FailedDispatchInputCount` 增加。
- [x] `ListRecentRecords()` 返回快照拷贝。
- [x] `ClearRecentRecords()` 清空记录但不影响 handlers 或 session。
- [x] recent records 容量上限 128 生效。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZRuntimeEventPump` 不依赖 UI、SDL 或平台层。
- [x] 可以用 fake input、test profile、null output 验证输入事件到动作派发的 Runtime 链路。

## Follow-Up Module

- [ ] 后续实现 RuntimeHost，持有真实后端、event queue、event pump、profile 和 session 的生命周期。
- [ ] 后续实现 App bootstrap，选择 `ZSdlInputBackend` 和 `ZWindowsSendInputBackend`，加载默认测试 profile。
- [ ] 后续实现基础 QML 输入状态面板，显示设备和最近输入事件。
- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
