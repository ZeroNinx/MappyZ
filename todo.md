# TODO: Profile Save Snapshot

## Next Step

下一步实现“保存当前 active profile 快照”：用户点击 `Save Profile` 后，把当前 RuntimeHost 中的 `SMappingProfile` 序列化为 JSON profile 文件。

本轮只做保存，不做 profile 切换、不做启动自动加载、不做文件选择器。

## Scope

包含：

- [x] `ZAppController` 增加保存当前 profile 的 QML API。
- [x] 使用现有 `ZProfileManager::SaveProfile()` 完成 JSON 写入。
- [x] TopBar 的 `Save Profile` 按钮调用真实保存 API。
- [x] 删除 `notifySaveProfileNotImplemented()` 临时 API，替换为 `saveActiveProfile()`。
- [x] 保存成功或失败时给 QML 一个可观察的结果。
- [x] 增加 UI Bridge 单元测试覆盖保存链路。

不做：

- [ ] 不实现 profile 手动切换。
- [ ] 不实现启动时从 UI 指定路径加载 profile。
- [ ] 不实现文件选择对话框。
- [ ] 不引入 `ZProfileModel`。
- [ ] 不做 profile dirty 状态。
- [ ] 不保存 event log 或运行时输入状态。

## API Plan

`ZAppController` 新增：

- [x] `Q_PROPERTY(QString profilePath READ ProfilePath NOTIFY profileStatusChanged)`
- [x] `Q_PROPERTY(QString profileMessage READ ProfileMessage NOTIFY profileStatusChanged)`
- [x] `Q_INVOKABLE bool saveActiveProfile(QString profilePath = QString())`
- [x] `QString ProfilePath() const`
- [x] `QString ProfileMessage() const`
- [x] `void profileStatusChanged()`
- [x] `void profileSaved(QString profilePath)`

语义：

- [x] `profilePath` 参数为空时使用默认 profile 路径。
- [x] 默认路径通过 `QStandardPaths::AppDataLocation` 计算，并追加 `profiles/default.json`。
- [x] `profilePath` 参数非空时使用调用方传入路径，主要供测试和后续文件选择器使用。
- [x] `profilePath` 属性本轮先保留为 UI Bridge 状态契约；QML 暂不展示，但测试需要覆盖成功保存后值已更新。
- [x] 保存成功更新 `profilePath` 和 `profileMessage`，发 `profileStatusChanged()` 和 `profileSaved(profilePath)`。
- [x] `profileSaved(profilePath)` 保留为语义事件，供后续 Load/Profile UI 或外部 shell 订阅；不要只依赖通用 `profileStatusChanged()`。
- [x] 保存失败更新 `profileMessage`，发 `profileStatusChanged()` 和现有 `runtimeError(message)`。

## Behavior

- [x] Runtime 未 initialize 时 `saveActiveProfile()` 返回 false，不创建文件，发 `runtimeError()`。
- [x] Ready 或 Running 状态下允许保存。
- [x] 保存内容来自 `Bootstrap.GetRuntimeHost().GetProfileSnapshot()`，不从 `MappingRuleModel` 反推。
- [x] 保存不会修改当前 RuntimeHost profile。
- [x] 保存不会修改 mapping enabled 状态。
- [x] 空 profile 保存为合法 JSON，`mappings` 为空数组。
- [x] 已 Apply 的 binding 保存后，JSON 中包含对应 mapping rule。
- [x] 父目录不存在时由 `ZProfileManager::SaveProfile()` 创建。
- [x] 保存失败不清空 `MappingRuleModel`，不改变 Runtime 状态。

## QML Plan

- [x] `TopBar.qml` 的 `Save Profile` 按钮调用 `appController.saveActiveProfile()`。
- [x] 保存成功/失败日志由 `ZAppController` 内部通过 `AppendLog()` 写入 `ZLogModel`，QML 不直接写日志。
- [x] 保存成功写入 `Success: Profile saved` 日志。
- [x] 保存失败写入 `Error: Profile save failed` 日志，并通过 `runtimeError(message)` 暴露错误。
- [x] `TopBar.qml` 不直接拼路径，不持有文件系统策略。
- [x] 本轮不展示 `profilePath`；只通过 Event Log 给最小可见反馈。

## Tests

`AppControllerTests.cpp`：

- [x] `saveActiveProfile()` before initialize returns false and emits `runtimeError`。
- [x] `saveActiveProfile(explicitPath)` after initialize creates JSON file。
- [x] saved empty profile can be parsed by `ZProfileManager`。
- [x] apply binding then save writes one mapping rule with expected control id and action。
- [x] saving creates missing parent directories。
- [x] saving while Running succeeds。
- [x] saving preserves `mappingEnabled` value。
- [x] successful save emits `profileStatusChanged` and `profileSaved`。
- [x] failed save emits `profileStatusChanged` and `runtimeError`。

QML smoke：

- [x] offscreen 启动无 QML binding/import/property warning。

## Acceptance Criteria

- [x] `Save Profile` 不再只是写 demo log，而是调用真实保存 API。
- [x] 保存出的 JSON 可由 `ZProfileManager::LoadProfile()` 读回。
- [x] Apply 出来的 runtime binding 能持久化到 profile JSON。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续实现启动时加载上次保存的 profile。
- [ ] 后续实现 profile 文件选择和 profile path UI。
- [ ] 后续实现 `ZProfileModel` 与多个 profile 管理。
