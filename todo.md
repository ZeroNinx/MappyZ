# TODO: Runtime Binding Rule Apply

## Next Step

下一步实现 Binding Editor 的真实运行期规则创建：用户选中输入控件和输出动作后，点击 Apply，把规则写入当前 RuntimeHost 的 active profile，并刷新 Current mappings 列表。

本轮只做内存 profile，不保存磁盘，不做完整 profile 编辑器。

## Scope

包含：

- [x] 新增 UI Bridge 映射规则列表模型，展示 active profile 中的规则。
- [x] `ZAppController` 暴露 `mappingRuleModel`。
- [x] `ZAppController` 增加 `applySelectedBinding(controlId, actionText)`。
- [x] Apply 成功后通过 `RuntimeHost::ReplaceProfile()` 更新运行期 profile。
- [x] Current mappings 从 QML demo `ListModel` 改为真实 `mappingRuleModel`。
- [x] 增加 UI Bridge 单元测试，验证规则写入和后续输入能被映射派发。

不做：

- [x] 不保存 profile 文件。
- [x] 不实现 action picker 或任意键位输入。
- [x] 不实现规则删除、规则禁用、拖拽排序。
- [x] 不改 Core/Runtime 的 mapping 数据契约。
- [x] 不支持 Axis2D -> MouseMove 的 UI 创建入口，本轮只保留后续扩展点。

## Behavior

- [x] `applySelectedBinding("", actionText)` 返回 false，发 `runtimeError`。
- [x] `applySelectedBinding(controlId, "")` 返回 false，发 `runtimeError`。
- [x] Runtime 未 initialize 时返回 false，发 `runtimeError`。
- [x] 支持 `Keyboard: Space` 输出：
  - action type 为 `KeyboardKey`。
  - key 为 `Space`。
  - mode 为 `PressRelease`。
- [x] 支持 `Mouse: Left Click` 输出：
  - action type 为 `MouseButton`。
  - button 为 0。
  - mode 为 `PressRelease`。
- [x] 输入类型按标准 control id 推断：
  - `button_*`、shoulder、stick button、start/back/guide -> Button。
  - `dpad_*` -> Hat。
  - `left_trigger` / `right_trigger` -> Trigger，事件类型为 Changed，阈值 0.5。
  - `left_stick` / `right_stick` 暂不支持创建，返回 false。
- [x] 同一个 `controlId` 再次 Apply 时替换旧规则，不追加重复规则。
- [x] Apply 后当前 mapping enabled 状态不变。
- [x] Apply 后 `mappingRuleModel` 立即刷新。
- [x] 规则 id 生成策略：本轮使用 `Rule.Id = controlId`，并按 `Input.ControlId` 替换已有规则，保证单个 profile 内当前范围唯一。

## Proposed Types

新增 `source/UI/Bridge/MappingRuleModel.h/.cpp`：

- [x] `class ZMappingRuleModel final : public QAbstractListModel`
- [x] Roles：
  - `ruleId`
  - `input`
  - `output`
  - `actionKind`
  - `enabled`
- [x] Role 语义：
  - `ruleId`：`SMappingRule::Id`，本轮等于原始 `controlId`。
  - `input`：原始 `SMappingRule::Input.ControlId`，例如 `button_south`；本轮不做显示名美化。
  - `output`：从 `SAction::Payload` 提取的用户可读输出值，例如 `Space` / `Left Click`。
  - `actionKind`：从 `SAction::Type` 推导的动作类别，例如 `Keyboard` / `Mouse`。
  - `enabled`：`SMappingRule::bEnabled`。
- [x] Public API：
  - `void ReplaceRules(TVector<SMappingRule> Rules)`
  - `Q_INVOKABLE void clear()`
  - `Q_INVOKABLE QString ruleIdAt(int row) const`
  - `TVector<SMappingRule> ListRulesSnapshot() const`

## AppController Integration

- [x] `Q_PROPERTY(ZMappingRuleModel* mappingRuleModel READ MappingRuleModel CONSTANT)`。
- [x] 初始化成功后用 host profile 刷新 model。
- [x] Error 重建失败路径不清空 mapping model；只有成功拿到新 profile 后替换。
- [x] Apply 时：
  - 取 `Bootstrap.GetRuntimeHost().GetProfileSnapshot()`。
  - 构造一条 UI 规则。
  - 设置 `Rule.Id = controlId`。
  - 按 `Input.ControlId` 替换或追加。
  - 调 `ReplaceProfile()`。
  - 刷新 `MappingRuleModel`。
  - 返回 true。

## QML Plan

- [x] `Main.qml` 删除 demo `mappingModel`。
- [x] `BindingEditor.qml` 不再接收 `mappingModel`。
- [x] Current mappings 的 Repeater 使用 `appController.mappingRuleModel`。
- [x] 增加 Apply 按钮，调用 `appController.applySelectedBinding(selectedControl, selectedAction)`。
- [x] Keyboard / Mouse 按钮仍只更新 `selectedAction`，不自动 Apply。

## Tests

- [x] `mappingRuleModel` property 非空。
- [x] initialize 后默认 mapping model 为空。
- [x] Apply Keyboard Space 成功后 model 有一条规则。
- [x] Apply Mouse Left Click 成功后 model 输出为 Left Click / Mouse。
- [x] `MappingRuleModel` 的 `input` role 返回原始 control id。
- [x] `MappingRuleModel` 的 `output` / `actionKind` roles 从 action payload 拆分，而不是直接返回 `Keyboard: Space` 这类 UI 选择字符串。
- [x] Apply 后生成的 `SMappingRule::Id` 等于 control id。
- [x] 空 control 或空 action 返回 false 并发 `runtimeError`。
- [x] Runtime 未 initialize 时 Apply 返回 false。
- [x] 同一 control 重复 Apply 替换旧规则，不增加 row count。
- [x] Apply 后 fake 输入同一 button，`pumpOnce()` 后 mapped/dispatched 计数为 1。
- [x] 不支持 Axis2D 创建时返回 false 且不修改 model。

## Acceptance Criteria

- [x] Current mappings 列表不再是硬编码 demo 数据。
- [x] Apply 后 profile snapshot 中存在对应 `SMappingRule`。
- [x] Apply 后真实输入能走 MappingEngine -> ActionDispatcher 链路。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。
