# TODO: UI Catch-Up Plan

## Goal

先让现有 UI 跟上已经具备的 Runtime 能力，减少“功能存在但 UI 看不出来 / 点了没反馈 / 状态不清楚”的落差。持久化、加载、运行中实时生效等能力接入排在 UI 基础体验之后。

本文件只规划 UI / UI Bridge / App glue，不规划 Core 重构。详细功能模块设计仍放在 `todo.md`。

## Execution Principle

- [ ] 先处理 UI debt，再处理 feature plumbing。
- [ ] UI catch-up 不应被 Save/Load/Profile 等新功能长期阻塞。
- [ ] 每一阶段都应该改善用户当前可见体验。
- [ ] 只要 runtime 在 running，输出就应实时生效；不再引入 `Real Output` 开关。

## Priority -1: Layout Fixes

目标：先修掉当前 UI 已有的显示问题，避免后续功能继续叠在不稳定布局上。

- [x] 设备名称、selected input 文本不应被明显截断。
- [x] 长文本优先增加可用宽度或换行；只有空间确实不足时才使用 elide。
- [x] `GamepadView` 标题区不能被 LT/RT trigger bar 遮挡；设备名、selected input、trigger bar 需要明确垂直分区或 z-order。
- [x] `Current mappings` 增加滚动区域，规则数量超过可见高度时不溢出。
- [x] BindingEditor 内容高度不足时内部可滚动，不挤压底部内容。
- [x] DevicesPanel 设备列表较多时可滚动。
- [x] EventLogPanel 保持可滚动，不因长日志撑破布局。
- [x] 小窗口下三列布局不重叠、不裁切主要按钮。
- [x] 增加 QML offscreen smoke，确认无 binding/import/property warning。

## Priority 0: UI Feedback And Empty States

目标：把已有操作的成功、失败、空状态说清楚，不新增持久化或加载能力。

- [x] P0 反馈默认采用所在 panel 内 inline message，不依赖 LogModel。
- [x] TopBar 操作例外曾使用主按钮 label + primary state 作为即时反馈；当前该按钮已移除，不再保留这条交互。
- [x] inline message 可被下一次操作覆盖，并在短时间后自动清除，例如 3 秒。
- [x] 成功反馈使用 accent/success tone，失败反馈使用 warning/danger tone。
- [x] BindingEditor Apply 成功后给 inline feedback。
- [x] BindingEditor Apply 不监听全局 `runtimeError`，避免 initialize/start/save 等非 Apply 错误污染局部反馈。
- [x] BindingEditor Apply 点击前先做 UI 前置检查：
  - [x] selectedDevice 为空：禁用 Capture 与 Apply，并显示 `Select a device first`。
  - [x] selectedControl 为空：禁用 Apply 或显示 `Select an input first`。
  - [x] selectedAction 为空：禁用 Apply 或显示 `Select an action first`。
  - [x] Runtime 未 initialize 或处于 Error：禁用 Apply，并显示 `Initialize runtime first`。
- [x] BindingEditor Apply 后端返回 false 时显示通用 `Apply failed`，不尝试从全局错误信号反推原因。
- [x] Capture active 时 Apply 按钮禁用，或明确显示 Apply 将使用当前 selected control。
- [x] Current mappings 为空时显示 empty state，例如 `No mappings yet`。
- [x] DevicesPanel 为空时显示可读说明，不只是一行弱提示。
- [x] Runtime 已 initialize 但未 running 时，Apply 仍可编辑 active profile snapshot；成功后 inline hint 显示 `Mapping will dispatch after runtime starts`，避免误导用户以为已经开始实时 dispatch。
- [x] P0 验收：已有 QML offscreen smoke 仍通过且无 warning。

## Priority 1: LogModel Lite

目标：替换 QML demo `eventModel`，让 Event Log 显示真实 UI / lifecycle 级事件，但不记录逐输入事件。

- [x] 新增 `ZLogModel : QAbstractListModel`。
- [x] roles 包含 `time`、`level`、`message`。
- [x] `time` 使用绝对时间 `HH:mm:ss.zzz`，通过 `QTime::currentTime().toString("HH:mm:ss.zzz")` 生成；不维护应用启动基准。
- [x] `level` 固定为 `Info` / `Success` / `Warning` / `Error` 四类：
  - [x] `Info`：initialize success、start、stop、mapping enabled/disabled。
  - [x] `Success`：apply binding success、capture completed。
  - [x] `Warning`：capture cancelled、暂未实现但用户主动触发的操作提示。
  - [x] `Error`：initialize/start/apply/runtimeError 等失败。
- [x] `ZAppController` 暴露只读 `logModel`，QML 只能绑定读取，不能直接 append/clear。
- [x] 日志写入入口集中在 `ZAppController` 内部，禁止 QML 组件直接拼日志字符串写 model。
- [x] `ZAppController` 增加内部 helper，例如 `AppendLog(level, message)`，所有 lifecycle 日志统一走它。
- [x] `ZAppController` 增加内部 helper，例如 `EmitRuntimeError(message)`：先写 `Error` 日志，再 `emit runtimeError(message)`，避免漏记和重复记录。
- [x] 记录 UI/lifecycle 级事件：
  - [x] initialize success/failure。
  - [x] start/stop。
  - [x] mapping enabled changed。
  - [x] apply binding success/failure。
  - [x] capture completed/cancelled。
  - [x] runtime errors。
- [x] Capture completed/cancelled 通过 `InputCaptureInstance` 信号在 `ZAppController` 内部连接并记录。
- [x] 不把逐输入、逐 mapped、逐 dispatch success 写入 Event Log。
- [x] per-frame mapped/dispatched 计数继续放 StatusBar。
- [x] EventLogPanel 使用 `appController.logModel`，删除 Main.qml demo `eventModel`。
- [x] EventLogPanel 使用真实 `logModel` 后增加 empty state，例如 `No events yet`；P0 不处理 demo model 的空状态。
- [x] EventLogPanel 着色逻辑改为匹配四类 level：`Error` 用 warning/danger，`Warning` 用 warning，`Success` 用 success，`Info` 用默认 text/muted。
- [x] TopBar 移除 `required property var eventModel`。
- [x] TopBar `Save Profile` 在 P1 不直接写 logModel；改为调用 `ZAppController` 的临时 API，例如 `notifySaveProfileNotImplemented()`。
- [x] `notifySaveProfileNotImplemented()` 只在 AppController 内部写一条 `Warning: Profile save not yet implemented`，P3 再替换为真实保存。
- [x] LogModel capacity 固定为 200 条，超过后丢弃最旧记录。
- [x] 增加测试：
  - [x] log append 增加 row。
  - [x] capacity 上限 200 生效。
  - [x] runtimeError 同时写 log。
  - [x] Apply 成功和失败都写 log。
  - [x] QML 无法直接调用 LogModel append/clear。
  - [x] TopBar Save Profile 写入 `Warning` 级暂未实现日志。

## Priority 2: Runtime Status Display Cleanup

目标：让 UI 明确显示当前运行时状态、输出后端状态、profile 名称，不要求先实现保存/加载。

- [x] TopBar profile tag 不再硬编码 `Default FPS`。
- [x] `ZAppController` 暴露当前 active profile name：
  - [x] `Q_PROPERTY(QString activeProfileName READ ActiveProfileName NOTIFY runtimeStatusChanged)`。
  - [x] `ActiveProfileName()` 在 Ready/Running 状态下读取 `RuntimeHost.GetProfileSnapshot().Name`。
  - [x] Runtime 未 initialize、Error、或 profile name 为空时返回 `Default`。
  - [x] TopBar Tag 绑定 `appController.activeProfileName`。
- [x] P4 `loadProfile(path)` 成功后通过同一 active profile name 属性自然更新，不在 P2 实现加载逻辑。
- [x] StatusBar 显示用户可理解的 output backend/status；`outputState` 保留给调试和测试使用。
- [x] 输出状态的事实来源放在 `ZApplicationBootstrap` / `RuntimeHost`，不在 QML 或 `ZAppController` 里重复缓存可切换 mode。
- [x] `ZAppController` 暴露 output display text，而不是让 QML 拼底层 state：
  - [x] `Q_PROPERTY(QString outputDisplayText READ OutputDisplayText NOTIFY runtimeStatusChanged)`。
  - [x] runtime 未 initialize 或 backend state 为 `Unavailable` 时返回 `Unavailable`。
  - [x] backend state 为 `Error` 时返回 `Output Error`。
  - [x] backend ready 且 runtime running 时返回 `Live Output`。
  - [x] backend ready 但 runtime 未 running 时返回 `Ready`。
- [x] StatusBar 的 `Output:` 使用 `appController.outputDisplayText`，不再直接暴露底层 mode/state。
- [x] StatusBar 已显示 mapping enabled 状态。
- [x] Runtime message 和 output display text 文案统一，不制造”运行但不输出”的模式歧义。
- [x] 增加测试：
  - [x] 默认 profile name 为 Default。
  - [x] mapping enabled 改变后 UI 状态属性同步。
  - [x] `activeProfileName` 在未 initialize、默认 profile、空 profile name 时返回 `Default`。
  - [x] `outputState` 字符串保持稳定：`unavailable` / `ready` / `error`。
  - [x] `outputDisplayText` 区分 `Ready` / `Live Output` / `Unavailable` / `Output Error`。
  - [x] StatusBar QML 绑定 `outputDisplayText`，QML smoke 无 warning。

## Priority 3: Save Active Profile

目标：`Save Profile` 按钮保存当前 RuntimeHost active profile，而不是写 demo log。

详细设计见 `todo.md` 的 `Profile Save Snapshot`。这里不重复 API 签名，避免两份规划分叉。

UI 侧验收：

- [x] TopBar `Save Profile` 调用真实保存 API。
- [x] 保存成功后给用户可见反馈。
- [x] 保存失败后给用户可见反馈。
- [x] QML 不直接拼路径，不持有文件系统策略。
- [x] 保存成功后 LogModel 写入 `Profile saved`。
- [x] 保存失败后 LogModel 写入 `Profile save failed`。

## Priority 4: Load Saved Profile

目标：UI 启动或用户操作时能加载保存过的 profile，并刷新映射列表。

详细设计见 `todo.md` 的 `Profile Load Snapshot`。这里不重复完整 API 签名，避免两份规划分叉。

UI 侧验收：

- [x] 使用独立 `loadProfile(path = QString())` 方案，不把 profile 加载继续塞进 `initializeRuntime(...)`。
- [x] `Main.qml` 在 `initializeRuntime()` 成功后显式调用无参 `appController.loadProfile()`。
- [x] 默认加载路径与 Save Active Profile 使用同一 `DefaultProfilePath()` helper。
- [x] 默认 profile 不存在时启动成功且 profile 保持空 Default。
- [x] 已保存 profile 加载后刷新 `mappingRuleModel`。
- [x] 已保存 profile 加载后 TopBar profile tag 使用加载后的 profile name。
- [x] 加载失败时保留当前 profile / mapping list，并显示错误。
- [x] QML 不直接拼路径，不持有文件系统策略。
- [x] 加载成功后 LogModel 写入 `Profile loaded`。
- [x] 默认 profile 不存在时只写低噪声 Info 日志或不报错。
- [x] 加载失败后 LogModel 写入 `Profile load failed`。

## Priority 5: Remove Output Toggle

目标：删除显式 `Real Output` 产品流程，改成 runtime running 时输出默认实时生效。

详细设计见 `todo.md` 的 `Remove Output Mode Toggle`。这里不重复完整 API 签名，避免两份规划分叉。

UI 侧验收：

- [x] TopBar 不再显示 `Real Output` / `Confirm Real Output`。
- [x] `Main.qml` 改为调用无参 `initializeRuntime()`，不再以 `initializeRuntime(true)` 走 `NullOutput` 启动路径。
- [x] runtime 启动成功后输出立即生效，不需要二次确认。
- [x] 输出后端不可用时，initialize / start 直接报错，不进入”运行但不生效”的状态。
- [x] StatusBar 如显示 `Output:`，应表达 backend 状态，而不是 `NullOutput` / `RealOutput` 模式。
- [x] QML 不直接持有 output mode 状态。

## Priority 6: Action Picker

目标：Binding Editor 不再只有两个硬编码输出动作。

详细设计见 `todo.md` 的 `Action Picker`。这里不重复完整 API 签名，避免两份规划分叉。

UI 侧验收：

- [x] 保留当前简单 UI，不引入复杂弹窗。
- [x] 使用 `appController.actionCatalogModel` 渲染可选 action。
- [x] action 选择状态由 `BindingEditor` 内部管理；`Main.qml` 不再持有 `selectedAction: "Keyboard: Space"` root property。
- [x] 使用 flat list 下拉选择，不做分组 header；`category` role 暂不驱动 UI 分组。
- [x] `Action output` 显示选中项的展示文案。
- [x] Apply 调用结构化参数，不再传 `"Keyboard: Space"` 这种展示字符串。
- [x] 默认选中 `Keyboard / Space`，保持当前体验。
- [x] Keyboard 支持 Space、Enter、Escape、Tab、A-Z、0-9、Arrow keys。
- [x] Mouse 支持 Left / Right / Middle。
- [x] Current mappings 展示不退化。
- [x] QML smoke 无 binding/import/property warning。

## Priority 7: Functional Runtime Pass

目标：让主流程真正跑通，而不是只能看输入和创建映射。

详细设计见 `todo.md` 的 `Functional Runtime Pass`。这里不重复完整 API 签名，避免两份规划分叉。

UI 侧验收：

- [x] DevicesPanel 只显示真实设备，不显示永久 `Runtime` 卡。
- [x] 设备卡状态 tag 改为 `Connected`，不显示 runtime state。
- [x] TopBar profile tag 显示 `Profile: <name>`。
- [x] TopBar 移除含义不明的 `Mapping On/Off` 主按钮。
- [x] runtime running 后设备列表保持稳定，不因输出语义调整而清空。
- [x] runtime running 后输入状态仍继续更新。
- [x] Current mappings 每行增加删除按钮。
- [x] 点击 Current mappings 行能回填 selected input 和 action picker。
- [x] Current mappings 的 Tag 使用展示向 `displayKind`，不直接显示 `MouseButton`。
- [x] 删除 mapping 成功/失败都有 inline feedback。
- [x] QML smoke 无 binding/import/property warning。

## Out Of Scope For This Catch-Up

- [ ] 不做复杂宏、组合键、长按、双击。
- [ ] 不做 per-app 自动 profile 切换。
- [ ] 不做云同步或导入导出 UI。
- [ ] 不做完整设置页。
- [ ] 不重做视觉设计。

## Suggested Execution Order

- [x] 1. Layout Fixes polish。
- [x] 2. UI Feedback And Empty States。
- [x] 3. LogModel Lite。
- [x] 4. Runtime Status Display Cleanup。
- [x] 5. Save Active Profile（详见 `todo.md`）。
- [x] 6. Load Saved Profile。
- [x] 7. Remove Output Toggle / API Cleanup。
- [x] 8. Action Picker。
- [x] 9. Functional Runtime Pass。

建议拆成两步实施：

- [x] Step A 先做删除/清理：output toggle、TopBar 按钮、Runtime 卡、设备 tag 文案、无参 initialize API 和相关死测试。
- [x] Step B 再做新增交互：removeBinding、mapping row 删除、row 点击回填 action picker、`actionValue` / `displayKind` role。

