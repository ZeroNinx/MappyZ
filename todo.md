# TODO: Profile Autosave And Dirty State

## Next Step

下一步让 profile 编辑具备可靠持久化语义：用户 Apply / Delete / Load 后，UI 能明确知道当前配置是否已保存；成功编辑默认自动保存，失败时保留 dirty 状态并给出可见错误。

本轮目标不是做完整 profile 管理页，而是避免“映射已经生效但重启后丢失”的实际使用问题。

## Scope

包含：

- [x] `ZAppController` 增加 profile dirty 状态。
- [x] Apply binding 成功后标记 dirty，并触发自动保存。
- [x] Remove binding 成功后标记 dirty，并触发自动保存。
- [x] Load profile 成功后清除 dirty。
- [x] Save 成功后清除 dirty。
- [x] Save / autosave 失败后保持 dirty。
- [x] TopBar / StatusBar 明确显示保存状态。
- [x] Save Profile 按钮从”唯一保存入口”变成”手动重试 / 强制保存入口”。
- [x] Event Log 不被 autosave 刷屏，只记录有意义的成功/失败。
- [x] 增加 AppController 和 QML smoke 测试。

不做：

- [ ] 不做 profile 列表管理。
- [ ] 不做 Save As / Open File 对话框。
- [ ] 不做多 profile 切换。
- [ ] 不做 profile rename UI。
- [ ] 不做云同步或导入导出 UI。

## Product Semantics

当前语义：

- [ ] Apply / Delete 立即影响 running runtime。
- [ ] 但用户必须记得点 Save Profile，否则重启后更改可能丢失。
- [ ] UI 没有明确 dirty / saved 状态。

目标语义：

- [ ] Apply / Delete 是用户明确的配置修改，应该自动持久化到当前 active profile path。
- [ ] 如果当前没有 profile path，autosave 使用默认 profile path。
- [ ] Autosave 成功后 UI 显示已保存。
- [ ] Autosave 失败后 UI 显示未保存，用户可以点 Save Profile 重试。
- [ ] runtime 实时生效和磁盘保存是两个状态，但 UI 必须明确区分。

## API Plan

`ZAppController` 新增 Q_PROPERTY：

```cpp
Q_PROPERTY(bool profileDirty READ IsProfileDirty NOTIFY profileStatusChanged)
Q_PROPERTY(QString profileSaveState READ ProfileSaveState NOTIFY profileStatusChanged)
```

新增 / 调整成员：

```cpp
NODISCARD bool IsProfileDirty() const;
NODISCARD QString ProfileSaveState() const;

bool bProfileDirty = false;
QString CachedProfileSaveState; // "clean" / "dirty" / "error"
```

内部 helper：

```cpp
void MarkProfileDirty(QString Reason);
bool SaveActiveProfileInternal(QString ProfilePath, bool bAutosave);
bool AutosaveActiveProfile(QString Reason);
```

规则：

- [x] `MarkProfileDirty(...)` 设置 `bProfileDirty = true`，`profileSaveState = "dirty"`，emit `profileStatusChanged()`。
- [x] `SaveActiveProfileInternal(..., bAutosave=false)` 保持现有 `saveActiveProfile()` 的用户触发语义。
- [x] `SaveActiveProfileInternal(..., bAutosave=true)` 使用同一保存实现，但日志和 message 更低噪声。
- [x] `saveActiveProfile(path)` 改成薄封装，只调用 `SaveActiveProfileInternal(path, false)`。
- [x] `AutosaveActiveProfile(...)` 使用 `CachedProfilePath`；为空时使用 `DefaultProfilePath()`。
- [x] `AutosaveActiveProfile(...)` 由 mutation 点直接同步调用，不通过 signal/queued slot 延迟触发。
- [x] Apply / Delete 的返回反馈可以直接使用 autosave 返回值组合文案。
- [x] autosave 成功后更新 `CachedProfilePath`，`bProfileDirty = false`，`profileSaveState = "clean"`。
- [x] autosave 失败后保持 `bProfileDirty = true`，`profileSaveState = "error"`。
- [x] `saveActiveProfile(path)` 成功后同样清除 dirty；失败后保持 dirty。
- [x] `loadProfile(path)` 成功后清除 dirty；失败不改变 dirty。
- [x] 本轮保存仍是同步文件写入，不引入 `"saving"` 状态；如果后续改成异步保存，再增加 saving/pending 状态。

## Mutation Points

需要接入 dirty/autosave 的成功修改点：

- [x] `applySelectedBinding(...)` 成功修改 active profile 后。
- [x] `removeBinding(...)` 成功删除 rule 后。
- [ ] 后续新增 `setBindingEnabled(...)` 时也必须走同一 helper。

不应标记 dirty 的点：

- [ ] `initializeRuntime()` 创建默认空 profile。
- [ ] `loadProfile()` 成功加载磁盘 profile。
- [ ] `startRuntime()` / `stopRuntime()`。
- [ ] input event / capture state / selected device 变化。
- [ ] action picker 选择变化。

## Autosave Logging

Autosave 不能每次都刷屏：

- [x] Apply / Delete 本身继续写 Success log。
- [x] Autosave 成功默认不额外写 Success log，避免 Event Log 被重复信息淹没。
- [x] Autosave 失败必须写 Error log。
- [x] 手动 Save 成功继续写 `Profile saved`。
- [x] 手动 Save 失败继续写 `Profile save failed`。
- [x] `profileMessage` 更新为最近一次保存/自动保存状态。

## QML Plan

### TopBar

- [x] Profile tag 显示保存状态：
  - [x] clean: `Profile: Default`
  - [x] dirty: `Profile: Default *`
  - [x] error: `Profile: Default !`
- [x] `Save Profile` 按钮：
  - [x] clean 状态仍可点击，作为手动保存。
  - [x] dirty/error 状态更明显，提示可重试保存。
  - [x] tooltip 暂不做；本轮通过 label / inline text / status 展示。

### StatusBar

- [x] 增加保存状态文案，例如 `Profile: saved` / `Profile: unsaved` / `Profile: save error`。
- [x] 不把长路径塞进 StatusBar。

### BindingEditor

- [x] Apply 成功反馈补充保存结果：
  - [x] autosave 成功：`Applied and saved`
  - [x] autosave 失败：`Applied, save failed`
- [x] Delete 成功反馈补充保存结果：
  - [x] autosave 成功：`Binding removed and saved`
  - [x] autosave 失败：`Binding removed, save failed`
- [x] 不要求 BindingEditor 直接调用 save；保存由 AppController mutation helper 统一处理。

## Tests

`AppControllerTests.cpp`：

- [x] default `profileDirty == false`。
- [x] default `profileSaveState == "clean"`。
- [x] `profileSaveState` 只使用 `"clean"` / `"dirty"` / `"error"` 三态。
- [x] Apply success marks dirty before save and ends clean when autosave succeeds。
- [x] Apply success writes default profile file when no path exists。
- [x] Apply autosave failure keeps `profileDirty == true` and state error。
- [x] Remove binding success autosaves。
- [x] Remove binding autosave failure keeps dirty。
- [x] Manual `saveActiveProfile()` success clears dirty。
- [x] Manual `saveActiveProfile()` failure keeps dirty。
- [x] `loadProfile()` success clears dirty。
- [x] `loadProfile()` failure does not clear existing dirty state。
- [x] autosave success does not add duplicate noisy Success log per mutation。
- [x] autosave failure writes Error log。

`QML smoke / UI tests`：

- [x] `profileDirty` property exists and is readable.
- [x] `profileSaveState` property exists and is readable.
- [x] TopBar binds profile dirty state without warning.
- [x] StatusBar binds save state without warning.
- [x] QML smoke no binding/import/property warning.

## Acceptance Criteria

- [x] 用户 Apply 后不点 Save，重启仍能加载刚才的绑定。
- [x] 用户 Delete 后不点 Save，重启后删除仍然保留。
- [x] 保存失败时，UI 明确显示当前 profile 未保存。
- [x] 保存失败时，runtime 中的更改仍然立即生效。
- [x] 用户可以点击 Save Profile 重试保存。
- [x] Event Log 不被每次 autosave 成功刷屏。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续做 Profile Manager：rename / duplicate / delete profile。
- [ ] 后续做 Save As / Import / Export。
- [ ] 后续做设置页：autosave on/off、profile path、startup behavior。
- [ ] 后续支持 rule enable/disable。
