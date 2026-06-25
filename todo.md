# TODO: Profile Load Snapshot

## Next Step

下一步实现“加载保存过的 active profile”：UI 启动时在 runtime 初始化成功后尝试加载默认 profile，后续测试也可以通过显式路径加载 profile。

本轮只做单个 active profile 的加载，不做 profile 切换列表、不做文件选择器、不做 dirty 状态。

## Scope

包含：

- [x] `ZAppController` 增加加载 profile 的 QML API。
- [x] 使用现有 `ZProfileManager::LoadProfile()` 完成 JSON 读取。
- [x] 加载成功后替换 `RuntimeHost` active profile snapshot。
- [x] 加载成功后刷新 `MappingRuleModel`。
- [x] 加载成功后刷新 `activeProfileName`。
- [x] 默认加载路径与 `saveActiveProfile()` 使用同一位置。
- [x] `Main.qml` 在 `initializeRuntime()` 成功后调用无参 `loadProfile()`。
- [x] 增加 UI Bridge 单元测试覆盖加载链路。

不做：

- [x] 不实现 profile 手动切换 UI。
- [x] 不实现文件选择对话框。
- [x] 不引入 `ZProfileModel`。
- [x] 不做 profile dirty 状态。
- [x] 不把加载逻辑塞进 `initializeRuntime()`。
- [x] 不重建输入/输出后端。
- [x] 不保存或加载 event log / runtime input state。

## API Plan

`ZAppController` 新增：

- [x] `Q_INVOKABLE bool loadProfile(QString profilePath = QString())`
- [x] `void profileLoaded(QString profilePath)`

复用 P3 已有属性和信号：

- [x] `Q_PROPERTY(QString profilePath READ ProfilePath NOTIFY profileStatusChanged)`
- [x] `Q_PROPERTY(QString profileMessage READ ProfileMessage NOTIFY profileStatusChanged)`
- [x] `void profileStatusChanged()`
- [x] `void runtimeError(QString message)`
- [x] `void runtimeStatusChanged()`

内部 helper：

- [x] 抽取 `QString DefaultProfilePath() const`，供 `saveActiveProfile()` 和 `loadProfile()` 共用。
- [x] `DefaultProfilePath()` 使用 `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)` 并追加 `profiles/default.json`。
- [x] 如果 `QStandardPaths` 返回空路径，保存和加载都返回失败并发出可观察错误。

语义：

- [x] `profilePath` 参数为空时使用默认 profile 路径。
- [x] `profilePath` 参数非空时使用调用方传入路径，主要供测试和后续文件选择器使用。
- [x] 无参 `loadProfile()` 找不到默认 profile 时返回 `true`，保持当前 Default 空 profile，不发 `runtimeError()`。
- [x] 显式 `loadProfile(path)` 找不到文件时返回 `false`，更新 `profileMessage`，发 `runtimeError(message)`。
- [x] 加载成功更新 `profilePath` 和 `profileMessage`，发 `profileStatusChanged()` 和 `profileLoaded(profilePath)`。
- [x] 加载失败只更新 `profileMessage`，不修改 `profilePath`。
- [x] 加载成功后发 `runtimeStatusChanged()`，保证 `activeProfileName` 绑定刷新。
- [x] 加载成功后刷新 `MappingRuleModel`，保证 Current mappings 立即更新。

## Behavior

- [x] Runtime 未 initialize 时 `loadProfile()` 返回 false，不读取文件，发 `runtimeError()`。
- [x] Ready 或 Running 状态下允许加载。
- [x] 加载内容来自 `ZProfileManager::LoadProfile()`。
- [x] 加载成功调用 `Bootstrap.GetRuntimeHost().ReplaceProfile(...)`。
- [x] 加载成功不会修改 mapping enabled 状态。
- [x] 加载成功不会启动或停止 runtime。
- [x] 加载成功不会重建 input/output backend。
- [x] 加载失败保留当前 RuntimeHost profile。
- [x] 加载失败不清空 `MappingRuleModel`。
- [x] 加载失败不改变 Runtime 状态。
- [x] 空 profile 文件加载为合法 profile，`MappingRuleModel` 为空。
- [x] 含 mapping rule 的 profile 加载后，Current mappings 与 profile rules 一致。

## QML Plan

- [x] `Main.qml` 的 `Component.onCompleted` 流程保持显式：
  - [x] `initializeRuntime(true)` 成功后调用 `appController.loadProfile()`。
  - [x] `loadProfile()` 成功或默认文件不存在时继续 `startRuntime()`。
  - [x] `loadProfile()` 失败时保留 runtime initialized 状态，并通过 Event Log / runtime error 暴露问题。
- [x] 不把 profile 加载继续塞进 `initializeRuntime(...)`。
- [x] QML 不直接拼 profile 路径，不持有文件系统策略。
- [x] 加载成功/失败日志由 `ZAppController` 内部通过 `AppendLog()` 写入 `ZLogModel`。
- [x] 加载成功写入 `Success: Profile loaded` 日志。
- [x] 默认 profile 不存在时写入 `Info: No saved profile found` 或等价低噪声日志，不作为错误。
- [x] 加载失败写入 `Error: Profile load failed` 日志，并通过 `runtimeError(message)` 暴露错误。
- [x] TopBar profile tag 继续绑定 `activeProfileName`，加载成功后自然更新。

## Tests

`AppControllerTests.cpp`：

- [x] `loadProfile()` before initialize returns false and emits `runtimeError`。
- [x] `loadProfile()` with missing default path returns true and keeps empty Default profile。
- [x] `loadProfile(explicitPath)` missing file returns false and emits `runtimeError`。
- [x] `loadProfile(explicitPath)` loads saved profile and refreshes `MappingRuleModel`。
- [x] loading profile updates `activeProfileName` from profile `Name`。
- [x] loading profile preserves `mappingEnabled` value。
- [x] loading profile while Running succeeds and does not stop runtime。
- [x] loading failure does not clear existing `MappingRuleModel`。
- [x] loading failure does not change `profilePath`。
- [x] successful load emits `profileStatusChanged` and `profileLoaded`。
- [x] default path save then no-arg load round-trips under `QStandardPaths::setTestModeEnabled(true)`。
- [x] introduce a small RAII guard for `QStandardPaths::setTestModeEnabled(true)` once P4 tests need it.

QML smoke：

- [x] offscreen 启动无 QML binding/import/property warning。

## Acceptance Criteria

- [x] App 启动时能自动尝试加载默认 profile，但加载逻辑不在 `initializeRuntime()` 内部。
- [x] 默认 profile 不存在时启动仍成功，保持空 Default profile。
- [x] 保存过的 profile 可通过无参 `loadProfile()` 从默认路径读回。
- [x] 显式路径 profile 可加载并刷新 Current mappings。
- [x] 加载后 TopBar profile tag 反映 profile name。
- [x] 加载失败保留当前 profile 和 mapping list。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续实现 profile 文件选择和 profile path UI。
- [ ] 后续实现 `ZProfileModel` 与多个 profile 管理。
- [ ] 后续实现 profile dirty 状态。
