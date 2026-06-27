# TODO: Keyboard / Mouse Mapping Picker

## Goal

把 BindingEditor 里的 `Action output` 下拉框替换成一个类似 AntiMicroX 的映射选择器。

用户当前已经可以通过 Capture 或点击 GamepadView 选中一个手柄输入 `selectedControl`，但输出动作只能从 ComboBox 里选。下一步目标是：打开选择器后展示可点击的键盘 / 鼠标布局，用户可以手动点选输出按键，也可以在选择器获得焦点时直接按物理键盘按键让对应虚拟键实时高亮；点击 Confirm 后立即把当前手柄输入和高亮输出按键绑定。

本轮只做“单个输入 -> 单个 Keyboard / MouseButton 输出”的选择器，不做复杂组合键或宏。

## Scope

包含：

- [ ] 新增 QML 映射选择器，替换 BindingEditor 中的 action ComboBox。
- [ ] 选择器支持 Keyboard / Mouse 两个页签。
- [ ] Keyboard 页显示常用键盘布局，按 AntiMicroX 风格使用大按钮网格。
- [ ] Mouse 页显示 Left / Middle / Right / Wheel Up / Wheel Down / Mouse Button 4 / Mouse Button 5 的按钮布局。
- [ ] 用户点击虚拟按键时，该按键高亮并成为 pending action。
- [ ] 选择器拥有焦点时，按物理键盘按键会实时高亮对应虚拟键。
- [ ] 点击 Confirm 后自动调用现有 `applySelectedBinding(selectedControl, kind, value)`。
- [ ] Confirm 成功后关闭选择器，并复用 BindingEditor 的 Apply feedback。
- [ ] Cancel / Esc 关闭选择器，不修改映射。
- [ ] 当前 mappings 行点击后仍能回填 selected input 和 pending action。
- [ ] QML smoke 覆盖新组件无 warning。

不做：

- [ ] 不做组合键，例如 Ctrl+Shift+A。
- [ ] 不做宏、长按、双击、turbo、toggle。
- [ ] 不做 mouse move / axis 参数 UI，本轮保留已有 MouseMove catalog 能力但不放进键盘页。
- [ ] 不做全局 OS 级键盘钩子；只捕获选择器窗口获得焦点时的 Qt key event。
- [ ] 不做完整国际键盘布局，本轮按 US/通用游戏键位布局。
- [ ] 不做持久化 UI 设置，例如最近使用按键、窗口大小。

## UX Contract

入口：

- [ ] BindingEditor 的 `Action output` 区域不再显示 ComboBox。
- [ ] 改为一个只读选择框，显示当前 pending action，例如 `Keyboard: Space`。
- [ ] 选择框右侧或下方提供 `Choose...` 按钮。
- [ ] 点击 `Choose...` 打开映射选择器。
- [ ] 如果 `selectedControl` 为空，`Choose...` 可打开但 Confirm 禁用，并显示 `Select an input first`；或者直接禁用入口。推荐：禁用入口，减少无效操作。

选择器：

- [ ] 使用 overlay / modal 风格，不新开原生窗口。
- [ ] 默认打开 Keyboard 页，并高亮当前 pending action。
- [ ] 如果当前 pending action 是 MouseButton，则默认打开 Mouse 页。
- [ ] 选择器顶部显示当前手柄输入，例如 `Mapping button_south to...`。
- [ ] Keyboard / Mouse 页签放在底部或顶部，文案为 `Keyboard` / `Mouse`。
- [ ] Confirm 按钮只有在存在 highlighted action 且 `selectedControl` 非空时可用。
- [ ] Confirm 文案使用 `Confirm`，Cancel 文案使用 `Cancel`。
- [ ] Enter 键等同 Confirm，Esc 键等同 Cancel。

高亮规则：

- [ ] 鼠标悬停只改变 hover 样式，不改变 pending action。
- [ ] 点击虚拟按键才改变 pending action。
- [ ] 物理键盘按键事件匹配成功时改变 pending action 并高亮对应虚拟按键。
- [ ] 如果按下的物理键不在本轮支持列表中，不改变 pending action，可显示短暂 hint：`Unsupported key`。
- [ ] Confirm 后绑定的是当前 highlighted action，不是 hover 项。

## QML Components

新增组件：

- [ ] `MappingPickerDialog.qml`
  - [ ] modal overlay 容器。
  - [ ] required `theme`。
  - [ ] required `appController`。
  - [ ] property `selectedControl`。
  - [ ] property `initialKind`。
  - [ ] property `initialValue`。
  - [ ] signal `accepted(string kind, string value)`。
  - [ ] signal `cancelled()`。
  - [ ] function `openFor(controlId, kind, value)`。
  - [ ] 内部维护 `pendingKind` / `pendingValue` / `pendingDisplayText`。

- [ ] `KeyboardPicker.qml`
  - [ ] 展示键盘网格。
  - [ ] 接收 `pendingKind` / `pendingValue`。
  - [ ] signal `keySelected(string value)`。
  - [ ] function `handleQtKey(int key, int modifiers)`，识别支持的 Qt key。
  - [ ] 不直接调用 AppController。

- [ ] `MousePicker.qml`
  - [ ] 展示鼠标按钮网格。
  - [ ] 接收 `pendingKind` / `pendingValue`。
  - [ ] signal `mouseActionSelected(string value)`。
  - [ ] 支持 Left / Right / Middle；Button4 / Button5 先作为 UI 项预留，但只有 AppController 支持后才启用。
  - [ ] 本轮如果 AppController 只支持 Left / Right / Middle，则 Button4 / Button5 置灰并不可 Confirm。

- [ ] `PickerKey.qml`
  - [ ] 单个可点击 tile。
  - [ ] properties：`label`、`kind`、`value`、`selected`、`enabled`、`wide`。
  - [ ] selected 时使用 accent border / fill。
  - [ ] disabled 时降低 opacity。

可选：如果组件数量过多，本轮可以把 `PickerKey` 内联在 `KeyboardPicker.qml`，但推荐拆出，避免键盘和鼠标重复样式。

## Supported Keyboard Actions

本轮支持的 Keyboard action 必须和 `ActionCatalogModel` 当前 catalog 对齐：

- [ ] Special：Space、Enter、Escape、Tab。
- [ ] Letters：A-Z。
- [ ] Digits：0-9。
- [ ] Arrows：ArrowUp、ArrowDown、ArrowLeft、ArrowRight。

Keyboard 页面布局可以额外显示以下键，但必须按是否支持区分：

- [ ] Backspace、Delete、Home、End、PageUp、PageDown、F1-F12、Ctrl、Alt、Shift 等本轮如果不在 catalog 中，置灰或不显示。
- [ ] 不允许点击后生成 catalog 不存在的 value。

设计约束：

- [ ] QML 布局中所有可 Confirm 的 key 都必须能通过 `appController.actionCatalogModel.findIndex("Keyboard", value)` 找到。
- [ ] `KeyboardPicker.handleQtKey()` 映射出的 value 也必须能通过 catalog 找到。
- [ ] 如果未来扩展 catalog，先扩展 `ActionCatalogModel`，再让 picker 启用对应 tile。

## Mouse Actions

本轮支持的 Mouse action 必须和 AppController 当前 MouseButton 分支对齐：

- [ ] `MouseButton / Left`
- [ ] `MouseButton / Right`
- [ ] `MouseButton / Middle`

不支持项：

- [ ] Wheel Up / Wheel Down 本轮不 Confirm，因为 `MouseWheel` 尚未接入 applySelectedBinding。
- [ ] Button4 / Button5 本轮不 Confirm，因为 `SMouseButtonAction` 后端映射当前只验证 0/1/2。

UI 表达：

- [ ] unsupported mouse items 可以显示为 disabled tile，保留后续扩展位置。
- [ ] disabled tile 点击不改变 pending action。

## BindingEditor Integration

替换当前 ComboBox：

- [ ] 删除 `ComboBox actionComboBox`。
- [ ] 保留 `_selectedActionIndex` 作为兼容 state，或改为 `_selectedActionKind` / `_selectedActionValue` / `_selectedActionDisplayText` 三个普通 property。
- [ ] 推荐改为三属性，避免所有状态都绕 catalog index：
  - [ ] `property string _selectedActionKind: "Keyboard"`
  - [ ] `property string _selectedActionValue: "Space"`
  - [ ] `property string _selectedActionDisplayText: "Keyboard: Space"`
- [ ] 新增 helper：`setPendingAction(kind, value)`，通过 catalog 查 displayText，找不到则拒绝并显示 warning。
- [ ] `Current mappings` 行点击时调用 `setPendingAction(actionKind, actionValue)`，保持回填行为。
- [ ] `Choose...` 打开 `MappingPickerDialog.openFor(selectedControl, _selectedActionKind, _selectedActionValue)`。
- [ ] `MappingPickerDialog.accepted(kind, value)` 后：
  - [ ] 调 `setPendingAction(kind, value)`。
  - [ ] 调 `appController.applySelectedBinding(selectedControl, kind, value)`。
  - [ ] 成功后显示 `Applied and saved` / `Applied, save failed`。
  - [ ] 失败后显示 `Apply failed`，选择器关闭与否按下方策略。

Confirm 失败策略：

- [ ] 如果后端返回 false，选择器保持打开，并在 dialog 内显示 inline error。
- [ ] 如果成功，选择器关闭。
- [ ] Cancel 永远关闭。

## Keyboard Event Handling

选择器打开时：

- [ ] `MappingPickerDialog` 调用 `forceActiveFocus()`。
- [ ] `Keys.onPressed` 捕获支持按键。
- [ ] `Escape`：Cancel。
- [ ] `Return` / `Enter`：如果 Confirm 可用，则 Confirm。
- [ ] 其他 supported key：切到 Keyboard 页，更新 pending action，高亮对应 tile，并 `event.accepted = true`。
- [ ] unsupported key：显示短暂 hint，不覆盖当前 pending action。

Qt key 到 catalog value 映射：

- [ ] `Qt.Key_A` - `Qt.Key_Z` -> `"A"` - `"Z"`。
- [ ] `Qt.Key_0` - `Qt.Key_9` -> `"0"` - `"9"`。
- [ ] `Qt.Key_Space` -> `"Space"`。
- [ ] `Qt.Key_Return` / `Qt.Key_Enter` -> `"Enter"` only when not being used as Confirm? 推荐：如果 no pending action 或 focused tile is Enter，可以选 Enter；否则 Enter confirms. 本轮采用 Confirm 优先。
- [ ] `Qt.Key_Escape` -> Cancel，不作为 Escape action；如需绑定 Escape，用户点击虚拟 ESC tile。
- [ ] `Qt.Key_Tab` -> `"Tab"`，需要 `KeyNavigation` 不抢走焦点。
- [ ] Arrow keys -> `"ArrowUp"` / `"ArrowDown"` / `"ArrowLeft"` / `"ArrowRight"`。

说明：

- [ ] Enter / Escape 作为 dialog 控制键和可绑定键存在冲突。本轮让物理 Enter/Escape 控制 dialog；用户仍可通过点击虚拟 Enter/Escape tile 选择这些输出。

## Tests

QML smoke：

- [ ] 新增 QML 文件加入 `qt_add_qml_module`。
- [ ] `MappyZQmlSmokeTests` 仍无 warning。

UI Bridge tests：

- [ ] 不需要新增 AppController API 测试；Confirm 仍调用现有 `applySelectedBinding()`。
- [ ] 如果扩展 `ActionCatalogModel` 支持新的键，必须补对应 tests。
- [ ] 本轮不扩展 catalog 时，现有 ActionCatalog tests 不变。

QML interaction tests（如果当前测试基础允许）：

- [ ] 创建 `MappingPickerDialog`，调用 `openFor("button_south", "Keyboard", "Space")` 后 pending action 为 Space。
- [ ] 调用 `KeyboardPicker.handleQtKey(Qt.Key_A, 0)` 后 pending action 高亮 A。
- [ ] 点击 Confirm 后发出 `accepted("Keyboard", "A")`。
- [ ] 点击 Cancel 后不发 accepted。
- [ ] unsupported key 不改变 pending action。

如果 QML 交互测试成本过高，本轮至少：

- [ ] 用 QML smoke 覆盖组件创建。
- [ ] 把 key mapping 逻辑写成 QML function，并在后续 QML test harness 中补测。

## Acceptance Criteria

- [ ] BindingEditor 不再使用 action ComboBox 作为主要选择方式。
- [ ] 用户可以打开键盘 / 鼠标映射选择器。
- [ ] 用户点击虚拟键盘按键后，该按键高亮。
- [ ] 选择器获得焦点时，用户按物理键盘 A-Z / 0-9 / Space / Tab / Arrow 能让对应虚拟按键高亮。
- [ ] 点击 Confirm 后当前 selectedControl 与 highlighted action 自动绑定。
- [ ] 绑定成功后 Current mappings 立即更新。
- [ ] 点击 mapping 行仍能回填 selectedControl 和 pending action。
- [ ] Cancel / Esc 不修改映射。
- [ ] QML smoke 无 binding/import/property warning。
- [ ] 所有测试通过。
- [ ] 修改文本文件保持 CRLF 行尾。

## Follow-Up

- [ ] 扩展 ActionCatalog：Backspace、Delete、Home、End、PageUp、PageDown、F1-F12、modifier keys。
- [ ] 接入 MouseWheel。
- [ ] 支持 mouse Button4 / Button5。
- [ ] 支持组合键和 modifier 状态。
- [ ] 增加专门 QML interaction test harness。
- [ ] 做更接近 AntiMicroX 的完整键盘布局缩放和小窗口适配。
