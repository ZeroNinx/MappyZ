# TODO: Stick Direction Inputs And Expanded Action Catalog

## Goal

让左摇杆、右摇杆先以 4 个方向虚拟输入参与映射，并补齐当前键盘、鼠标选择器里缺失但常用的输出按钮。

本轮目标是把"可绑定输入"和"可选输出"补到更实用的程度；不做完整模拟鼠标移动，不做 8 向摇杆，不做高级参数页。

## Scope

包含：

- [x] 将 Left Stick / Right Stick 各拆成 4 个方向虚拟输入：
  - [x] `left_stick_up`
  - [x] `left_stick_down`
  - [x] `left_stick_left`
  - [x] `left_stick_right`
  - [x] `right_stick_up`
  - [x] `right_stick_down`
  - [x] `right_stick_left`
  - [x] `right_stick_right`
- [x] 让这些虚拟输入可以像普通按钮一样绑定 Keyboard / MouseButton 输出。
- [x] 扩展 Keyboard action catalog 和 Keyboard picker 可用按键。
- [x] 扩展 Mouse action catalog 和 Mouse picker 可用按钮。
- [x] 保持现有 Axis2D 原始状态显示，不破坏 GamepadView 的摇杆高亮/数值显示。
- [x] 摇杆方向输入线（P1-P3）和 Keyboard/Mouse catalog 扩展线（P4-P5）彼此独立，可并行实现；实际执行时按风险和依赖选择顺序。

不包含：

- 不做 8 向摇杆。
- 不做摇杆到连续 MouseMove 的完整模拟。
- 不做 deadzone / sensitivity / curve / invert 配置 UI。
- 不做组合键，例如 Ctrl+Shift+A。
- 不做宏、连发、长按、toggle。
- 不做 DInput 输出功能。

## Priority 0: Design Contract

输入语义：

- [x] Stick direction 是从 Axis2D 派生出的虚拟 button-like input。
- [x] Direction threshold 暂定 `0.5`，超过阈值视为 pressed，回落到阈值内视为 released。
- [x] X/Y 轴互相独立：左摇杆右推只影响 `left_stick_right`，不影响 `left_stick_up/down`。
- [x] Y 轴方向约定固定为 SDL/gamepad 常见屏幕坐标：`Y > 0.5` -> `*_down`，`Y < -0.5` -> `*_up`。
- [x] 同一轴正负方向互斥：`left_stick_left` 和 `left_stick_right` 不应同时 pressed。
- [x] 对角输入允许两个方向同时 pressed，例如 up + right。
- [x] 方向虚拟输入产生的事件应进入现有 capture / mapping / UI state 流程，而不是只在 QML 里显示。

命名：

- [x] 使用 snake_case control id，和现有 `button_south` / `left_trigger` 风格一致。
- [x] 当前 mappings 中先显示 raw control id；展示美化可后续独立做。

架构约束：

- [x] 不在 QML 中临时推导 stick direction；方向拆分应发生在 runtime/input bridge 层，保证 capture、profile、mapping engine 都看到同一套输入。
- [x] 保留原始 Axis2D state，用于 GamepadView 摇杆显示。
- [x] 方向虚拟输入应有测试覆盖，避免后续改 SDL axis 合并逻辑时破坏映射。

## Priority 1: Stick Direction Input Pipeline

需要先定位现有 Axis2D 流程：

- [x] SDL backend / runtime 当前如何把 X/Y 合并成 `SInputEvent::Axis2D`。
- [x] `InputRuntime` 如何记录 Axis2D state。
- [x] `InputCaptureModel::IsCaptureWorthyInput()` 当前如何处理 Axis2D。
- [x] `MappingEngine` 当前如何处理 Axis2D -> MouseMove。
- [x] `InputStateModel` 当前如何暴露 Axis2D 给 QML。

实现方案：

- [x] 新增 stick direction 派生逻辑，输入为 Axis2D event，输出为 0-4 个 button-like `SInputEvent`。
- [x] 派生事件复用现有 Button 控件类型：`ControlType = Button`，`ControlId = left_stick_up` 等；本轮不新增 `EInputControlType::VirtualButton`。
- [x] 每个方向保存上一帧 pressed 状态，只在状态变化时发出 press/release，避免每帧重复刷 mapping。
- [x] 初始状态默认全部 released。
- [x] Clear / device removed 时清理对应 device 的 stick direction cache。
- [x] 原始 Axis2D event 仍继续进入 `InputStateModel`，但 mapping/capture 可消费派生方向事件。

建议落点：

- [x] 优先在 Runtime 层增加一个小型 `ZStickDirectionSynthesizer`，避免把方向状态散落在 UI 或 backend。
- [x] `ZRuntimeEventPump` 或 `ZMappingSession` 处理 input event 前，先将 Axis2D 展开为原始事件 + 派生事件队列。
- [x] 保持 FIFO：原始 Axis2D state 更新先发生，随后方向 press/release 进入 capture/mapping。

Tests：

- [x] Axis2D `(0, 0)` 不产生方向 press。
- [x] Axis2D `(0.6, 0)` 产生 `*_right pressed`。
- [x] Axis2D `(0.4, 0)` 从 pressed 回落时产生 `*_right released`。
- [x] 序列测试：先发 `(0.6, 0)` -> `*_right pressed`，再发 `(-0.6, 0)` -> `*_left pressed` + `*_right released`。
- [x] Axis2D `(0, -0.6)` 产生 `*_up pressed`。
- [x] Axis2D `(0, 0.6)` 产生 `*_down pressed`。
- [x] Axis2D `(0.7, -0.7)` 可同时产生 right + up。
- [x] 多设备 stick direction cache 互不影响。
- [x] Clear / device removed 清理 direction cache。

## Priority 2: Capture And UI State Integration

Capture：

- [x] Capture Input 时，摇杆超过阈值应捕获对应 direction control id，而不是捕获粗粒度 `left_stick` / `right_stick` Axis2D。
- [x] 摇杆轻微漂移低于阈值不应完成 capture。
- [x] 捕获完成后 selectedControl 应显示 direction control id，例如 `left_stick_up`。

InputStateModel：

- [x] direction press/release 更新 `PressedRole`。
- [x] `latestControlId(deviceId)` 能返回最近触发的 direction control id。
- [x] QML `isPressed(deviceId, "left_stick_up")` 可用。

GamepadView：

- [x] 左/右摇杆 UI 可在四个方向上显示 pressed/highlight 状态。
- [x] 不破坏原有 LS/RS 或轴值显示。
- [x] 本轮不强制拆分摇杆 dot 的 4 个点击区域；点击摇杆 dot 仍可选择粗粒度 stick 或进入 capture，用户通过推摇杆方向完成 direction capture。

Tests：

- [x] Capture stick direction 成功。
- [x] Stick drift 不抢占 capture。
- [x] InputStateModel direction 状态可查询。
- [x] GamepadView 相关 QML smoke 不报 warning。

## Priority 3: Mapping Engine / Profile Integration

Mapping profile：

- [x] `SMappingInput` 可表达 stick direction control id。
- [x] 方向输入按 button-like PressRelease 规则映射 Keyboard / MouseButton。
- [x] 保存 profile 后 direction 规则可序列化。
- [x] 加载 profile 后 direction 规则可恢复。

Mapping behavior：

- [x] Stick direction pressed 输出 key/mouse pressed action。
- [x] Stick direction released 输出 key/mouse released action。
- [x] 同方向重复 pressed 状态不重复 dispatch。
- [x] 对角方向可同时 dispatch 两个输出。

Tests：

- [x] `left_stick_up -> Keyboard W` press/release。
- [x] `left_stick_left -> Keyboard A` press/release。
- [x] `right_stick_right -> MouseButton Right` press/release。
- [x] profile save/load round trip preserves direction rules。

## Priority 4: Expanded Keyboard Catalog

当前 Keyboard picker 里大量键位显示为 disabled。下一步优先补常用键，不一次性追求完整国际键盘。

新增 Keyboard actions：

- [x] Function keys：F1-F12。
- [x] Editing/navigation：Backspace、Delete、Insert、Home、End、PageUp、PageDown。
- [x] Modifiers：LeftShift、RightShift、LeftCtrl、RightCtrl、LeftAlt、RightAlt、LeftMeta/Win。
- [x] Symbols for US layout：Minus、Equal、LeftBracket、RightBracket、Backslash、Semicolon、Apostrophe、Comma、Period、Slash、Backquote。
- [x] Numpad：Num0-Num9、NumDivide、NumMultiply、NumSubtract、NumAdd、NumDecimal。

实现要求：

- [x] 扩展 `ActionCatalogModel`，为每个 key 定义 stable value 和 display text。
- [x] `applySelectedBinding(controlId, "Keyboard", value)` 支持新增 value。
- [x] `KeyboardPicker.handleQtKey()` 尽量识别新增 Qt key。
- [x] Picker 中对应 key 从 disabled 变 enabled。
- [x] 小键盘必须和主键区数字分开：主键区保持 `"0"`-`"9"`，小键盘使用 `"Num0"`-`"Num9"` 等独立 value。
- [x] KeyboardPicker 小键盘区域点击 `7` 应提交 `"Num7"`，不能继续提交主键区 `"7"`。
- [x] display text 保持用户可读，例如 `Keyboard: Backspace`、`Keyboard: F5`。

Tests：

- [x] ActionCatalogModel 包含新增 key。
- [x] applySelectedBinding 支持至少 Backspace / F5 / LeftShift / Minus / Num1。
- [x] 主键区 `1` 和小键盘 `Num1` 生成不同 action value。
- [x] MappingRuleModel output/actionValue 正确显示新增 key。
- [x] QML smoke 通过。

## Priority 5: Expanded Mouse Catalog

新增 Mouse actions：

- [x] MouseButton Button4 / Button5。
- [ ] MouseWheel Up / Down（当前 PressRelease 管线无持续滚动语义，暂不启用）。
- 可选：MouseWheel Left / Right，如果底层 action 已支持或容易扩展。

实现要求：

- [x] 扩展 `ActionCatalogModel`：`MouseButton / Button4`、`MouseButton / Button5`。
- [x] `applySelectedBinding()` 支持新增 mouse values。
- [x] `SAction` / `MappingEngine` / `OutputBackend` 当前 MouseWheel 类型已有但 PressRelease 语义不完整，先只启用 Button4/Button5。
- [x] MousePicker 中 Button4/Button5 变 enabled。
- [ ] Wheel Up/Down 只有在完整 dispatch 语义打通后才能 enabled。

Tests：

- [x] Button4/Button5 可创建 mapping rule。
- [x] MousePicker 点击 Button4/Button5 可 Confirm。
- [ ] 如果启用 MouseWheel，则 MappingEngine 和 OutputBackend tests 覆盖 wheel action。
- [x] 未完成的 MouseMove 仍不可新建。

## Priority 6: Verification

- [x] `git diff --check`
- [x] `cmake --build build --config Debug`
- [x] `.\build\Debug\MappyZQmlSmokeTests.exe`
- [x] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] 手动验证：
  - [ ] Capture 左摇杆上方向，绑定到 `W`。
  - [ ] Capture 左摇杆左方向，绑定到 `A`。
  - [ ] Capture 右摇杆右方向，绑定到鼠标右键。
  - [ ] 保存 profile，重启或 reload 后规则仍存在。
  - [ ] F1-F12 / Backspace / Delete / Button4 / Button5 可在 picker 中选择。
