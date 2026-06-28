# TODO: Mouse Wheel Mapping

## Goal

让用户可以在映射选择器中选择鼠标滚轮上/下，并把手柄按钮、D-Pad、摇杆方向等 button-like 输入绑定为离散滚轮动作。

本轮只打通垂直滚轮 `MouseWheel Up / Down`。不做持续滚动、不做横向滚轮、不做滚动速度/重复频率配置。

## Scope

包含：

- [ ] 在 action catalog 中启用 `MouseWheel / Up` 和 `MouseWheel / Down`。
- [ ] 在 Mouse picker 中启用滚轮上/下 tile。
- [ ] `applySelectedBinding(controlId, "MouseWheel", value)` 可创建滚轮规则。
- [ ] MappingEngine 支持 button-like 输入触发 MouseWheel 输出。
- [ ] Windows SendInput 后端滚轮路径验证补齐。
- [ ] Current mappings 能显示滚轮规则。
- [ ] Profile save/load 保留滚轮规则。

不包含：

- 横向滚轮 `Wheel Left / Wheel Right`。
- 长按持续滚动、repeat rate、acceleration。
- 摇杆连续模拟滚轮。
- 鼠标移动参数页。
- DInput 输出。

## Priority 0: Semantics

设计决策：

- [ ] MouseWheel 是离散 pulse 输出，不是 pressed/released 状态。
- [ ] `MouseWheel Up` 使用 `SMouseWheelAction{ Delta = 1.0f }`。
- [ ] `MouseWheel Down` 使用 `SMouseWheelAction{ Delta = -1.0f }`。
- [ ] PressRelease 规则遇到 MouseWheel 时只在输入激活事件上输出一次滚轮 action。
- [ ] Button / Hat / stick direction：只在 `Pressed` 事件输出滚轮 action，`Released` 不输出任何 action。
- [ ] Trigger / Axis1D 暂不作为 MouseWheel 的推荐输入；如果规则存在，先沿用现有阈值判断，但测试必须避免把它当作持续滚动能力。
- [ ] 不新增新的 `EMappingActionMode`，避免本轮扩大 profile schema 和 UI 编辑范围。

风险说明：

- [ ] 当前 Runtime 没有 repeat scheduler，因此长按按钮不会持续滚动。
- [ ] 如果未来要做长按持续滚动，应新增 Runtime 层 repeat/turbo 机制，而不是让 MappingEngine 在每帧重复输出。

## Priority 1: Core Mapping

实现：

- [ ] 修改 `ZMappingEngine::BuildPressReleaseAction()`：
  - [ ] `EActionType::MouseWheel` 校验 payload 为 `SMouseWheelAction`。
  - [ ] 输入激活时返回 MouseWheel action，保留配置中的 `Delta`。
  - [ ] 输入未激活时返回 `std::nullopt`，避免 release 事件产生反向或零滚动。
- [ ] 保持 `Analog` 模式不支持 MouseWheel。
- [ ] 保持 payload mismatch 路径输出错误日志并安全跳过。

Tests：

- [ ] Button Pressed -> MouseWheel Up action。
- [ ] Button Released -> no action。
- [ ] Stick direction Pressed -> MouseWheel Down action。
- [ ] Payload mismatch skips rule。
- [ ] Disabled rule/profile 仍跳过。

## Priority 2: AppController And Catalog

实现：

- [ ] `ActionCatalogModel` 增加：
  - [ ] `Kind = "MouseWheel", Value = "Up", Display = "Mouse: Wheel Up"`。
  - [ ] `Kind = "MouseWheel", Value = "Down", Display = "Mouse: Wheel Down"`。
- [ ] `ZAppController::applySelectedBinding()` 支持：
  - [ ] `"MouseWheel" / "Up"` -> `EActionType::MouseWheel`, `Delta = 1.0f`。
  - [ ] `"MouseWheel" / "Down"` -> `EActionType::MouseWheel`, `Delta = -1.0f`。
  - [ ] 未知 value 返回 false 并写入 runtimeError/log。
- [ ] `MappingRuleModel` 如已有 MouseWheel 显示逻辑则只补测试；如显示不够明确则输出 `Wheel Up` / `Wheel Down`。

Tests：

- [ ] ActionCatalogModel contains `MouseWheel/Up` and `MouseWheel/Down`。
- [ ] applySelectedBinding 可创建 Wheel Up 规则。
- [ ] applySelectedBinding 可创建 Wheel Down 规则。
- [ ] unknown MouseWheel value returns false。
- [ ] MappingRuleModel output/actionKind/actionValue 展示正确。

## Priority 3: Output Backend Verification

现状：

- `SMouseWheelAction`、ProfileManager 序列化、Windows SendInput command 基础路径已存在。

实现：

- [ ] 确认 `BuildCommandFromAction()` 对 `MouseWheel` 的 delta 转换为 `WheelDeltaUnit` 倍数。
- [ ] 确认 `WindowsSendInputBackend` 将 wheel delta 写入 `INPUT.mi.mouseData`。
- [ ] `Delta == 0` 保持 no-op success。

Tests：

- [ ] `Delta = 1.0f` -> `WheelDelta = +120`。
- [ ] `Delta = -1.0f` -> `WheelDelta = -120`。
- [ ] `Delta = 0.0f` no-op success。
- [ ] payload mismatch returns error。

## Priority 4: Mouse Picker UI

实现：

- [ ] Mouse picker 中滚轮上/下 tile 从 disabled 改为 enabled。
- [ ] 点击滚轮上设置 pending action：`kind = "MouseWheel", value = "Up"`。
- [ ] 点击滚轮下设置 pending action：`kind = "MouseWheel", value = "Down"`。
- [ ] Confirm 后 BindingEditor selected action 显示 `Mouse: Wheel Up/Down`。
- [ ] Button4/Button5、Left/Right/Middle 现有行为不变。
- [ ] Mouse movement tile 继续 disabled，不恢复 MouseMove 新建入口。

Tests / Verification：

- [ ] QML smoke 无 warning。
- [ ] 手动验证滚轮上/下 tile 可选中并 Confirm。
- [ ] 绑定保存后 Current mappings 显示滚轮规则。

## Priority 5: Profile Round Trip

Tests：

- [ ] 保存包含 MouseWheel Up 的 profile 后 JSON 包含 `mouse_wheel.delta = 1.0`。
- [ ] 保存包含 MouseWheel Down 的 profile 后 JSON 包含 `mouse_wheel.delta = -1.0`。
- [ ] 加载 profile 后 action type 和 delta 保持一致。
- [ ] AppController apply + autosave 后重新 load 能恢复滚轮规则。

## Priority 6: Verification

- [ ] `git diff --check`
- [ ] CRLF line ending check
- [ ] `cmake --build build --config Debug`
- [ ] `.\build\Debug\MappyZQmlSmokeTests.exe`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] 手动验证：
  - [ ] Capture `button_south`，绑定 `Mouse: Wheel Up`。
  - [ ] Capture `left_stick_up`，绑定 `Mouse: Wheel Down`。
  - [ ] 在真实输出模式下滚轮动作生效。
  - [ ] 保存 profile，重启或 reload 后滚轮规则仍存在。
