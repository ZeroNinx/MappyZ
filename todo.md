# TODO: Runtime Mapping Session Module

## Next Step

下一步实现 `Runtime/MappingSession.h/.cpp` 的最小版本。这个模块持有当前 active `SMappingProfile` 快照，接收单个 `SInputEvent`，调用 `ZMappingEngine` 生成 `SAction`，再交给 `ZActionDispatcher` 派发。

优先做这个模块的理由：

- `ZMappingEngine` 已经能把输入事件映射为动作。
- `ZActionDispatcher` 已经能把动作派发到 `IOutputBackend`。
- Runtime 还缺少“一次输入事件的完整映射会话入口”。
- 有了 `ZMappingSession` 后，可以用 fake/null 组件做接近端到端的 Runtime 测试。

## AntiMicroX Architecture Reference

AntiMicroX 的运行时会在设备/控件对象中维护当前映射状态，并把输入事件一路转成输出事件。MappyZ 本轮借鉴“运行中 active profile 驱动输入到输出”的概念，但保持数据快照和执行逻辑分离。

MappyZ 本轮改进点：

- [x] `ZMappingSession` 持有 profile 快照，不直接读写配置文件。
- [x] 输入映射由 Core `ZMappingEngine` 负责，Session 只做编排。
- [x] 输出派发由 `ZActionDispatcher` 负责，Session 不直接调用输出后端。
- [x] Session 可启停映射，便于后续 UI 的 Mapping enabled 开关。
- [x] 不使用 QObject 控件树，不复制 AntiMicroX 的 set/slot 类结构或代码。

## Scope

本轮只覆盖 Runtime 映射会话最小行为。

包含：

- [x] `source/Runtime/MappingSession.h`
- [x] `source/Runtime/MappingSession.cpp`
- [x] `tests/Runtime/MappingSessionTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不订阅 `IInputBackend::OnInputEvent`。
- [x] 不依赖 `ZInputRuntime`。
- [x] 不实现 profile JSON loader/saver。
- [x] 不实现 profile 匹配或自动切换。
- [x] 不实现 UI model 或 QML bridge。
- [x] 不实现动作去重、按键状态缓存或释放补偿。
- [x] 不实现多 profile/layer/mode shift。
- [x] 不做线程安全承诺，本轮沿用单线程测试假设。

## Design Decisions

- [x] `ZMappingSession` 属于 Runtime 层，可以依赖 `ZMappingEngine` 和 `ZActionDispatcher`。
- [x] `ZMappingSession` 不拥有 `ZActionDispatcher`；构造时接收引用。
- [x] `ZMappingSession` 自己持有 `SMappingProfile` 副本，作为当前 active profile 快照。
- [x] `ReplaceProfile()` 用传入 profile 替换当前快照。
- [x] 默认 session enabled，支持 `SetEnabled(false)` 暂停映射。
- [x] session disabled 时不调用 MappingEngine 或 ActionDispatcher，返回空 summary，并记录状态。
- [x] profile disabled 时仍调用 MappingEngine 也会得到空动作；Session 不额外复制规则判断。
- [x] 每次 `HandleInputEvent()` 记录一次最近处理结果。
- [x] 最近处理记录固定容量 128，超过容量丢弃最旧记录。

## Proposed Types

- [x] 新增 `struct SMappingSessionResult`：
  - `uint32 ActionCount = 0`
  - `SActionDispatchSummary DispatchSummary`
  - `bool bMapped = false`
  - `bool bDispatched = false`
  - `StdString Message`
- [x] 新增 `struct SMappingSessionRecord`：
  - `SInputEvent Event`
  - `SMappingSessionResult Result`
- [x] 新增 `class ZMappingSession`：
  - `static constexpr uint32 MaxRecentRecords = 128`
  - `explicit ZMappingSession(ZActionDispatcher& Dispatcher)`
  - `SMappingSessionResult HandleInputEvent(const SInputEvent& Event)`
  - `void ReplaceProfile(SMappingProfile Profile)`
  - `SMappingProfile GetProfileSnapshot() const`
  - `bool IsEnabled() const noexcept`
  - `void SetEnabled(bool bEnabled) noexcept`
  - `TVector<SMappingSessionRecord> ListRecentRecords() const`
  - `uint32 GetRecentRecordCount() const noexcept`
  - `void ClearRecentRecords()`

## Behavior

- [x] 默认构造后 enabled。
- [x] 默认 profile 为空且 enabled；输入事件不会产生动作。
- [x] `ReplaceProfile()` 替换当前 profile 快照。
- [x] `GetProfileSnapshot()` 返回 profile 拷贝，调用方修改不影响 session 内部。
- [x] enabled 状态下 `HandleInputEvent()` 调用 `ZMappingEngine::MapInput()`。
- [x] 有动作时调用 `ZActionDispatcher::DispatchActions()`。
- [x] 无动作时不调用 dispatcher，Result `ActionCount = 0`，`bMapped = false`，`bDispatched = false`。
- [x] 有动作且全部派发成功时，Result `bMapped = true`，`bDispatched = true`。
- [x] 有动作但派发部分或全部失败时，Result `bMapped = true`，`bDispatched = false`。
- [x] session disabled 时不映射、不派发，Message 包含 `"disabled"`。
- [x] 每次 `HandleInputEvent()` 都追加 recent record，包括 disabled 和无匹配情况。
- [x] `ClearRecentRecords()` 只清空记录，不改变 enabled 状态或 profile。
- [x] recent records 超过 128 条时丢弃最旧记录。

## Result Semantics

- [x] 无匹配动作不是错误，Message 可为 `"no actions"`。
- [x] disabled 状态不是后端错误，Message 包含 `"disabled"`。
- [x] 派发失败信息来自 `SActionDispatchSummary::Message`。
- [x] 本轮 `HandleInputEvent()` 不返回 `TResult`，因为无匹配和部分派发失败都需要结构化表达。

## CMake Plan

- [x] 将 `source/Runtime/MappingSession.cpp` 加入 `MappyZRuntime`。
- [x] `MappyZRuntime` 已链接 `MappyZCore` 和 `MappyZOutputBackends`，无需新增第三方依赖。
- [x] 新增 `tests/Runtime/MappingSessionTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 测试使用 `ZNullOutputBackend` + `ZActionDispatcher`。
- [x] 不修改主应用 target。

## Tests

- [x] 默认 enabled。
- [x] 默认空 profile 处理输入后无动作、不派发。
- [x] `ReplaceProfile()` 后 Button -> Keyboard 可映射并派发到 Null 后端。
- [x] `ReplaceProfile()` 保存的是快照，外部修改原 profile 不影响 session。
- [x] `GetProfileSnapshot()` 返回拷贝，修改返回值不影响 session。
- [x] session disabled 时不调用 dispatcher，后端不记录动作。
- [x] disabled 后 re-enable 可恢复映射派发。
- [x] profile disabled 时无动作。
- [x] 多条规则命中时派发多个动作。
- [x] dispatcher/backend 失败时 session result 标记派发失败。
- [x] 无匹配动作也会追加 recent record。
- [x] disabled 输入也会追加 recent record。
- [x] `ClearRecentRecords()` 清空记录但保留 enabled 和 profile。
- [x] recent records 容量上限 128 生效。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增文本文件使用 CRLF 行尾。
- [x] `ZMappingSession` 不依赖 UI、SDL 或平台层。
- [x] `MappyZRuntimeTests` 覆盖映射成功、无动作、disabled、派发失败、快照隔离和记录容量。

## Follow-Up Module

- [ ] 下一模块建议实现 profile JSON loader/saver，开始支持真实配置文件。
- [ ] 再下一步实现 `ZSdlInputBackend`，接入真实 SDL3 输入事件。
- [ ] 后续实现 `ZWindowsSendInputBackend`，把 `SAction` 转换为 Windows 键盘和鼠标输出。
