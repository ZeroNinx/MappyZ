# TODO: QML Main Split

## Next Step

下一步只做 `ui/Main.qml` 的合理拆分，把当前单文件 UI 拆成若干职责清晰的 QML 组件，降低后续维护成本。

本轮目标是结构整理，不改交互语义、不加新功能、不改 C++ API。

优先做这个模块的理由：

- [x] `Main.qml` 已经包含主题、通用控件、状态代理、设备面板、手柄视图、绑定编辑器、事件日志和状态栏，职责过多。
- [x] 后续 Binding Editor / Log / Profile UI 都会继续膨胀，继续堆在 `Main.qml` 会增加误改风险。
- [x] 近期已经修过 QML signal 绑定问题，拆分时需要显式梳理组件边界和依赖，避免再次依赖隐藏上下文。

## Scope

包含：

- [x] 新增独立 QML 组件文件。
- [x] 将 `Main.qml` 中的 inline reusable components 移出。
- [x] 将主要区域面板拆成独立文件。
- [x] `Main.qml` 保留应用级状态、生命周期调用、全局 Connections 和整体布局编排。
- [x] 更新 `CMakeLists.txt` 的 `qt_add_qml_module(QML_FILES ...)`。
- [x] 保持现有 UI 外观、布局、按钮行为、输入刷新行为不变。
- [x] 构建、测试、QML offscreen 启动验证。

不做：

- [x] 不修改 `ZAppController`、`ZDeviceModel`、`ZInputStateModel`、`ZInputCaptureModel`。
- [x] 不新增 C++ 类。
- [x] 不实现 mapping rule 创建、profile 保存、日志模型或真实 mapping 列表。
- [x] 不重做视觉设计。
- [x] 不改 signal 名称、QML 事件流或 capture 行为。
- [x] 不引入 Qt Quick Controls 依赖。

## Target File Layout

新增文件建议放在 `ui/` 下，先保持一层目录，避免本轮额外处理 import/path 复杂度：

- [x] `ui/Theme.qml`
- [x] `ui/Panel.qml`
- [x] `ui/ActionButton.qml`
- [x] `ui/Tag.qml`
- [x] `ui/FieldLabel.qml`
- [x] `ui/ValueText.qml`
- [x] `ui/InputControlState.qml`
- [x] `ui/ControlDot.qml`
- [x] `ui/TopBar.qml`
- [x] `ui/DevicesPanel.qml`
- [x] `ui/GamepadView.qml`
- [x] `ui/BindingEditor.qml`
- [x] `ui/EventLogPanel.qml`
- [x] `ui/StatusBar.qml`

`Main.qml` 目标职责：

- [x] 创建 `Window`。
- [x] 持有 app-level state：`selectedDevice`、`selectedDeviceDisplayName`、`selectedControl`、`selectedAction`、`latestControlForSelectedDevice`。
- [x] 持有临时 demo models：`mappingModel`、`eventModel`。
- [x] 处理 `appController.initializeRuntime()` / `startRuntime()` / `startPumpTimer()` / shutdown。
- [x] 处理设备选择和 latest input 的顶层 `Connections`。
- [x] 组合 `TopBar`、`DevicesPanel`、`GamepadView`、`BindingEditor`、`EventLogPanel`、`StatusBar`。

## Component Contracts

`Theme.qml`：

- [x] 使用 `QtObject`。
- [x] 暴露当前颜色属性：`window`、`panel`、`panelHeader`、`surface`、`border`、`text`、`muted`、`accent`、`accentSoft`、`accentHover`、`success`、`warning`、`danger`。
- [x] 不做 singleton，先由 `Main.qml` 实例化并作为 `theme` property 传给子组件。

`Panel.qml`：

- [x] 从 inline `component Panel` 提取。
- [x] `required property var theme`。
- [x] `property string heading`。
- [x] `default property alias content: contentHost.data`，让调用处可以直接嵌套内容。
- [x] 保持标题栏、边框、内边距和 clipping 不变。

`ActionButton.qml`：

- [x] 从 inline `component ActionButton` 提取。
- [x] `required property var theme`。
- [x] `property string label`。
- [x] `property bool primary`。
- [x] `signal clicked()`。
- [x] 保持 hover、颜色、尺寸和 cursor 行为不变。

`Tag.qml`：

- [x] 从 inline `component Tag` 提取。
- [x] `required property var theme`。
- [x] `property string label`。
- [x] `property color tone`。
- [x] 保持尺寸、圆角和文字样式不变。

`FieldLabel.qml` / `ValueText.qml`：

- [x] 从 inline `component FieldLabel` / `ValueText` 提取。
- [x] `required property var theme`。
- [x] 保持当前字体、颜色和 elide 行为。

`InputControlState.qml`：

- [x] 从 inline `component InputControlState` 提取。
- [x] `required property var inputStateModel`。
- [x] `required property string deviceId`。
- [x] `required property string controlId`。
- [x] 保留 `pressed`、`value`、`axisX`、`axisY`、`displayValue`。
- [x] 保留 `refresh()` / `reset()`。
- [x] `Connections.target` 使用传入的 `inputStateModel`，不直接引用 `appController`。
- [x] signal handler 继续只在 device/control 匹配时刷新。

`ControlDot.qml`：

- [x] 从 inline `component ControlDot` 提取。
- [x] `required property var theme`。
- [x] `required property var inputStateModel`。
- [x] `required property string selectedDevice`。
- [x] `required property string selectedControl`。
- [x] `property string controlId`。
- [x] `property string label`。
- [x] `signal selected(string controlId)`。
- [x] 内部使用 `InputControlState` 驱动 active。
- [x] 点击时只发 `selected(controlId)`，由父组件决定是否取消 capture 或更新 selected control。

`TopBar.qml`：

- [x] 接收 `theme`、`appController`、`eventModel`。
- [x] 保持产品名、runtime subtitle、profile tag、mapping toggle、Save Profile 行为不变。
- [x] 不持有 app state。

`DevicesPanel.qml`：

- [x] 接收 `theme`、`appController`、`selectedDevice`、`selectedDeviceDisplayName`。
- [x] 暴露 `signal deviceSelected(string deviceId, string displayName)`。
- [x] 内部保留 `Repeater { model: appController.deviceModel }`。
- [x] 点击设备卡片时发 `deviceSelected(...)`。
- [x] 保留 runtime message 卡片。
- [x] 暴露 `property int deviceCount`，供 `StatusBar` 或 `Main.qml` 使用；或由 `Main.qml` 直接读 `appController.deviceModel.rowCount()`，二选一即可。

`GamepadView.qml`：

- [x] 接收 `theme`、`appController`、`selectedDevice`、`selectedDeviceDisplayName`、`selectedControl`、`latestControlForSelectedDevice`。
- [x] 暴露 `signal controlSelected(string controlId)`。
- [x] 暴露 `signal actionButtonControlSelected(string controlId)`，用于 Back / Start 这类非 `ControlDot` 控件。
- [x] 内部使用 `ControlDot` 和 `InputControlState`。
- [x] 控件点击只发 `controlSelected(controlId)`。
- [x] 父级 `Main.qml` 收到后设置 `selectedControl` 并调用 `appController.inputCapture.cancel()`，保持当前行为。
- [x] Back / Start 按钮继续只更新 `selectedControl`，不取消 capture；父级 `Main.qml` 收到 `actionButtonControlSelected(controlId)` 后只设置 `selectedControl`。
- [x] 保持手柄布局、LT/RT 数值显示、stick 偏移逻辑不变。

`BindingEditor.qml`：

- [x] 接收 `theme`、`appController`、`selectedDevice`、`selectedControl`、`selectedAction`、`mappingModel`。
- [x] 暴露 `signal clearControlRequested()`，Clear 按钮不直接写父级状态。
- [x] 暴露 `signal selectedActionChangedByUi(string actionText)`，Keyboard / Mouse 按钮不直接写父级状态。
- [x] Capture 按钮仍调用 `appController.inputCapture.begin(selectedDevice)` / `cancel()`。
- [x] Clear 按钮发 `clearControlRequested()`，由 `Main.qml` 设置 `selectedControl = ""`。
- [x] Keyboard / Mouse 按钮发 `selectedActionChangedByUi(...)`，由 `Main.qml` 更新 `selectedAction`。
- [x] 保持 current mappings demo list 不变。

`EventLogPanel.qml`：

- [x] 接收 `theme`、`eventModel`。
- [x] 保持现有 demo log list 渲染不变。
- [x] 不新增真实 log 数据源。

`StatusBar.qml`：

- [x] 接收 `theme`、`appController`、`deviceCount`。
- [x] 保持 status text 内容不变。
- [x] 不直接依赖 `deviceRepeater` id。

## Main.qml Refactor Plan

- [x] 保留 `Window` 根对象和尺寸、标题、背景颜色。
- [x] 用 `Theme { id: theme }` 替换 inline `QtObject theme`。
- [x] 删除 inline `component Panel`、`ActionButton`、`Tag`、`FieldLabel`、`ValueText`、`InputControlState`、`ControlDot`。
- [x] 用组件实例替换 `topBar`、`devicePanel`、`gamepadPanel`、`bindingPanel`、`eventPanel`、`statusBar` 的内部实现。
- [x] 组件之间不要互相读取兄弟组件 id；布局锚点仍由 `Main.qml` 管理。
- [x] `Main.qml` 继续作为 app state owner，子组件通过 signals 请求状态变更。
- [x] `Main.qml` 继续持有 `_findDeviceRow()`，除非 `DevicesPanel` 内部需要私有查找；避免重复逻辑。
- [x] 所有跨组件数据依赖都通过 `required property` 或 signal 显式传递。

## CMake Plan

- [x] 更新 `qt_add_qml_module(MappyZ QML_FILES ...)`，加入所有新增 QML 文件。
- [x] 为 `ui/*.qml` 设置 `QT_RESOURCE_ALIAS` 到模块根文件名，保持 `MappyZUI` 模块内组件名稳定并避免 Qt QTP0004 extra directory warning。
- [x] 不移动 QML module URI，继续使用 `MappyZUI 1.0`。

## Tests And Verification

自动验证：

- [x] `cmake --build build --config Debug` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `QT_QPA_PLATFORM=offscreen` 启动 `MappyZ.exe`，stderr 不出现 QML import/component/property/binding 错误。

QML 手工验证：

- [x] 应用启动后窗口布局与拆分前一致。
- [x] 设备列表显示和点击选择行为不变。
- [x] 手柄输入高亮、LT/RT 数值、摇杆偏移仍更新。
- [x] `Capture Input`、capture 完成、点击 `ControlDot` 取消 capture 行为不变。
- [x] Mapping On/Off 按钮行为不变。
- [x] Save Profile demo 日志插入行为不变。
- [x] Event Log 和 Status Bar 显示内容不变。

## Acceptance Criteria

- [x] `Main.qml` 明显缩小，主要负责 app state、lifecycle、Connections 和页面布局。
- [x] 可复用控件不再作为 `Main.qml` inline component 存在。
- [x] 每个新 QML 文件职责单一，依赖通过 `required property` / `signal` 表达。
- [x] 没有新增业务行为、视觉改版或 C++ API 改动。
- [x] QML startup 无 binding/import/signal handler warning。
- [x] 所有验证命令通过。

## Follow-Up Module

- [x] 后续再做 Binding Editor 的真实 rule 创建。
- [x] 后续再做 active profile 修改和保存。
- [x] 后续再做 `ZLogModel` 和真实事件日志。
- [x] 如果拆分后组件边界稳定，再考虑把 `Theme.qml` 改成 QML singleton。
