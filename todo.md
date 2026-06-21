# TODO: Output Backend Interface and Null Backend Module

## Next Step

下一步实现 `Backends/Output` 的最小版本：`IOutputBackend` 纯接口和 `ZNullOutputBackend` 测试后端。这个模块接收 `SAction`，记录输出动作和后端状态，为后续 `ZActionDispatcher`、`ZMappingSession` 和 Windows SendInput 后端提供稳定边界。

优先做这个模块的理由：

- `ZMappingEngine` 已经能把 `SInputEvent + SMappingProfile` 转成 `SAction`，但目前没有输出侧接口承接动作。
- 先做 `ZNullOutputBackend` 可以在不调用 Win32 API 的情况下测试 Runtime 派发链路。
- 输出后端边界需要早定，避免后续 `ZActionDispatcher` 直接依赖 Windows SendInput 或 UI 状态。
- Null 后端可以作为日志、调试面板和端到端测试的观察点。

## AntiMicroX Architecture Reference

AntiMicroX 通过 event handler factory 在不同平台输出实现之间切换，例如 SendInput、XTest 或 uinput。这个方向值得借鉴：映射层只产生动作，平台输出由后端封装。

MappyZ 本轮只借鉴输出后端分层，不照搬实现：

- [x] 借鉴”输出后端接口统一，平台实现可替换”的结构。
- [x] 借鉴”需要查询输出后端状态和错误信息”的诊断思路。
- [x] 改进为 `IOutputBackend` 只接收 Core `SAction`，不理解 UI、profile 或输入设备。
- [x] 先做 `ZNullOutputBackend`，不直接进入 Windows SendInput。
- [x] 后续可以添加 factory 或注册表，但本轮不引入 singleton 或全局 mutable 状态。
- [x] 不复制 AntiMicroX 的 event handler 代码、类结构、平台映射表或资源。

## Scope

本轮只覆盖输出后端接口和 Null 后端。

包含：

- [x] `source/Backends/Output/OutputBackend.h`
- [x] `source/Backends/Output/NullOutputBackend.h`
- [x] `source/Backends/Output/NullOutputBackend.cpp`
- [x] `tests/Backends/Output/NullOutputBackendTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不实现 `ZWindowsSendInputBackend`。
- [x] 不调用 Win32、Qt、SDL 或 QML。
- [x] 不实现 `ZActionDispatcher`。
- [x] 不实现 `ZMappingSession`。
- [x] 不实现输出后端 factory。
- [x] 不做动作去重、按键状态管理或释放补偿。
- [x] 不做 key name 到平台 virtual-key 的转换。

## Design Decisions

- [x] `IOutputBackend` 属于 Backend 层，可以依赖 Core `SAction` 和 `ProjectCore.h`。
- [x] `IOutputBackend` 不依赖 Runtime、UI、Qt、SDL 或平台 API。
- [x] `IOutputBackend::SendAction()` 返回 `TResult<void>`，后续真实后端可表达失败原因。
- [x] `IOutputBackend::GetStatus()` 返回轻量状态快照，供 Runtime 和 UI 观察。
- [x] `ZNullOutputBackend` 只记录动作，不产生系统级输入。
- [x] `ZNullOutputBackend` 不做线程安全承诺，本轮沿用单线程测试假设。
- [x] `ZNullOutputBackend` 默认 Ready，可通过测试接口切换到 Error/Unavailable。
- [x] Error/Unavailable 状态下 `SendAction()` 返回失败，不记录动作。
- [x] `EActionType::None` 本轮作为无效动作处理，`SendAction()` 返回失败，不记录动作。

## Proposed Types

### Output Backend Interface

- [x] 新增 `enum class EOutputBackendState`：
  - `Unavailable`
  - `Ready`
  - `Error`
- [x] 新增 `struct SOutputBackendStatus`：
  - `EOutputBackendState State = EOutputBackendState::Unavailable`
  - `StdString Message`
- [x] 新增 `class IOutputBackend`：
  - 虚析构
  - 禁止拷贝和移动
  - `ZERO_NODISCARD virtual TResult<void> SendAction(const SAction& Action) = 0`
  - `ZERO_NODISCARD virtual SOutputBackendStatus GetStatus() const = 0`

### Null Output Backend

- [x] 新增 `class ZNullOutputBackend final : public IOutputBackend`。
- [x] 提供：
  - `TResult<void> SendAction(const SAction& Action) override`
  - `SOutputBackendStatus GetStatus() const override`
  - `TVector<SAction> ListActions() const`
  - `uint32 GetActionCount() const noexcept`
  - `void ClearActions()`
  - `void SetStatus(SOutputBackendStatus Status)`
  - `void SetReady(StdString Message = "ready")`
  - `void SetUnavailable(StdString Message)`
  - `void SetError(StdString Message)`
- [x] `ListActions()` 返回快照拷贝，调用方修改不影响内部记录。

## Behavior

- [x] 默认构造后状态为 `Ready`，Message 为 `"ready"`。
- [x] `SendAction()` 在 Ready 状态下记录非 `None` 动作并返回成功。
- [x] `SendAction()` 在 Ready 状态下遇到 `EActionType::None` 返回失败，不记录动作。
- [x] `SendAction()` 在 Unavailable 状态下返回失败，不记录动作。
- [x] `SendAction()` 在 Error 状态下返回失败，不记录动作。
- [x] `SetReady()` 切换状态为 Ready，并更新 Message。
- [x] `SetUnavailable()` 切换状态为 Unavailable，并更新 Message。
- [x] `SetError()` 切换状态为 Error，并更新 Message。
- [x] `ClearActions()` 只清空动作记录，不改变状态。
- [x] `ListActions()` 返回动作快照拷贝。

## Error Semantics

- [x] `EActionType::None` 的错误信息应包含 `"none"` 或 `"invalid"`，方便测试和日志定位。
- [x] Unavailable 状态的错误信息使用当前 `SOutputBackendStatus::Message`。
- [x] Error 状态的错误信息使用当前 `SOutputBackendStatus::Message`。
- [x] 本轮不引入自定义错误码映射；使用 ZeroStyle `TResult<void>` / `SError` 的现有能力。

## CMake Plan

- [x] 新建 `MappyZOutputBackends` static library target。
- [x] 源文件包含 `source/Backends/Output/NullOutputBackend.cpp`。
- [x] `MappyZOutputBackends` public 链接 `MappyZCore`。
- [x] 为 `MappyZOutputBackends` 添加与其他 target 一致的 warning 选项。
- [x] 新增 `MappyZOutputBackendTests` 测试目标。
- [x] 测试目标链接 `MappyZOutputBackends` 和 `Catch2::Catch2WithMain`。
- [x] 通过 `catch_discover_tests(MappyZOutputBackendTests)` 注册到 CTest。
- [x] 不修改主应用或 Runtime target。

## Tests

- [x] `IOutputBackend` 头文件可 include 编译。
- [x] `ZNullOutputBackend` 默认状态为 Ready。
- [x] Ready 状态下发送 Keyboard action 会记录并返回成功。
- [x] Ready 状态下发送 MouseButton action 会记录并返回成功。
- [x] Ready 状态下发送 MouseMove action 会记录并返回成功。
- [x] Ready 状态下发送 `EActionType::None` 返回失败且不记录。
- [x] Unavailable 状态下 `SendAction()` 返回失败且不记录。
- [x] Error 状态下 `SendAction()` 返回失败且不记录。
- [x] `SetReady()` 可从 Error/Unavailable 恢复 Ready。
- [x] `ListActions()` 返回快照拷贝。
- [x] `ClearActions()` 清空记录但不改变状态。
- [x] `GetActionCount()` 返回当前记录数量。
- [x] `SetStatus()` 能设置完整状态快照。
- [x] 新增 Output Backend 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增文本文件使用 CRLF 行尾。
- [x] `IOutputBackend` 不依赖 Runtime、UI 或平台层。
- [x] `ZNullOutputBackend` 能记录动作并覆盖失败路径。

## Follow-Up Module

- [ ] 下一模块建议实现 `Runtime/ActionDispatcher.h/.cpp`，将 `SAction` 派发给当前 `IOutputBackend`。
- [ ] 再下一步实现 `Runtime/MappingSession.h/.cpp`，串联 `ZInputRuntime`、`ZMappingEngine` 和 `ZActionDispatcher`。
- [ ] 后续实现 `ZWindowsSendInputBackend`，把 `SAction` 转换为 Windows 键盘和鼠标输出。
