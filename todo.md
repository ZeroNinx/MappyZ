# TODO: Runtime Backend Event Queue Module

## Next Step

下一步实现 `Runtime/BackendEventQueue.h/.cpp` 的最小版本，负责订阅 `IInputBackend` 的设备和输入回调，并把回调转换为线程安全的待处理事件队列。后续 App bootstrap 再从主线程或 UI thread 调用 `DrainEvents()`，把 SDL worker 线程事件安全送进 Runtime。

优先做这个模块的理由：

- [x] `ZSdlInputBackend` 已完成，但它的回调从 worker 线程触发。
- [x] `ZDeviceManager`、`ZInputRuntime`、`ZMappingSession` 当前都不做线程安全承诺。
- [x] 直接在 SDL callback 中调用 mapping/output 会让运行链路难以调试，也不利于 UI 状态读取。
- [x] 先建立一个小的事件队列，可以让 App bootstrap、UI Bridge、RuntimeHost 后续共享同一个线程切换入口。

## Scope

本轮只做后端事件队列，不做完整 App bootstrap。

包含：

- [x] `source/Runtime/BackendEventQueue.h`
- [x] `source/Runtime/BackendEventQueue.cpp`
- [x] `tests/Runtime/BackendEventQueueTests.cpp`
- [x] CMake target 和 CTest 接入
- [x] 设备连接、设备断开、输入事件三类事件入队
- [x] 线程安全入队和批量 drain
- [x] 重复 attach/detach 的安全行为

不做：

- [ ] 不启动或停止输入后端，生命周期仍由调用方控制。
- [ ] 不直接调用 `ZMappingSession`。
- [ ] 不直接调用 `ZActionDispatcher` 或输出后端。
- [ ] 不读取或保存 profile。
- [ ] 不实现 UI Bridge 或 QML model。
- [ ] 不实现 App bootstrap。
- [ ] 不实现事件 coalescing、去抖、限速或优先级。
- [ ] 不改变 `IInputBackend` 当前单回调接口。

## Architecture Boundary

- [x] `ZBackendEventQueue` 属于 Runtime 层，依赖 `IInputBackend` 和 Core 数据类型。
- [x] 不依赖 Qt、QML、SDL、Win32 或 nlohmann_json。
- [x] public header 不包含平台头。
- [x] 队列只负责收集和交付事件，不解释 control id、不执行 mapping。
- [x] 调用方负责在合适线程调用 `DrainEvents()`，并决定如何处理事件。

## Proposed Types

- [x] 新增 `enum class EBackendEventType`：
  - `DeviceConnected`
  - `DeviceDisconnected`
  - `Input`
- [x] 新增 `using TBackendEventPayload = std::variant<std::monostate, SDeviceInfo, SDeviceId, SInputEvent>`。
- [x] 新增 `struct SBackendEvent`：
  - `EBackendEventType Type`
  - `TBackendEventPayload Payload`
- [x] 新增 `class ZBackendEventQueue`：
  - `explicit ZBackendEventQueue(IInputBackend& Backend)`
  - `~ZBackendEventQueue()`
  - `void Attach()`
  - `void Detach()`
  - `bool IsAttached() const noexcept`
  - `TVector<SBackendEvent> DrainEvents()`
  - `uint32 GetPendingEventCount() const`
  - `void Clear()`

## Behavior

- [x] 默认未 attached，队列为空。
- [x] `Attach()` 设置后端三个回调，重复调用不重复绑定、不清空队列。
- [x] `Attach()` 时如果后端已有非空回调，输出 stderr 警告后覆盖。
- [x] `Detach()` 清空后端三个回调，重复调用安全。
- [x] 析构时自动 `Detach()`，避免后端持有悬空回调。
- [x] `OnDeviceConnected` 入队 `DeviceConnected`，Payload 为完整 `SDeviceInfo`。
- [x] `OnDeviceDisconnected` 入队 `DeviceDisconnected`，Payload 为 `SDeviceId`。
- [x] `OnInputEvent` 入队 `Input`，Payload 为完整 `SInputEvent`。
- [x] `DrainEvents()` 按 FIFO 返回当前所有事件，并清空内部队列。
- [x] `DrainEvents()` 内部用 `swap` 取走 pending vector，避免逐元素拷贝。
- [x] `Clear()` 丢弃待处理事件，但不改变 attach 状态。
- [x] `GetPendingEventCount()` 返回当前待处理事件数量。
- [x] 所有回调入队、`DrainEvents()`、`Clear()`、`GetPendingEventCount()` 使用 mutex 保护。
- [x] 本轮不设置容量上限，避免 worker 线程回调丢事件；容量策略后续根据 UI/Runtime 压力再加。

## Callback Ownership

- [x] `ZBackendEventQueue` attach 后拥有 `IInputBackend` 三个回调槽位。
- [x] 调用方不要同时让 `ZDeviceManager` 或 `ZInputRuntime` 直接 attach 到同一个后端。
- [x] 后续 RuntimeHost 负责从 `DrainEvents()` fan-out 到设备状态、输入状态和 mapping session。
- [x] 如果 Attach 时发现后端已有回调，本轮直接覆盖并在 stderr 输出警告。
- [x] Detach 时无条件清空后端三个回调；裸 `std::function` 无法判断是否被外部覆盖，本轮不做检测。

## Error Semantics

- [x] `Attach()` / `Detach()` 不返回错误，保持与现有 Runtime attach 类一致。
- [x] 空回调不是错误，queue attach 后负责填充后端回调。
- [x] 后端 stopped 时是否产生事件由具体 backend 决定，queue 不额外过滤。
- [x] 入队不抛异常作为普通错误路径；本轮不处理内存分配失败。

## CMake Plan

- [x] 将 `source/Runtime/BackendEventQueue.cpp` 加入 `MappyZRuntime`。
- [x] 新增 `tests/Runtime/BackendEventQueueTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 测试使用 `ZFakeInputBackend`，不依赖 SDL。
- [x] 不修改主应用 target。
- [x] 不修改 Core、InputBackend、OutputBackend 接口。

## Tests

- [x] 默认状态未 attached 且队列为空。
- [x] `Attach()` 后设备连接事件会入队。
- [x] `Attach()` 后设备断开事件会入队。
- [x] `Attach()` 后输入事件会入队。
- [x] 三类事件 Payload 类型与 `EBackendEventType` 匹配，可用 `std::get_if` 读取。
- [x] 多个事件按 FIFO 顺序 drain。
- [x] `DrainEvents()` 返回快照并清空队列。
- [x] `DrainEvents()` 返回的 vector 修改不影响队列内部状态。
- [x] `Clear()` 清空队列但保持 attached。
- [x] 重复 `Attach()` 不清空队列、不重复产生事件。
- [x] `Attach()` 覆盖已有后端回调时不会崩溃。
- [x] `Detach()` 后后端事件不再入队。
- [x] 重复 `Detach()` 安全。
- [x] 析构后后端回调被清空，后续 fake backend 注入不崩溃。
- [x] 多线程入队和 drain 不崩溃，事件数量正确。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZBackendEventQueue` 不依赖 UI、SDL 或平台层。
- [x] `ZBackendEventQueue` 可以安全接收 SDL worker 线程回调。

## Follow-Up Module

- [ ] 后续实现 RuntimeHost，把 `ZBackendEventQueue`、设备状态、输入状态、`ZMappingSession` 和输出后端串成主线程 pump。
- [ ] 后续实现 App bootstrap，选择 `ZSdlInputBackend` 和 `ZWindowsSendInputBackend`，加载默认测试 profile。
- [ ] 后续实现基础 QML 输入状态面板，显示设备和最近输入事件。
- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
