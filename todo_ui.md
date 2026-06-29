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

## Priority 8: Editable Gamepad Mapping View

目标：参考 `docs/gamepad_mapping_layout_editable.png` / `docs/gamepad_mapping_layout_editable.svg`，把当前中间 `GamepadView` 重构成可直接查看和编辑映射的手柄布局。右侧 `BindingEditor` 不再承担完整 Current mappings 列表，只保留当前选中控件的 Inspector / 编辑操作。

本轮只做 UI 结构和现有键盘/鼠标按钮映射展示，不新增 DInput 功能，不新增宏、turbo、hold、复杂参数页。

### Design Source

- [ ] 参考图的整体结构保持不变：
  - [ ] 左侧主区域标题为 `Gamepad Mapping View`，承载手柄图、连线、控制分组卡片和每个控件的当前绑定。
  - [ ] 右侧为 `Inspector`，只展示 selected control、pending action、Capture / Clear / Choose 等操作和后续高级信息。
  - [ ] 主区域中的 mapping row 可点击，点击后选中对应 control，并同步右侧 Inspector。
- [ ] 参考图尺寸是视觉设计稿，不直接硬编码到 QML：
  - [ ] SVG viewBox 为 `1672x941`，其中主区域约 `1160x914`，Inspector 约 `464x914`。
  - [ ] 当前应用窗口、左侧 DevicesPanel、右侧 BindingEditor 宽度与设计稿不同，实现时必须按当前 `GamepadView` 可用尺寸做比例换算。
  - [ ] 坐标使用归一化 anchor / ratio / scale，而不是直接复制 SVG 像素坐标。
  - [ ] 以主区域参考尺寸 `1160x914` 作为内部设计坐标系，计算 `scale = min(actualWidth / 1160, actualHeight / 914)`，再把分组卡片、手柄、连线按统一 scale 投影到当前面板。
  - [ ] 最小窗口下如果无法完整展示，优先启用主区域内部缩放或滚动，不允许卡片和 Footer 裁切。

### Layout Plan

- [ ] 新增或重构中间主组件，例如 `ui/gamepad/EditableGamepadMappingView.qml`，替代当前纯手柄状态展示式 `GamepadView`。
- [ ] 保留当前 `InputControlState` 的实时高亮能力：
  - [ ] 手柄实体按钮按输入状态发亮。
  - [ ] mapping row 也能根据对应 control 的输入状态轻微高亮。
  - [ ] selected control 使用 accent 边框或 glow 标识。
- [ ] 主区域按参考图拆成固定控制组：
  - [ ] `LB / LT`：`left_shoulder`、`left_trigger`。
  - [ ] `Back / Guide / Start`：`button_back`、`button_guide`、`button_start`。
  - [ ] `RB / RT`：`right_shoulder`、`right_trigger`。
  - [ ] `Left Stick`：`left_stick_up`、`left_stick_left`、`left_stick_down`、`left_stick_right`。
  - [ ] `D-Pad`：`dpad_up`、`dpad_down`、`dpad_left`、`dpad_right`。
  - [ ] `Right Stick`：`right_stick_up`、`right_stick_down`、`right_stick_left`、`right_stick_right`。
  - [ ] `ABXY`：`button_south`、`button_east`、`button_west`、`button_north`。
- [ ] 每个控制组使用统一 `MappingGroupCard` 组件：
  - [ ] 标题。
  - [ ] 多行 `MappingRow`。
  - [ ] 左侧显示用户友好输入名，例如 `A`、`B`、`Up`、`LT`。
  - [ ] 右侧显示当前输出，例如 `Space`、`Mouse L`、`Unassigned`。
  - [ ] 未绑定状态显示 `Unassigned`，使用 muted/italic，不显示空白。
- [ ] 每个 `MappingRow` 行为：
  - [ ] 点击 row 选中对应 control。
  - [ ] 双击或点击行内 action 区域打开现有 `MappingPickerDialog`。
  - [ ] 已选中 row 高亮，且右侧 Inspector 同步显示该 control。
  - [ ] row 不直接做删除按钮，删除/清空先由右侧 Inspector 的 Clear 处理。
- [ ] 保留手柄轮廓图，但降低它对布局的支配：
  - [ ] 手柄轮廓居中，作为空间和连线参照。
  - [ ] 分组卡片围绕手柄放置。
  - [ ] 蓝色连接线连接分组卡片与对应手柄区域。
  - [ ] 连接线可以先用 `Shape` / `Canvas` / 简单 `Rectangle` 段实现，不要求完全复刻 SVG 曲线。
- [ ] `ABXY` 的当前选中和实时按下状态要同时可见：
  - [ ] 当前选中 control 用 accent 边框。
  - [ ] 正在按下输入用 success / glow。
  - [ ] 两者同时存在时不互相覆盖。

### Right Inspector Refactor

- [ ] `BindingEditor` 改成 Inspector 语义：
  - [ ] 标题从 `Binding Editor` 改为 `Inspector` 或 `Binding Inspector`。
  - [ ] 保留 selected control 显示。
  - [ ] 保留 action output/pending action 显示。
  - [ ] 保留 `Capture Input`、`Choose...`、`Clear`。
  - [ ] 保留 Apply 或改为 picker Confirm 后直接 apply，二选一需实现前明确；本轮建议先保留 Apply，降低行为变更。
- [ ] 从右侧 Inspector 移除完整 `Current mappings` Repeater。
- [ ] 删除/清空能力不丢失：
  - [ ] 当前 selected control 已有绑定时，`Clear` 删除对应 mapping rule。
  - [ ] 当前 selected control 无绑定时，`Clear` 只清空 selectedControl 或保持 no-op，需要明确文案。
  - [ ] 清空成功/失败继续使用 inline feedback。
- [ ] Inspector 中的 Advanced 区先不做真实功能：
  - [ ] 可以显示占位字段 `Mode: Press`、`Conflict: None`，但不能暗示已实现 turbo/hold。
  - [ ] 或者本轮完全不显示 Advanced，避免虚假功能。
  - [ ] 选择其中一种并在实现前固定。

### Data / Model Requirements

- [ ] `GamepadView` 需要能按 `controlId` 查询当前绑定显示，不应在 QML 里遍历整表拼复杂逻辑。
- [ ] 优先在 `ZMappingRuleModel` 增加只读 invokable helper：
  - [ ] `displayOutputForInput(QString controlId) -> QString`，未绑定返回空字符串。
  - [ ] `displayKindForInput(QString controlId) -> QString`，未绑定返回空字符串。
  - [ ] `ruleIdForInput(QString controlId) -> QString`，用于 Clear 当前 control。
  - [ ] 如已有同等 helper，则复用，不新增重复 API。
- [ ] QML 展示层只负责 control group 配置和绑定展示，不直接构造 profile/rule。
- [ ] 当前 `applySelectedBinding`、`removeBinding`、`setBindingEnabled` 等 AppController API 不在本轮扩大语义。
- [ ] 右侧 Inspector 和中间 MappingRow 使用同一个 selected control 状态，避免出现“中间选中 A、右侧显示 B”的分叉状态。

### Responsive Behavior

- [ ] 默认窗口尺寸下，中间区域完整显示所有分组卡片、手柄、连线和右侧 Inspector。
- [ ] 最小窗口尺寸下：
  - [ ] 中间主区域不裁切 ABXY、Right Stick、D-Pad、底部卡片。
  - [ ] 连接线可以缩短或隐藏，但 mapping row 必须可读。
  - [ ] 如空间不足，主区域可用 Flickable 横向/纵向滚动，优先保证可操作。
- [ ] 不使用负 margin 把卡片推出父区域。
- [ ] 文本必须 elide 或缩放到卡片内，不允许盖住右侧输出值。
- [ ] 参考图中的 group 位置按比例保留：
  - [ ] LB/LT 在手柄左上方。
  - [ ] Back/Guide/Start 在上方居中。
  - [ ] RB/RT 在手柄右上方。
  - [ ] Left Stick 在左侧。
  - [ ] ABXY 在右侧。
  - [ ] D-Pad 在下方偏左。
  - [ ] Right Stick 在下方偏右。

### Visual Requirements

- [ ] 回退上一轮失败的低对比度方案，保持文字和按钮肉眼可辨。
- [ ] 正常文本、muted 文本、disabled 文本、selected 文本必须有明确层级。
- [ ] `Unassigned` 不应暗到不可读。
- [ ] Selected row 的高亮不能只靠文字颜色，必须有边框或背景变化。
- [ ] 按下状态的发光应克制，不影响 row 文本阅读。
- [ ] 当前主题仍以现有 MappyZ 暗色风格为准，不照搬参考图的所有颜色。

### Tests / Verification

- [ ] `ZMappingRuleModel` 新 helper 测试：
  - [ ] 已绑定 control 返回正确 output/display kind/rule id。
  - [ ] 未绑定 control 返回空字符串。
  - [ ] 替换 rules 后 helper 结果更新。
- [ ] AppController / QML smoke：
  - [ ] QML module loads without warnings。
  - [ ] `EditableGamepadMappingView` 在默认窗口尺寸加载无 binding loop。
  - [ ] 无 required property 未赋值 warning。
- [ ] 手动验证：
  - [ ] 点击 ABXY row 可选中对应 control。
  - [ ] 点击 Left Stick 方向 row 可选中 `left_stick_*`。
  - [ ] 点击 row 后右侧 Inspector 显示同一个 control。
  - [ ] Choose + Apply 后中间 row 立即显示新绑定。
  - [ ] Clear 当前 control 后中间 row 变为 `Unassigned`。
  - [ ] 实际按下手柄按钮时，中间手柄按钮和 row 都有实时反馈。

## Out Of Scope For This Catch-Up

- [ ] 不做复杂宏、组合键、长按、双击。
- [ ] 不做 per-app 自动 profile 切换。
- [ ] 不做云同步或导入导出 UI。
- [ ] 不做完整设置页。
- [ ] 不做完整视觉体系重做；本轮只重构中间映射区域和右侧 Inspector 信息架构。

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
- [ ] 10. Editable Gamepad Mapping View。

建议拆成两步实施：

- [x] Step A 先做删除/清理：output toggle、TopBar 按钮、Runtime 卡、设备 tag 文案、无参 initialize API 和相关死测试。
- [x] Step B 再做新增交互：removeBinding、mapping row 删除、row 点击回填 action picker、`actionValue` / `displayKind` role。
- [ ] Step C 重构中间区域：按参考图比例换算布局，迁移 Current mappings 到手柄周围分组卡片，右侧只保留 Inspector。

