# TODO: Runtime Action Dispatcher Module

## Next Step

下一步实现 `Runtime/ActionDispatcher.h/.cpp` 的最小版本。这个模块接收 `SAction` 或 `TVector<SAction>`，调用当前 `IOutputBackend`，并记录最近派发结果，为后续 `ZMappingSession` 串联 `ZMappingEngine` 和输出后端提供稳定入口。

优先做这个模块的理由：

- `ZMappingEngine` 已经能生成 `SAction`。
- `IOutputBackend` 和 `ZNullOutputBackend` 已经能接收并记录动作。
- Runtime 还缺少“统一派发动作并处理输出失败”的边界，不能让 `ZMappingSession` 直接散落调用输出后端。
- 先用 Null 后端测试派发链路，可以不接 Win32 就验证 Runtime 行为。

## AntiMicroX Architecture Reference

AntiMicroX 的输出侧由 event handler 处理键盘、鼠标和平台 API 调用，映射逻辑最终会走到统一输出处理点。MappyZ 本轮借鉴这个“输出动作集中派发”的结构，但保持 Runtime 和平台后端解耦。

MappyZ 本轮改进点：

- [x] Runtime 只依赖 `IOutputBackend` 接口，不依赖 Windows SendInput。
- [x] `ZActionDispatcher` 不理解输入设备、profile、QML 或 SDL。
- [x] 派发结果以纯数据快照记录，后续可以接日志面板。
- [x] 不引入全局 singleton 或 factory，构造时接收 `IOutputBackend&`。
- [x] 不复制 AntiMicroX 的 event handler 代码、平台映射表或类结构。

## Scope

本轮只覆盖 Runtime 动作派发。

包含：

- [x] `source/Runtime/ActionDispatcher.h`
- [x] `source/Runtime/ActionDispatcher.cpp`
- [x] `tests/Runtime/ActionDispatcherTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不实现 `ZMappingSession`。
- [x] 不订阅 `ZInputRuntime` 或输入后端。
- [x] 不调用 `ZMappingEngine`。
- [x] 不实现 Windows SendInput。
- [x] 不做动作去重、按键状态缓存或释放补偿。
- [x] 不做批量事务回滚；批量派发中途失败时只记录失败并继续/停止的策略本轮明确。
- [x] 不接 UI 日志模型。

## Design Decisions

- [x] `ZActionDispatcher` 属于 Runtime 层，可以依赖 Core `SAction` 和 `Backends/Output/OutputBackend.h`。
- [x] `ZActionDispatcher` 不拥有输出后端；构造时接收 `IOutputBackend&`。
- [x] `ZActionDispatcher` 禁止拷贝和移动。
- [x] 本轮不做线程安全承诺，沿用单线程测试假设。
- [x] 默认启用派发，支持 `SetEnabled(false)` 暂停输出。
- [x] disabled 状态下不调用后端，返回失败结果，并记录一次派发失败。
- [x] 批量派发按输入 action 顺序调用后端，遇到失败后继续派发剩余 action，最终返回失败。
- [x] 最近派发记录设置固定容量，默认 `128`；超过容量时丢弃最旧记录。
- [x] 派发记录中保存 action 副本、成功标记和消息。

## Proposed Types

- [x] 新增 `struct SActionDispatchRecord`：
  - `SAction Action`
  - `bool bSucceeded = false`
  - `StdString Message`
- [x] 新增 `struct SActionDispatchSummary`：
  - `uint32 RequestedCount = 0`
  - `uint32 SucceededCount = 0`
  - `uint32 FailedCount = 0`
  - `bool bSucceeded = true`
  - `StdString Message`
- [x] 新增 `class ZActionDispatcher`：
  - `static constexpr uint32 MaxRecentRecords = 128`
  - `explicit ZActionDispatcher(IOutputBackend& OutputBackend)`
  - `TResult<void> DispatchAction(const SAction& Action)`
  - `SActionDispatchSummary DispatchActions(const TVector<SAction>& Actions)`
  - `bool IsEnabled() const noexcept`
  - `void SetEnabled(bool bEnabled) noexcept`
  - `SOutputBackendStatus GetOutputStatus() const`
  - `TVector<SActionDispatchRecord> ListRecentRecords() const`
  - `uint32 GetRecentRecordCount() const noexcept`
  - `void ClearRecentRecords()`

## Behavior

- [x] 默认构造后 `IsEnabled() == true`。
- [x] `DispatchAction()` enabled 时调用 `OutputBackend.SendAction(Action)`。
- [x] `DispatchAction()` 成功时记录 `bSucceeded = true`，Message 可为 `"ok"`。
- [x] `DispatchAction()` 失败时记录 `bSucceeded = false`，Message 使用后端错误信息。
- [x] `DispatchAction()` disabled 时不调用后端，返回失败并记录 disabled 消息。
- [x] `DispatchActions()` 对空列表返回成功 summary，Requested/Succeeded/Failed 都为 0。
- [x] `DispatchActions()` 按顺序派发每个 action。
- [x] `DispatchActions()` 即使某个 action 失败，也继续派发后续 action。
- [x] `DispatchActions()` 只要有任一失败，summary `bSucceeded = false`。
- [x] `DispatchActions()` 汇总 Requested/Succeeded/Failed 数量。
- [x] `GetOutputStatus()` 透传 `OutputBackend.GetStatus()`。
- [x] `ListRecentRecords()` 返回快照拷贝。
- [x] `ClearRecentRecords()` 只清空记录，不改变 enabled 状态。
- [x] 最近记录超过 128 条时丢弃最旧记录。

## Error Semantics

- [x] disabled 状态错误消息包含 `"disabled"`。
- [x] 单个 action 后端失败时，`DispatchAction()` 返回失败。
- [x] 批量派发用 `SActionDispatchSummary` 表达部分成功，不返回 `TResult`，避免隐藏部分成功信息。
- [x] 本轮不新增自定义错误码，使用 ZeroStyle `TResult<void>` / `SError` 能力。

## CMake Plan

- [x] 将 `source/Runtime/ActionDispatcher.cpp` 加入 `MappyZRuntime`。
- [x] `MappyZRuntime` public 链接 `MappyZOutputBackends`。
- [x] 新增 `tests/Runtime/ActionDispatcherTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 测试使用 `ZNullOutputBackend`。
- [x] 不新增第三方依赖。
- [x] 不修改主应用 target。

## Tests

- [x] 默认 enabled。
- [x] 单个 Keyboard action 会派发到 Null 后端并记录成功。
- [x] 单个 MouseButton action 会派发到 Null 后端并记录成功。
- [x] 后端 Error 状态下单个派发返回失败并记录失败。
- [x] disabled 状态下不调用后端并记录失败。
- [x] disabled 后重新 enabled 可以恢复派发。
- [x] 空批量派发返回成功 summary。
- [x] 批量派发按顺序发送多个 action。
- [x] 批量派发遇到失败后继续派发后续 action。
- [x] 批量 summary 正确统计 requested/succeeded/failed。
- [x] `GetOutputStatus()` 透传后端状态。
- [x] `ListRecentRecords()` 返回快照拷贝。
- [x] `ClearRecentRecords()` 清空记录但保持 enabled 状态。
- [x] 最近记录容量上限 128 生效。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增文本文件使用 CRLF 行尾。
- [x] `ZActionDispatcher` 不依赖 UI、SDL 或平台层。
- [x] `MappyZRuntimeTests` 覆盖成功、失败、disabled、批量和记录容量行为。

## Follow-Up Module

- [ ] 下一模块建议实现 `Runtime/MappingSession.h/.cpp`，串联 `ZMappingEngine` 和 `ZActionDispatcher`。
- [ ] 再下一步实现 profile JSON loader/saver，开始支持真实配置文件。
- [ ] 后续实现 `ZWindowsSendInputBackend`，把 `SAction` 转换为 Windows 键盘和鼠标输出。
