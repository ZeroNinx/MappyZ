# TODO: Mapping Rule Enable Disable UX

## Goal

让用户能管理已经保存的映射规则，而不是只能新增、替换或删除。当前 `MappingRuleModel` 已经暴露 `enabled` role，Core profile 也有 `SMappingRule::bEnabled`，下一步只补 UI Bridge 和 QML 操作入口。

本轮目标是小范围打通"禁用某条绑定但保留配置"的能力，并保持 running runtime、autosave、MappingRuleModel 三者一致。

## Scope

包含：

- [x] `ZAppController` 新增 `setBindingEnabled(ruleId, enabled)`。
- [x] 修改 active profile 中对应 `SMappingRule::bEnabled`。
- [x] 替换 RuntimeHost active profile，让 running runtime 立即生效。
- [x] 刷新 `MappingRuleModel`，让 QML 当前列表同步。
- [x] 复用现有 dirty/autosave 机制，成功后自动保存。
- [x] BindingEditor 当前映射列表增加启用/禁用入口。
- [x] 禁用规则在 UI 中有明确视觉状态。
- [x] 增加 AppController 测试和 QML smoke 覆盖。

不做：

- [ ] 不做批量启用/禁用。
- [ ] 不做规则拖拽排序。
- [ ] 不做规则重命名。
- [ ] 不做高级条件、分组、层级 profile。
- [ ] 不改 MappingEngine 行为；继续依赖已有 disabled rule skip 逻辑。

## API Plan

`ZAppController` 新增 QML API：

```cpp
Q_INVOKABLE bool setBindingEnabled(QString ruleId, bool enabled);
```

行为：

- [x] `ruleId` 为空时返回 false，写 Error log，emit `runtimeError`。
- [x] runtime 未 initialize 时返回 false，写 Error log，emit `runtimeError`。
- [x] 找不到 rule 时返回 false，写 Error log，emit `runtimeError`。
- [x] rule 当前状态已经等于目标状态时返回 true，但不标 dirty、不 autosave、不重复写成功日志。
- [x] 状态实际变化时：
  - [x] 更新 profile snapshot 中的 `Rule.bEnabled`。
  - [x] `RuntimeHost.ReplaceProfile(Profile)`。
  - [x] `RefreshMappingRuleModelFromHost()`。
  - [x] `MarkProfileDirty()`。
  - [x] `AutosaveActiveProfile()`。
  - [x] 写 Success log：`Binding enabled: <ruleId>` / `Binding disabled: <ruleId>`，保存失败时使用 `..., save failed` 文案。

## QML Plan

`BindingEditor.qml` 当前 mapping delegate 已有 `ruleEnabled` role（从 `enabled` 改名避免 Item.enabled 冲突）。新增交互：

- [x] 每条 mapping 右侧增加一个小 toggle（On/Off 按钮）。
- [x] toggle 文案使用 `On` / `Off`。
- [x] 点击 toggle 调用 `appController.setBindingEnabled(ruleId, !ruleEnabled)`。
- [x] toggle 成功后显示 inline feedback：
  - [x] autosave 成功：`Enabled and saved: <input>` / `Disabled and saved: <input>`。
  - [x] autosave 失败：`Enabled, save failed: <input>` / `Disabled, save failed: <input>`。
- [x] disabled rule 的 mapping 行降低 opacity，仍保留删除入口。
- [x] disabled rule 点击行仍能回填 selected control/action，方便重新编辑。
- [x] 删除按钮继续工作，不改变现有 `removeBinding()` 语义。

## Data Flow

- [x] `MappingRuleModel::EnabledRole` 继续作为 QML 数据源，role name 改为 `ruleEnabled` 避免 QML 属性冲突。
- [x] `setBindingEnabled()` 修改 profile 后通过 `ReplaceProfile()` 和 `RefreshMappingRuleModelFromHost()` 同步 model。
- [x] autosave 成功后 profile 状态回到 clean。
- [x] autosave 失败后 runtime 中的启用/禁用仍立即生效，profile 状态显示 save error。
- [x] `Remap Active/Paused` 是全局 dispatch 开关；单条 rule enabled 是局部规则开关，两者互不替代。

## Tests

`AppControllerTests.cpp`：

- [x] `setBindingEnabled` returns false for empty ruleId。
- [x] `setBindingEnabled` returns false before initialize。
- [x] `setBindingEnabled` returns false for unknown ruleId。
- [x] disabling existing rule updates `MappingRuleModel.ruleEnabled` to false。
- [x] enabling disabled rule updates `MappingRuleModel.ruleEnabled` to true。
- [x] repeated set to same state is no-op and does not emit `profileStatusChanged`。
- [x] disabled rule persists through `saveActiveProfile()` / `loadProfile()` round trip。
- [ ] disabled rule is not dispatched while enabled sibling rules still work, if existing test infrastructure can assert this cheaply。
- [x] autosave failure keeps runtime state changed and profile save state error。

QML / smoke：

- [x] QML smoke passes with new delegate controls.
- [x] no binding/import/property warning.

## Acceptance Criteria

- [x] 用户可以在 Current mappings 中禁用一条绑定而不删除它。
- [x] 禁用后该规则不再产生 mapped output。
- [x] 再次启用后该规则恢复生效。
- [x] 禁用/启用会自动保存；保存失败时 UI 明确显示 save error。
- [x] 全局 Remap Paused 与单条 Disabled 状态在 UI 上可区分。
- [x] 所有测试通过。
- [x] 修改文本文件保持 CRLF 行尾。

## Follow-Up

- [ ] 后续做 rule rename / display name。
- [ ] 后续做 drag reorder。
- [ ] 后续做 profile management：rename / duplicate / delete profile。
- [ ] 后续做 Save As / Import / Export。
