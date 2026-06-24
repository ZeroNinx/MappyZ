# TODO: UI Catch-Up Plan

## Goal

先让现有 UI 跟上已经具备的 Runtime 能力，减少“功能存在但 UI 看不出来 / 点了没反馈 / 状态不清楚”的落差。持久化、加载、真实输出等新功能接入排在 UI 基础体验之后。

本文件只规划 UI / UI Bridge / App glue，不规划 Core 重构。详细功能模块设计仍放在 `todo.md`。

## Execution Principle

- [ ] 先处理 UI debt，再处理 feature plumbing。
- [ ] UI catch-up 不应被 Save/Load/Profile 等新功能长期阻塞。
- [ ] 每一阶段都应该改善用户当前可见体验。
- [ ] 默认保持 NullOutput 安全模式，真实 SendInput 必须由用户明确开启。

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

- [ ] P0 反馈统一采用所在 panel 内 inline message，不依赖 LogModel。
- [ ] inline message 可被下一次操作覆盖，并在短时间后自动清除，例如 3 秒。
- [ ] 成功反馈使用 accent/success tone，失败反馈使用 warning/danger tone。
- [ ] BindingEditor Apply 成功后给 inline feedback。
- [ ] BindingEditor Apply 失败后给 inline feedback，不只依赖 stderr。
- [ ] Capture active 时 Apply 按钮禁用，或明确显示 Apply 将使用当前 selected control。
- [ ] selectedDevice 为空时 Capture 与 Apply 按钮禁用，并显示 `Select a device first` inline hint。
- [ ] Current mappings 为空时显示 empty state，例如 `No mappings yet`。
- [ ] DevicesPanel 为空时显示可读说明，不只是一行弱提示。
- [ ] EventLogPanel 为空时显示 empty state。
- [ ] Runtime 未 running 时，Apply 仍可编辑 active profile snapshot，但 inline hint 显示 `Mapping will dispatch after runtime starts`，避免误导用户以为已经真实输出。
- [ ] Mapping On/Off 操作后给 inline feedback。
- [ ] P0 验收：已有 QML offscreen smoke 仍通过且无 warning。

## Priority 1: LogModel Lite

目标：替换 QML demo `eventModel`，让 Event Log 显示真实 UI / lifecycle 级事件，但不记录逐输入事件。

- [ ] 新增 `ZLogModel : QAbstractListModel`。
- [ ] roles 至少包含 `time`、`level`、`message`。
- [ ] `ZAppController` 暴露 `logModel`。
- [ ] 记录 UI/lifecycle 级事件：
  - [ ] initialize success/failure。
  - [ ] start/stop。
  - [ ] mapping enabled changed。
  - [ ] apply binding success/failure。
  - [ ] capture completed/cancelled。
  - [ ] runtime errors。
- [ ] 不把逐输入、逐 mapped、逐 dispatch success 写入 Event Log。
- [ ] per-frame mapped/dispatched 计数继续放 StatusBar。
- [ ] EventLogPanel 使用 `appController.logModel`，删除 Main.qml demo `eventModel`。
- [ ] LogModel capacity 固定为 200 条，超过后丢弃最旧记录。
- [ ] 增加测试：
  - [ ] log append 增加 row。
  - [ ] capacity 上限 200 生效。
  - [ ] runtimeError 同时写 log。
  - [ ] Apply 成功和失败都写 log。

## Priority 2: Runtime Status Display Cleanup

目标：让 UI 明确显示当前运行时状态、输出模式、profile 名称，不要求先实现保存/加载。

- [ ] TopBar profile tag 不再硬编码 `Default FPS`。
- [ ] `ZAppController` 暴露当前 active profile name，读取 RuntimeHost active profile snapshot 的 `Name`；初始为空或缺省时显示 `Default`。
- [ ] P4 `loadProfile(path)` 成功后通过同一 active profile name 属性自然更新，不在 P2 实现加载逻辑。
- [ ] StatusBar 显示 output backend state。
- [ ] StatusBar 显示 output mode：`NullOutput` / `RealOutput` / `Unavailable`。
- [ ] StatusBar 显示 mapping enabled 状态。
- [ ] Runtime message 和 output state 文案统一，不让用户误以为 NullOutput 已经真实输出。
- [ ] 增加测试：
  - [ ] 默认 profile name 为 Default。
  - [ ] mapping enabled 改变后 UI 状态属性同步。
  - [ ] output state 字符串稳定。

## Priority 3: Save Active Profile

目标：`Save Profile` 按钮保存当前 RuntimeHost active profile，而不是写 demo log。

详细设计见 `todo.md` 的 `Profile Save Snapshot`。这里不重复 API 签名，避免两份规划分叉。

UI 侧验收：

- [ ] TopBar `Save Profile` 调用真实保存 API。
- [ ] 保存成功后给用户可见反馈。
- [ ] 保存失败后给用户可见反馈。
- [ ] QML 不直接拼路径，不持有文件系统策略。
- [ ] 保存成功后 LogModel 写入 `Profile saved`。
- [ ] 保存失败后 LogModel 写入 `Profile save failed`。

## Priority 4: Load Saved Profile

目标：UI 启动或用户操作时能加载保存过的 profile，并刷新映射列表。

- [ ] 使用独立 `loadProfile(path)` 方案，不把 profile 加载继续塞进 `initializeRuntime(...)`。
- [ ] `loadProfile(path)` 不重建输入/输出后端，只替换 RuntimeHost profile snapshot 并刷新 UI model。
- [ ] 默认加载路径与 Save Active Profile 使用同一位置。
- [ ] 如果默认 profile 存在，启动后加载；不存在则继续使用空 Default profile。
- [ ] 加载成功后刷新 `mappingRuleModel`。
- [ ] 加载失败时保留当前安全状态，显示错误。
- [ ] TopBar profile tag 使用加载后的 profile name。
- [ ] 增加测试：
  - [ ] 默认路径不存在时启动成功且 profile 为空。
  - [ ] 已保存 profile 可通过 `loadProfile(path)` 加载。
  - [ ] 加载后 Current mappings 与 profile rules 一致。
  - [ ] 加载失败不清空当前 mappingRuleModel。
  - [ ] 加载失败发出可观察错误信号。

## Priority 5: Real Output Mode

目标：在保持 NullOutput 安全默认的前提下，提供明确的真实输出开关，用于验证 Apply -> SendInput 的端到端闭环。

- [ ] 默认继续使用 NullOutput，避免误触系统输入。
- [ ] UI 增加明确的 `Use Real Output` toggle 或按钮。
- [ ] 开启真实输出前给出可见提示或确认。
- [ ] `Main.qml` 不再永久硬编码 `appController.initializeRuntime(true)`，但默认仍传 NullOutput。
- [ ] 用户主动开启后，Stop -> Initialize(real output) -> Start。
- [ ] 真实输出不可用时回退 NullOutput，并显示错误/提示。
- [ ] 切换输出模式不丢失当前 mapping enabled 状态。
- [ ] 切换输出模式应保留当前 profile snapshot 并重新应用到新 host。
- [ ] 增加测试：
  - [ ] 默认 initialize 使用 NullOutput。
  - [ ] RealOutput 请求会走 output factory。
  - [ ] output factory 失败时 UI 有错误信号并保留安全状态。
  - [ ] 切换模式不会丢失 mapping enabled。
  - [ ] 切换模式后 mappingRuleModel 仍与 active profile 一致。

## Priority 6: Action Picker

目标：Binding Editor 不再只有两个硬编码输出动作。

- [ ] 保留当前简单 UI，不引入复杂弹窗。
- [ ] 先扩展 UI 选择能力，再重构 API。
- [ ] `applySelectedBinding()` 从字符串解析改结构化参数的工作推迟到本模块实现时一起做。
- [ ] 增加 action type 选择：
  - [ ] Keyboard key。
  - [ ] Mouse button。
  - [ ] Mouse move preset 后续再做。
- [ ] Keyboard 支持常用键：
  - [ ] Space、Enter、Escape、Tab。
  - [ ] A-Z。
  - [ ] 0-9。
  - [ ] Arrow keys。
- [ ] Mouse 支持 Left / Right / Middle。
- [ ] 增加测试：
  - [ ] 常用 keyboard key 能生成正确 `SKeyboardAction`。
  - [ ] mouse button 能生成正确 `SMouseButtonAction`。
  - [ ] 未知 key 被拒绝且不修改 profile。
  - [ ] 结构化参数不会依赖 QML 展示字符串。

## Priority 7: Mapping Rule Editing

目标：Current mappings 不只是展示，可以删除、替换和禁用规则。

- [ ] `ZAppController` 增加 `removeBinding(ruleId)`。
- [ ] `ZAppController` 增加 `setBindingEnabled(ruleId, enabled)`。
- [ ] Current mappings 每行增加删除按钮。
- [ ] Current mappings 每行增加 enabled 状态展示或切换。
- [ ] 点击 mapping row 可选中对应 control 和 action。
- [ ] 删除/禁用后立即 `ReplaceProfile()` 并刷新 `mappingRuleModel`。
- [ ] 增加测试：
  - [ ] remove existing rule。
  - [ ] remove unknown rule no-op 或 false，语义明确。
  - [ ] disable rule 后输入不再 mapped。
  - [ ] re-enable 后输入重新 mapped。

## Out Of Scope For This Catch-Up

- [ ] 不做复杂宏、组合键、长按、双击。
- [ ] 不做 per-app 自动 profile 切换。
- [ ] 不做云同步或导入导出 UI。
- [ ] 不做完整设置页。
- [ ] 不重做视觉设计。

## Suggested Execution Order

- [x] 1. Layout Fixes polish。
- [ ] 2. UI Feedback And Empty States。
- [ ] 3. LogModel Lite。
- [ ] 4. Runtime Status Display Cleanup。
- [ ] 5. Save Active Profile（详见 `todo.md`）。
- [ ] 6. Load Saved Profile。
- [ ] 7. Real Output Mode。
- [ ] 8. Action Picker。
- [ ] 9. Mapping Rule Editing。

