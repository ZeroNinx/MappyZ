# TODO: Core Mapping Engine Module

## Next Step

下一步实现 `Core/MappingEngine.h/.cpp` 的最小版本。这个模块消费单个 `SInputEvent` 和一个 `SMappingProfile` 快照，输出按 profile 规则命中的 `TVector<SAction>`。

优先做这个模块的理由：

- `SInputEvent`、`SAction`、`SMappingRule`、`SMappingProfile` 已经稳定，下一步需要把这些 Core 数据真正串起来。
- `ZMappingEngine` 是后续 `ZMappingSession`、输出派发、profile loader 和绑定 UI 的共同核心。
- 先做纯 Core 映射算法，可以避免 Runtime、SDL、Win32 和 QML 影响规则语义。
- 有了 MappingEngine 后，后续 `ZNullOutputBackend` 和 `ZActionDispatcher` 可以直接做端到端 Runtime 测试。

## AntiMicroX Architecture Reference

AntiMicroX 的映射路径大体是控件对象收到输入后，沿着 set / slot 配置执行对应输出。它的优势是控件层能直接处理复杂行为，例如 set 切换、长按、宏和特殊鼠标模式。

MappyZ 本轮只借鉴其中的映射语义，不照搬执行结构：

- [x] 借鉴”一个输入可以命中多条映射并产生多个输出动作”的行为。
- [x] 借鉴”按钮按下和抬起都要能驱动输出状态”的语义。
- [x] 借鉴”摇杆映射鼠标移动需要 deadzone 和 sensitivity”的基础参数。
- [x] 改进为 stateless 纯函数式引擎：`SInputEvent + SMappingProfile -> TVector<SAction>`。
- [x] 不让控件对象拥有映射逻辑；控件状态仍由 Runtime 管，映射决策由 Core 引擎做。
- [x] 本轮不实现 AntiMicroX 的 set/layer、宏、鼠标加速度曲线、连发、长按短按或 mode shift。
- [x] 不复制 AntiMicroX 的代码、类结构、XML 格式、资源或文案。

## Scope

本轮只覆盖 Core 映射引擎最小行为。

包含：

- [x] `source/Core/MappingEngine.h`
- [x] `source/Core/MappingEngine.cpp`
- [x] `tests/Core/MappingEngineTests.cpp`
- [x] CMake target 调整和测试接入

不做：

- [x] 不实现 Runtime `ZMappingSession`。
- [x] 不实现 `ZActionDispatcher`。
- [x] 不实现 `IOutputBackend` 或 `ZNullOutputBackend`。
- [x] 不实现 JSON profile loader/saver。
- [x] 不实现 profile 匹配、保存、迁移或冲突检测。
- [x] 不实现状态去重、边沿检测或”只在阈值跨越时触发”。
- [x] 不实现复杂 axis curve、鼠标加速度、宏、连发、长按短按、组合键、layer 或 mode shift。

## Design Decisions

- [x] `ZMappingEngine` 属于 Core 层，只能依赖 `ProjectCore.h`、`InputEvent.h`、`Action.h`、`MappingProfile.h`。
- [x] `ZMappingEngine` 是无状态对象；不保存上一次输入、不保存按键是否已按下、不访问 Runtime 状态。
- [x] `MapInput()` 不修改传入的 `SInputEvent` 或 `SMappingProfile`。
- [x] 输出动作按 profile 中规则顺序追加，保持 deterministic。
- [x] disabled profile 直接返回空 action 列表。
- [x] disabled rule 会被跳过。
- [x] 空 profile 或无匹配规则返回空 action 列表。
- [x] 本轮遇到 action type 和 payload variant 不匹配时跳过该规则，不崩溃。
- [x] 本轮不返回错误；没有命中或配置不支持都返回空 action 列表。

## Interface Contract

- [x] 新增 `class ZMappingEngine final`。
- [x] 提供：
  - `ZERO_NODISCARD TVector<SAction> MapInput(const SInputEvent& Event, const SMappingProfile& Profile) const;`
- [x] 可选 private helper：
  - `bool DoesRuleMatchInput(const SInputEvent& Event, const SMappingRule& Rule) const`
  - `TOptional<SAction> BuildAction(const SInputEvent& Event, const SMappingRule& Rule) const`
  - `TOptional<SAction> BuildPressReleaseAction(const SInputEvent& Event, const SMappingRule& Rule) const`
  - `TOptional<SAction> BuildAnalogAction(const SInputEvent& Event, const SMappingRule& Rule) const`

## Matching Behavior

- [x] `ControlId` 必须精确匹配。
- [x] `ControlType` 必须精确匹配。
- [x] Button / Hat 的 `PressRelease` 规则接受 `Pressed` 和 `Released` 事件。
- [x] Button / Hat 的 `PressRelease` 规则忽略 `Changed` 事件。
- [x] Trigger / Axis1D 的阈值规则接受 `Changed` 事件。
- [x] Trigger / Axis1D 的激活判断为 `Event.Value >= Rule.Input.Threshold`。
- [x] Axis2D 的 `Analog` 规则接受 `Changed` 事件。
- [x] Axis2D 的 deadzone 使用径向长度判断：`sqrt(X*X + Y*Y) <= Deadzone` 时不输出动作。
- [x] 本轮不 clamp `Threshold`、`Deadzone` 或 `Sensitivity`；配置有效性留给后续 profile validation。

## Action Building Behavior

- [x] Keyboard `PressRelease`：
  - 输入 `Pressed` 输出 `SKeyboardAction::bPressed = true`
  - 输入 `Released` 输出 `SKeyboardAction::bPressed = false`
  - Key 从规则配置中的 `SKeyboardAction::Key` 复制
- [x] MouseButton `PressRelease`：
  - 输入 `Pressed` 输出 `SMouseButtonAction::bPressed = true`
  - 输入 `Released` 输出 `SMouseButtonAction::bPressed = false`
  - Button 从规则配置中的 `SMouseButtonAction::Button` 复制
- [x] Trigger / Axis1D 到 Keyboard 或 MouseButton：
  - `Value >= Threshold` 输出 pressed action
  - `Value < Threshold` 输出 released action
  - 这是 stateless 连续判断，可能重复输出同一 pressed/released 状态，后续 Runtime 再做去重
- [x] Axis2D 到 MouseMove：
  - 只支持 `EActionType::MouseMove`
  - 输出 `DeltaX = Event.Axis2D.X * Rule.Output.Sensitivity`
  - 输出 `DeltaY = Event.Axis2D.Y * Rule.Output.Sensitivity`
  - deadzone 内返回空
- [x] `EMappingActionMode::Hold` 本轮暂不实现，遇到时返回空；后续需要 Runtime 状态配合。
- [x] `EActionType::MouseWheel` 本轮暂不实现，遇到时返回空。

## CMake Plan

- [x] 将 `MappyZCore` 从 `INTERFACE` library 调整为 `STATIC` library。
- [x] `MappyZCore` 源文件包含 `source/Core/MappingEngine.cpp`。
- [x] `MappyZCore` 继续 public 暴露 `${CMAKE_CURRENT_SOURCE_DIR}/source` include path。
- [x] `MappyZCore` 继续 public 链接 `Zero::ZeroStyle`。
- [x] 为 `MappyZCore` 添加与其他 C++ target 一致的 warning 选项。
- [x] 新增 `tests/Core/MappingEngineTests.cpp`，加入 `MappyZCoreTests`。
- [x] 不新增第三方依赖。
- [x] 不修改主应用 QML 或 Runtime 代码。

## Tests

- [x] 空 profile 返回空 action 列表。
- [x] disabled profile 返回空 action 列表。
- [x] disabled rule 不参与映射。
- [x] control id 不匹配时返回空。
- [x] control type 不匹配时返回空。
- [x] Button -> Keyboard 在 Pressed 时输出 pressed key action。
- [x] Button -> Keyboard 在 Released 时输出 released key action。
- [x] Button -> MouseButton 在 Pressed / Released 时输出对应 mouse button 状态。
- [x] 多条匹配规则按 profile 顺序输出多个 action。
- [x] Trigger below threshold 输出 released action。
- [x] Trigger above threshold 输出 pressed action。
- [x] Axis2D deadzone 内不输出 MouseMove。
- [x] Axis2D deadzone 外输出按 sensitivity 缩放的 MouseMove。
- [x] action type 和 payload 不匹配时跳过规则且不崩溃。
- [x] `Hold` mode 本轮返回空。
- [x] `MouseWheel` 本轮返回空。
- [x] `MappingEngine.h` 可单独 include 编译。
- [x] Core 新文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增文本文件使用 CRLF 行尾。
- [x] `ZMappingEngine` 不依赖 Runtime、Backend、UI 或平台层。
- [x] `MappyZCoreTests` 覆盖本轮最小映射行为。

## Follow-Up Module

- [ ] 下一模块建议实现 `Backends/Output/OutputBackend.h` 和 `ZNullOutputBackend`，让动作输出可以被 Runtime 测试记录。
- [ ] 再下一步实现 `Runtime/ActionDispatcher.h/.cpp`，将 `SAction` 派发给当前输出后端。
- [ ] 后续实现 `Runtime/MappingSession.h/.cpp`，串联 `ZInputRuntime`、`ZMappingEngine` 和 `ZActionDispatcher`。
