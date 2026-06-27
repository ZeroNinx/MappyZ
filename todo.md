# TODO: Analog Mouse Move Binding

## Goal

把已经存在于 Core / Runtime / Windows output backend 的 `Axis2D -> MouseMove` 能力接到 UI。

当前用户可以绑定按钮、扳机、方向键到键盘或鼠标点击，但 `left_stick` / `right_stick` 在 `InferInputFromControlId()` 中被显式拒绝，`ActionCatalogModel` 也没有 `MouseMove` 项。结果是 v0.1 目标中的“摇杆映射鼠标移动”和“基础摇杆死区”没有可用入口。

本轮只做一条稳定闭环：选择摇杆输入 -> 选择 Mouse Move -> Apply -> running runtime 中产生鼠标移动 action。

## Scope

包含：

- [x] `ActionCatalogModel` 新增 `MouseMove` action。
- [x] `ZAppController::applySelectedBinding()` 支持 `actionKind == "MouseMove"`。
- [x] `InferInputFromControlId()` 支持 `left_stick` / `right_stick` 作为 `Axis2D`。
- [x] `Axis2D -> MouseMove` 规则使用 `EMappingActionMode::Analog`。
- [x] 设置基础 deadzone 和 sensitivity 默认值。
- [x] `MappingRuleModel` 能展示 MouseMove 规则。
- [x] BindingEditor 能选择并回填 MouseMove action。
- [x] 增加 AppController / ActionCatalog / MappingRuleModel 测试。
- [x] QML smoke 覆盖新 action，无 binding/import/property warning。

不做：

- [ ] 不做 sensitivity / deadzone UI slider。
- [ ] 不做 per-axis 反转。
- [ ] 不做 response curve。
- [ ] 不做 MouseWheel。
- [ ] 不做 Shift / layer / mode switching。
- [ ] 不做多个 action 绑定到同一个 input。

## Constants

本轮使用保守默认值，后续再做可配置：

- [x] `Axis2D` deadzone 默认 `0.20f`。
- [x] `Axis2D -> MouseMove` sensitivity 默认 `12.0f`。
- [x] `MouseMove` action payload 存储为 `SMouseMoveAction{0.0f, 0.0f}`；实际 delta 由 `ZMappingEngine::BuildAnalogAction()` 根据输入轴值实时生成。

理由：

- deadzone 不能为 0，否则手柄轻微漂移会导致鼠标持续移动。
- sensitivity 不能沿用 `1.0f`，否则实际移动太弱，用户会误以为没有生效。
- payload 中不存固定 delta，避免把“配置动作”和“运行时输出”混在一起。

## Action Catalog Plan

`ZActionCatalogModel` 新增一项：

- [x] `Kind = "MouseMove"`。
- [x] `Value = "Cursor"`。
- [x] `DisplayText = "Mouse: Move Cursor"`。
- [x] `Category = "Mouse"`。

行为：

- [x] `Contains("MouseMove", "Cursor")` 返回 true。
- [x] `findIndex("MouseMove", "Cursor")` 返回有效行。
- [x] 现有 Keyboard / MouseButton 项顺序不做大改；MouseMove 放在 MouseButton 三项之后。

## AppController Plan

`applySelectedBinding(controlId, actionKind, actionValue)` 新增 MouseMove 分支：

- [x] `actionKind == "MouseMove"` 时必须通过 `ActionCatalogModelInstance.Contains(actionKind, actionValue)` 校验。
- [x] 仅允许 `left_stick` / `right_stick` 绑定到 `MouseMove`。
- [x] 如果非 Axis2D 输入尝试绑定 MouseMove，返回 false，`EmitRuntimeError("Apply failed: MouseMove requires stick input")`。
- [x] `SAction.Type = EActionType::MouseMove`。
- [x] `SAction.Payload = SMouseMoveAction{.DeltaX = 0.0f, .DeltaY = 0.0f}`。
- [x] 规则 `Output.Mode = EMappingActionMode::Analog`。
- [x] 规则 `Output.Sensitivity = 12.0f`。
- [x] 规则 `Input.ControlType = EInputControlType::Axis2D`。
- [x] 规则 `Input.EventType = EInputEventType::Changed`。
- [x] 规则 `Input.Deadzone = 0.20f`。
- [x] 规则 `Input.Threshold = 0.0f`。

`InferInputFromControlId()` 调整：

- [x] `left_stick` / `right_stick` 返回 true。
- [x] 对这两个控件填充 `Axis2D` / `Changed` / deadzone `0.20f` / threshold `0.0f`。
- [x] 其他按钮、扳机、方向键行为不变。

兼容规则：

- [x] Keyboard / MouseButton 仍使用 `PressRelease`。
- [x] Button / Trigger 不允许绑定到 MouseMove，本轮不做”扳机控制鼠标移动”这类高级用法。
- [x] Axis2D 暂不允许绑定 Keyboard / MouseButton，避免生成 Core 当前不支持的规则。

## MappingRuleModel Plan

MouseMove 展示规则：

- [x] `actionKind` 返回 `"MouseMove"`。
- [x] `displayKind` 返回 `"Mouse"`。
- [x] `actionValue` 返回 `"Cursor"`。
- [x] `output` 返回 `"Move Cursor"`。
- [x] `ruleEnabled` 行为不变。

这样 Current mappings 中显示为：

```text
left_stick -> Move Cursor    Mouse    On
```

## QML Plan

BindingEditor 不新增复杂控件，只复用现有 action picker：

- [x] action picker 显示 `Mouse: Move Cursor`。
- [x] 选中 MouseMove 后 `Apply` 调用 `applySelectedBinding(selectedControl, “MouseMove”, “Cursor”)`。
- [x] 点击已有 MouseMove mapping 行时，回填 action picker 到 `MouseMove / Cursor`。
- [x] 如果 selectedControl 不是 stick，Apply 后端会拒绝并显示现有 `Apply failed` inline feedback。
- [x] 不在 QML 中复制”哪些输入能绑定 MouseMove”的业务规则；业务规则留在 AppController。

可选 UI polish，不作为阻断：

- [ ] 当 selectedControl 明显不是 `left_stick` / `right_stick` 且 action 是 MouseMove 时，可显示轻量 hint：`Mouse Move requires a stick input`。

## Runtime Behavior

期望数据流：

```text
SInputEvent{ControlId="left_stick", ControlType=Axis2D, EventType=Changed, Axis2D={X,Y}}
  -> ZMappingEngine::BuildAnalogAction()
  -> SAction{Type=MouseMove, Payload={DeltaX=X*12, DeltaY=Y*12}}
  -> ZActionDispatcher
  -> IOutputBackend
```

验收行为：

- [x] Axis magnitude 小于等于 `0.20` 时不产生 MouseMove action。
- [x] Axis magnitude 大于 `0.20` 时产生 MouseMove action。
- [x] 全局 Remap Paused 时不 dispatch。
- [x] 单条 rule disabled 时不 dispatch。
- [x] 保存 / 加载后 MouseMove 规则保持 `Analog`、deadzone、sensitivity、enabled 状态。

## Tests

`ActionCatalogModelTests.cpp`：

- [x] catalog contains `MouseMove / Cursor`。
- [x] display text 为 `Mouse: Move Cursor`。
- [x] category 为 `Mouse`。
- [x] `findIndex("MouseMove", "Cursor")` 有效。

`AppControllerTests.cpp`：

- [x] `applySelectedBinding("left_stick", "MouseMove", "Cursor")` 成功。
- [x] 成功后 `MappingRuleModel` row 显示 `actionKind == "MouseMove"`、`actionValue == "Cursor"`、`output == "Move Cursor"`。
- [x] 生成的 RuntimeHost profile rule 使用 `Input.ControlType == Axis2D`。
- [x] 生成的 RuntimeHost profile rule 使用 `Output.Mode == Analog`。
- [x] 生成的 RuntimeHost profile rule 使用 deadzone `0.20f`、sensitivity `12.0f`。
- [x] `applySelectedBinding("right_stick", "MouseMove", "Cursor")` 成功。
- [x] `applySelectedBinding("button_south", "MouseMove", "Cursor")` 返回 false 并 emit `runtimeError`。
- [x] `applySelectedBinding("left_stick", "Keyboard", "Space")` 返回 false，避免生成 Core 不支持的 Axis2D PressRelease 规则。
- [x] MouseMove rule 保存 / 加载回环后仍保留 mode、deadzone、sensitivity。

`MappingRuleModelTests.cpp`：

- [x] MouseMove output role 返回 `Move Cursor`。
- [x] MouseMove actionValue role 返回 `Cursor`。
- [x] MouseMove displayKind role 返回 `Mouse`。

Runtime integration test：

- [x] 用 fake input + null output 创建 left_stick MouseMove 规则。
- [x] 注入 deadzone 内 Axis2D 事件，`lastDispatchedInputCount` 不增加。
- [x] 注入 deadzone 外 Axis2D 事件，`lastDispatchedInputCount` 增加。
- [x] 禁用该 rule 后再次注入 Axis2D 事件，不再 dispatch。

QML / smoke：

- [x] QML smoke passes。
- [x] no binding/import/property warning。

## Acceptance Criteria

- [x] 用户能在 Action output 中选择 `Mouse: Move Cursor`。
- [x] 用户能把 `left_stick` 或 `right_stick` 绑定到鼠标移动。
- [x] 绑定后 Current mappings 明确显示 MouseMove 规则。
- [x] 摇杆 deadzone 内不会移动鼠标。
- [x] 摇杆 deadzone 外会通过现有 dispatch 链路产生 MouseMove action。
- [x] 保存 / 重启 / 加载后 MouseMove 规则仍存在并可用。
- [x] 现有 Keyboard / MouseButton 映射不回退。
- [x] 所有测试通过。
- [x] 修改文本文件保持 CRLF 行尾。

## Follow-Up

- [ ] 做 MouseMove sensitivity / deadzone UI。
- [ ] 做 Y-axis invert。
- [ ] 做 response curve。
- [ ] 做 profile management：rename / duplicate / delete profile。
- [ ] 做 Save As / Import / Export。
