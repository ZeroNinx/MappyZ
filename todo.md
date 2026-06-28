# TODO: Keyboard / Mouse Mapping Picker

## Goal

把 BindingEditor 里的 `Action output` 下拉框替换成一个独立的映射目标选择器。

参考视觉见：

- `docs/mapping_panel_keyboard.png`
- `docs/mapping_panel_mouse.png`
- `docs/mapping_panel_dinput.png`

目标交互：用户选中一个手柄输入后，打开选择器，手动点选键盘或鼠标输出；选择器获得焦点时，用户按物理键盘按键也会让对应虚拟按键实时高亮；点击 Confirm 后立即把当前手柄输入和高亮输出绑定。

本轮只做 Keyboard / MouseButton 的 UI 和处理链路。DInput 作为未来扩展方向在 UI 中保留入口，但不实现可绑定功能。

重要产品决策：

- [x] 本轮不提供 `Mouse: Move Cursor` 新建入口。
- [x] 之前的 MouseMove / Cursor 只是早期技术验证，不作为当前 UI 主流程继续暴露。
- [x] 摇杆到鼠标移动未来会作为独立模块重做，不在本轮选择器里临时塞一个不完整入口。
- [x] 后续更可能的方向是把摇杆拆解为 4/8 个方向或轴向虚拟输入，再映射到普通按键/鼠标动作；完整模拟鼠标移动另做独立配置页。

## Scope

包含：

- [x] 新增 QML 映射选择器，替换 BindingEditor 中的 action ComboBox。
- [x] 选择器支持 Keyboard / Mouse / DInput 三个页签。
- [x] Keyboard 页按 `docs/mapping_panel_keyboard.png` 的布局和视觉密度实现。
- [x] Mouse 页按 `docs/mapping_panel_mouse.png` 的布局和视觉密度实现。
- [x] DInput 页按 `docs/mapping_panel_dinput.png` 预留静态/禁用布局，不允许 Confirm。
- [x] 用户点击虚拟按键时，该按键高亮并成为 pending action。
- [x] 选择器拥有焦点时，按物理键盘按键会实时高亮对应虚拟键。
- [x] 点击 Confirm 后自动调用现有 `applySelectedBinding(selectedControl, kind, value)`。
- [x] Confirm 成功后关闭选择器，并复用 BindingEditor 的 Apply feedback。
- [x] Confirm 失败时选择器保持打开，并在选择器内显示 inline error。
- [x] Cancel / Esc 关闭选择器，不修改映射。
- [x] 当前 mappings 行点击后仍能回填 selected input 和 pending action。
- [x] QML smoke 覆盖新组件无 warning。

不做：

- [x] 不做 DInput 功能绑定；DInput 页只作为 disabled preview / future tab。
- [x] 不做组合键，例如 Ctrl+Shift+A。
- [x] 不做宏、长按、双击、turbo、toggle。
- [x] 不做 mouse move / axis 参数 UI，也不提供 `Mouse: Move Cursor` fallback 入口。
- [x] 本轮不新增 stick -> MouseMove 绑定入口；已有旧 profile 如包含 MouseMove，只保证不崩溃，不保证可编辑重建。
- [x] 不做全局 OS 级键盘钩子；只捕获选择器获得焦点时的 Qt key event。
- [x] 不做完整国际键盘布局，本轮按 US/通用游戏键位布局。
- [x] 不做持久化 UI 设置，例如最近使用按键、窗口大小。

## Visual Contract

整体弹窗：

- [x] 使用 app 内 modal overlay，不新开原生窗口。
- [x] 视觉接近参考图：深色半透明层次、细边框、轻微高光、蓝色 accent。
- [x] 弹窗尺寸使用大面板，不塞进 BindingEditor 小卡片；桌面端目标宽度约占窗口 80%-90%。
- [x] 顶部区域包含标题 `选择映射目标`，左侧可放简单 gamepad icon 或文字标识。
- [x] 标题下方居中显示上下文文案：`为 "<selectedControl display>" 选择映射目标`。
- [x] 页签位于内容区上方左侧，顺序为 `键盘` / `鼠标` / `DInput`。
- [x] 当前页签底部使用蓝色 3px 左右 accent line。
- [x] 内容区域是单个大 panel，不嵌套多层卡片。
- [x] 右下角固定放 `取消` / `确定`，`确定` 使用 primary 蓝色。
- [x] 右上角不需要复制系统窗口按钮；作为 app 内 modal，使用 Cancel / Esc 关闭即可。

Tile 风格：

- [x] 所有可选目标使用统一 tile：深灰渐变或深灰填充、1px border、4px radius。
- [x] hover 只改变 hover 样式，不改变 pending action。
- [x] selected 使用蓝色 border / glow / accent fill，必须比 hover 更明显。
- [x] disabled tile 降低 opacity，禁止点击，不改变 pending action。
- [x] tile 文本居中，长文本允许缩小字号或换行，不溢出。

布局响应：

- [x] 桌面宽度下键盘布局尽量接近参考图，不出现横向滚动。
- [x] 小窗口下内容区允许等比缩放或内部滚动，但 Confirm / Cancel 始终可见。
- [x] 键盘、鼠标、DInput 三页切换不改变弹窗整体尺寸，避免布局跳动。

## UX Contract

入口：

- [x] BindingEditor 的 `Action output` 区域不再显示 ComboBox。
- [x] 改为一个只读选择框，显示当前 pending action，例如 `Keyboard: Space`。
- [x] 选择框右侧或下方提供 `Choose...` 按钮。
- [x] 点击 `Choose...` 打开映射选择器。
- [x] 如果 `selectedControl` 为空，`Choose...` 禁用，并显示现有 `Select an input first`。

选择器：

- [x] 默认打开 Keyboard 页，并高亮当前 pending action。
- [x] 如果当前 pending action 是 MouseButton，则默认打开 Mouse 页。
- [x] 如果当前 pending action 是 MouseMove，本轮默认打开 Mouse 页并显示 read-only/unsupported hint；Confirm 禁用，用户需要重新选择 Keyboard 或 MouseButton。
- [x] DInput 页可点击切换，但显示 disabled/future state，Confirm 禁用。
- [x] Confirm 按钮只有在存在 highlighted action、`selectedControl` 非空、当前页可绑定时可用。
- [x] Confirm 文案使用 `确定`，Cancel 文案使用 `取消`。
- [x] Enter 键等同 Confirm，Esc 键等同 Cancel。

高亮规则：

- [x] 鼠标悬停只改变 hover 样式，不改变 pending action。
- [x] 点击虚拟按键才改变 pending action。
- [x] 物理键盘按键事件匹配成功时改变 pending action 并高亮对应虚拟按键。
- [x] 如果按下的物理键不在本轮支持列表中，不改变 pending action，可显示短暂 hint：`Unsupported key`。
- [x] Confirm 后绑定的是当前 highlighted action，不是 hover 项。

## QML Components

新增组件：

- [x] `MappingPickerDialog.qml`
  - [x] modal overlay 容器。
  - [x] required `theme`。
  - [x] required `appController`。
  - [x] property `selectedControl`。
  - [x] property `initialKind`。
  - [x] property `initialValue`。
  - [x] signal `accepted(string kind, string value)`。
  - [x] signal `cancelled()`。
  - [x] function `openFor(controlId, kind, value)`。
  - [x] 内部维护 `pendingKind` / `pendingValue` / `pendingDisplayText` / `currentTab`。

- [x] `PickerTabs.qml`
  - [x] 三段 tab：`键盘` / `鼠标` / `DInput`。
  - [x] DInput tab 可切换但内容不可绑定。
  - [x] 当前 tab 使用蓝色底边。

- [x] `KeyboardPicker.qml`
  - [x] 展示键盘网格。
  - [x] 接收 `pendingKind` / `pendingValue`。
  - [x] signal `keySelected(string value)`。
  - [x] function `handleQtKey(int key, int modifiers)`，识别支持的 Qt key。
  - [x] 不直接调用 AppController。

- [x] `MousePicker.qml`
  - [x] 展示鼠标按钮与鼠标移动区域。
  - [x] 接收 `pendingKind` / `pendingValue`。
  - [x] signal `mouseActionSelected(string value)`。
  - [x] 支持 Left / Right / Middle。
  - [x] Wheel / side buttons / movement tiles 本轮按能力置灰且不可 Confirm。
  - [x] MouseMove / Cursor 不作为 enabled tile 出现。

- [x] `DInputPicker.qml`
  - [x] 展示 DInput 未来页布局：摇杆/轴、扳机、D-Pad、Buttons。
  - [x] 所有 tile disabled。
  - [x] 显示提示：`DInput mapping is planned, not available yet`。
  - [x] 不发出 action selected 信号。

- [x] `PickerKey.qml`
  - [x] 单个可点击 tile。
  - [x] properties：`label`、`kind`、`value`、`selected`、`enabled`、`wide`。
  - [x] selected 时使用 accent border / fill。
  - [x] disabled 时降低 opacity。

可选：如果组件数量过多，本轮可以把 `PickerKey` 内联在 `KeyboardPicker.qml`，但推荐拆出，避免键盘、鼠标、DInput 重复样式。

## Keyboard Page

参考 `docs/mapping_panel_keyboard.png`。

布局要求：

- [x] 顶部 function row：Esc、F1-F12、PrtSc、ScrLk、Pause。当前 catalog 不支持的 function keys 置灰。
- [x] 主键区：数字行、QWERTY、ASDF、ZXCV、Space 行。
- [x] 右侧保留导航键区：Insert、Home、PageUp、Delete、End、PageDn。当前 catalog 不支持则置灰。
- [x] 右下保留方向键区，ArrowUp / ArrowDown / ArrowLeft / ArrowRight 可用。
- [x] 数字小键盘可显示但本轮大部分置灰，除 0-9 可按 catalog 映射为普通 digit。
- [x] Caps / NumLock 的绿点只是视觉状态占位，不接真实锁定状态。

本轮支持的 Keyboard action 必须和 `ActionCatalogModel` 当前 catalog 对齐：

- [x] Special：Space、Enter、Escape、Tab。
- [x] Letters：A-Z。
- [x] Digits：0-9。
- [x] Arrows：ArrowUp、ArrowDown、ArrowLeft、ArrowRight。

约束：

- [x] 所有可 Confirm 的 key 都必须能通过 `appController.actionCatalogModel.findIndex("Keyboard", value)` 找到。
- [x] `KeyboardPicker.handleQtKey()` 映射出的 value 也必须能通过 catalog 找到。
- [x] 如果未来扩展 catalog，先扩展 `ActionCatalogModel`，再让 picker 启用对应 tile。

## Mouse Page

参考 `docs/mapping_panel_mouse.png`。

布局要求：

- [x] 左侧区域标题 `鼠标按钮`。
- [x] 左侧展示 Left / Right / Middle 三个可用 tile。
- [x] Wheel Up / Wheel Down 显示为 disabled tile，因为 `MouseWheel` 尚未接入 apply。
- [x] 中间展示鼠标轮廓或简化图形，作为视觉锚点；不要求真实图片资源，可用 QML shapes / rectangles 实现。
- [x] Side Button 1 / Side Button 2 显示为 disabled tile，保留后续扩展位置。
- [x] 右侧区域标题 `鼠标移动`。
- [x] 鼠标左移/右移/上移/下移 tile 本轮 disabled；不生成 `MouseMove / Cursor`。
- [x] 不添加 `Mouse: Move Cursor` enabled tile；避免把未完成的模拟鼠标移动继续暴露给用户。
- [x] Hint 文案：`提示：点击图标或按钮即可选择`。

本轮可 Confirm 的 Mouse action：

- [x] `MouseButton / Left`
- [x] `MouseButton / Right`
- [x] `MouseButton / Middle`

不支持项：

- [x] Wheel Up / Wheel Down 本轮不 Confirm。
- [x] Button4 / Button5 本轮不 Confirm。
- [x] Mouse movement direction tile 本轮不 Confirm。
- [x] MouseMove / Cursor 本轮不 Confirm。

## MouseMove Backout Policy

上一轮已经实现了 `MouseMove / Cursor` 技术链路，但当前产品决策是暂缓暴露：

- [x] Picker 不显示可用的 `Mouse: Move Cursor`。
- [x] BindingEditor 删除 ComboBox 后不再提供 MouseMove 新建路径。
- [x] 如果 `ActionCatalogModel` 中仍保留 MouseMove 项，本轮 picker 不消费它。
- [ ] 如果要彻底回退 UI Bridge 入口，单独做 cleanup commit：从 `ActionCatalogModel` 移除 `MouseMove / Cursor`，并移除/调整对应 AppController UI 测试。
- [x] Core / Runtime / OutputBackend 中已有 Axis2D -> MouseMove 能力可以保留为底层能力，等待未来完整模块重新接入。

未来设计方向：

- [ ] 摇杆拆解为 4/8 方向虚拟输入，例如 Stick Up / Down / Left / Right。
- [ ] 这些虚拟方向优先映射为普通 Keyboard / MouseButton 动作。
- [ ] 完整模拟鼠标移动需要独立配置：deadzone、sensitivity、曲线、Y 轴反转、加速度等。

## DInput Page

参考 `docs/mapping_panel_dinput.png`。

本轮定位：

- [x] DInput 是 future tab，只做视觉占位。
- [x] 用户可以切到 DInput 页查看布局，但所有 tile disabled。
- [x] Confirm 在 DInput 页禁用。
- [x] 不修改 `ActionCatalogModel`。
- [x] 不修改 `applySelectedBinding()`。

布局要求：

- [x] 左上 `摇杆 / 轴（模拟）` 分区：左/右摇杆 X/Y，X-/X+、Y-/Y+。
- [x] 中上 `扳机（轴）` 分区：LT / RT，轴 +。
- [x] 右上 `方向键（D-Pad）` 分区：上/下/左/右。
- [x] 下方 `按钮（Button）` 分区：B1-B16。
- [x] 顶部 hint：`DInput support is planned for a future module` 或中文等价文案。

## BindingEditor Integration

替换当前 ComboBox：

- [x] 删除 `ComboBox actionComboBox`。
- [x] 改为 `_selectedActionKind` / `_selectedActionValue` / `_selectedActionDisplayText` 三个普通 property：
  - [x] `property string _selectedActionKind: "Keyboard"`
  - [x] `property string _selectedActionValue: "Space"`
  - [x] `property string _selectedActionDisplayText: "Keyboard: Space"`
- [x] 新增 helper：`setPendingAction(kind, value)`，通过 catalog 查 displayText，找不到则拒绝并显示 warning。
- [x] `Current mappings` 行点击时调用 `setPendingAction(actionKind, actionValue)`，保持回填行为。
- [x] 如果 mapping 行是 `MouseMove / Cursor`，可以选中 input，但 pending action 显示为 unsupported/read-only，不允许直接 Confirm 重建该规则。
- [x] `Choose...` 打开 `MappingPickerDialog.openFor(selectedControl, _selectedActionKind, _selectedActionValue)`。
- [x] `MappingPickerDialog.accepted(kind, value)` 后：
  - [x] 调 `setPendingAction(kind, value)`。
  - [x] 调 `appController.applySelectedBinding(selectedControl, kind, value)`。
  - [x] 成功后显示 `Applied and saved` / `Applied, save failed`。
  - [x] 失败后显示 `Apply failed`，选择器保持打开。

Confirm 失败策略：

- [x] 如果后端返回 false，选择器保持打开，并在 dialog 内显示 inline error。
- [x] 如果成功，选择器关闭。
- [x] Cancel 永远关闭。

## Keyboard Event Handling

选择器打开时：

- [x] `MappingPickerDialog` 调用 `forceActiveFocus()`。
- [x] `Keys.onPressed` 捕获支持按键。
- [x] `Escape`：Cancel。
- [x] `Return` / `Enter`：如果 Confirm 可用，则 Confirm。
- [x] 其他 supported key：切到 Keyboard 页，更新 pending action，高亮对应 tile，并 `event.accepted = true`。
- [ ] unsupported key：显示短暂 hint，不覆盖当前 pending action。

Qt key 到 catalog value 映射：

- [x] `Qt.Key_A` - `Qt.Key_Z` -> `"A"` - `"Z"`。
- [x] `Qt.Key_0` - `Qt.Key_9` -> `"0"` - `"9"`。
- [x] `Qt.Key_Space` -> `"Space"`。
- [x] `Qt.Key_Tab` -> `"Tab"`，需要避免 Tab 被焦点导航吃掉。
- [x] Arrow keys -> `"ArrowUp"` / `"ArrowDown"` / `"ArrowLeft"` / `"ArrowRight"`。

冲突规则：

- [x] 物理 Enter 用于 Confirm，不作为 Enter action；用户可点击虚拟 Enter tile 绑定 Enter。
- [x] 物理 Escape 用于 Cancel，不作为 Escape action；用户可点击虚拟 Esc tile 绑定 Escape。

## Tests

QML smoke：

- [x] 新增 QML 文件加入 `qt_add_qml_module`。
- [x] `MappyZQmlSmokeTests` 仍无 warning。

UI Bridge tests：

- [x] 不需要新增 AppController API 测试；Confirm 仍调用现有 `applySelectedBinding()`。
- [x] 本轮不扩展 `ActionCatalogModel` 时，现有 ActionCatalog tests 不变。
- [x] 如果实施时扩展 catalog 支持更多键，必须补对应 tests。

QML interaction tests（如果当前测试基础允许）：

- [ ] 创建 `MappingPickerDialog`，调用 `openFor("button_south", "Keyboard", "Space")` 后 pending action 为 Space。
- [ ] 调用 `KeyboardPicker.handleQtKey(Qt.Key_A, 0)` 后 pending action 高亮 A。
- [ ] 点击 Confirm 后发出 `accepted("Keyboard", "A")`。
- [ ] 点击 Cancel 后不发 accepted。
- [ ] unsupported key 不改变 pending action。
- [ ] 切到 DInput 页后 Confirm disabled。
- [ ] MouseMove / Cursor 不出现在 picker 的可 Confirm 目标中。

如果 QML 交互测试成本过高，本轮至少：

- [x] 用 QML smoke 覆盖组件创建。
- [x] 把 key mapping 逻辑写成 QML function，并在后续 QML test harness 中补测。

## Acceptance Criteria

- [x] BindingEditor 不再使用 action ComboBox 作为主要选择方式。
- [x] 用户可以打开键盘 / 鼠标 / DInput 三页式映射选择器。
- [x] 用户点击虚拟键盘按键后，该按键高亮。
- [x] 选择器获得焦点时，用户按物理键盘 A-Z / 0-9 / Space / Tab / Arrow 能让对应虚拟按键高亮。
- [x] 用户可以在 Mouse 页选择 Left / Right / Middle。
- [x] 用户不能在本轮 picker 中新建 MouseMove / Cursor 绑定。
- [x] DInput 页可见但不可 Confirm，不产生任何绑定。
- [x] 点击 Confirm 后当前 selectedControl 与 highlighted action 自动绑定。
- [x] 绑定成功后 Current mappings 立即更新。
- [x] 点击 mapping 行仍能回填 selectedControl 和 pending action。
- [x] Cancel / Esc 不修改映射。
- [x] QML smoke 无 binding/import/property warning。
- [x] 所有测试通过。
- [x] 修改文本文件保持 CRLF 行尾。

## Follow-Up

- [ ] 扩展 ActionCatalog：Backspace、Delete、Home、End、PageUp、PageDown、F1-F12、modifier keys。
- [ ] 接入 MouseWheel。
- [ ] 支持 mouse Button4 / Button5。
- [ ] 摇杆 4/8 方向虚拟输入映射。
- [ ] 完整 MouseMove 模拟模块：direction / sensitivity / deadzone / curve / invert。
- [ ] 实现 DInput action catalog 和 apply pipeline。
- [ ] 支持组合键和 modifier 状态。
- [ ] 增加专门 QML interaction test harness。
