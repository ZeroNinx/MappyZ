# TODO: QML Structure And Mapping Picker Layout Fix

## Goal

整理 `ui/` 目录结构，避免所有 QML 文件继续堆在一个目录下；同时修复映射选择器当前截图中暴露的排版问题。

本轮只做 UI 文件组织和映射选择器布局修复，不新增映射能力，不改 Core / Runtime / Backend 行为。

## Scope

包含：

- [ ] 将 `ui/` 下的 QML 组件按职责拆入子目录。
- [ ] 保持 `Main.qml` 入口加载路径稳定。
- [ ] 保持现有 QML 类型名和主要绑定关系稳定，避免目录拆分变成功能重写。
- [ ] 修复 `MappingPickerDialog` 的 Keyboard / Mouse / DInput 页排版问题。
- [ ] 保持当前产品决策：Keyboard / MouseButton 可绑定，DInput 仅占位，MouseMove 不提供新建入口。

不包含：

- [ ] 不新增 DInput 映射功能。
- [ ] 不恢复 MouseMove / Cursor 新建入口。
- [ ] 不新增组合键、鼠标滚轮、侧键、宏、turbo、toggle。
- [ ] 不重写主题系统。
- [ ] 不改 C++ runtime / profile / output backend。

## Priority 0: Guard Rails

- [ ] 开始前记录当前 `ui/` QML 文件清单。
- [ ] 移动文件前确认所有待移动 QML 都已被 `qt_add_qml_module` 管理。
- [ ] 移动后必须保持 `Main.qml` 仍可通过当前应用入口加载。
- [ ] 所有文本文件保持 CRLF。
- [ ] 不在文档或代码中写入本机绝对路径。
- [ ] 每个阶段至少跑一次 QML smoke；最终跑 `ctest --test-dir build -C Debug --output-on-failure`。

## Priority 1: UI Directory Structure

目标结构：

```text
ui/
  Main.qml
  common/
    ActionButton.qml
    FieldLabel.qml
    InlineMessage.qml
    Panel.qml
    Tag.qml
    ValueText.qml
  gamepad/
    ControlDot.qml
    GamepadView.qml
    InputControlState.qml
  panels/
    BindingEditor.qml
    DevicesPanel.qml
    EventLogPanel.qml
    StatusBar.qml
    TopBar.qml
  picker/
    DInputPicker.qml
    KeyboardPicker.qml
    MappingPickerDialog.qml
    MousePicker.qml
    PickerKey.qml
    PickerTabs.qml
  theme/
    Theme.qml
```

Implementation plan：

- [ ] 移动文件到上述目录。
- [ ] 更新 `CMakeLists.txt` 中 `set_source_files_properties(... QT_RESOURCE_ALIAS ...)` 的源路径。
- [ ] 保持 alias 为原文件名，例如 `ui/picker/MappingPickerDialog.qml` 仍 alias 为 `MappingPickerDialog.qml`。
- [ ] 保持 QML 类型引用不变，例如 `MappingPickerDialog {}`、`ActionButton {}` 不需要改名。
- [ ] 如 Qt 对子目录 QML module 有额外 qmldir warning，优先通过稳定 alias 保持现状，不在本轮引入 QML namespace 分层。
- [ ] 确认 `Main.qml` 中组件引用全部仍可解析。
- [ ] 删除旧根目录下被移动的 QML 文件，避免同名类型重复。

Acceptance：

- [ ] `ui/` 根目录只保留 `Main.qml`，其余 QML 进入子目录。
- [ ] `MappyZQmlSmokeTests` 无 QML loading warning。
- [ ] 应用启动后主界面视觉和交互与移动前一致。

## Priority 2: Mapping Picker Dialog Shell Layout

当前问题：

- [ ] Dialog 面板过大但内容没有合理利用空间，Keyboard / Mouse / DInput 页都集中在左上，底部和右侧留白过多。
- [ ] 内容区域高度计算依赖 `parent.height - y ...`，可读性差，也容易在窗口尺寸变化时产生错误。
- [ ] 页签、当前选择、内容区、底部按钮之间缺少稳定的布局约束。

Implementation plan：

- [ ] 将 `MappingPickerDialog` 拆成清晰的 header / tabs / selection summary / content / footer 五段。
- [ ] Footer 固定在底部，Cancel / Confirm 永远可见。
- [ ] Content 区使用单独容器承载当前页，必要时内部滚动，不让按钮被内容挤出。
- [ ] 移除脆弱的 `height: parent.height - y + ...` 计算，改用 anchors 或明确的剩余高度容器。
- [ ] Dialog 最大宽高仍受窗口约束，但内容应在可用区域内居中显示。
- [ ] Tab 切换不改变 Dialog 外框尺寸。

Acceptance：

- [ ] 最小窗口 `1040x700` 下 Dialog 不裁切 Footer。
- [ ] 默认窗口 `1120x720` 下 Dialog 不裁切 Footer。
- [ ] 切换 Keyboard / Mouse / DInput 页时 Dialog 不跳动。
- [ ] 当前选择 pill、提示、错误消息不会覆盖内容。

## Priority 3: Keyboard Picker Layout Fix

截图问题：

- [ ] Keyboard 页按键网格过宽，右侧数字键盘区域被 Dialog 裁切。
- [ ] 键盘整体没有根据内容区宽度缩放或居中。
- [ ] 键盘只占据 Dialog 上半部，底部大面积空白，视觉重心不稳定。
- [ ] 部分 disabled key 过暗但仍占据大量视觉空间，导致可用键不够突出。

Implementation plan：

- [ ] 给 `KeyboardPicker` 定义基础设计尺寸，例如 `implicitLayoutWidth` / `implicitLayoutHeight`。
- [ ] 根据 content 宽度计算按键单位尺寸或整体 scale，优先保证不横向裁切。
- [ ] 键盘布局在 content 区水平居中。
- [ ] 如果窗口过窄，允许 content 区纵向滚动，但不允许横向裁切关键区域。
- [ ] 可用 key 的 selected / hover / enabled 状态保持清晰；disabled key 降低存在感但仍能保留布局参考。
- [ ] 保持物理键盘输入只在 Keyboard 页生效。

Acceptance：

- [ ] 最小窗口 `1040x700` 下 `Space`、方向键、数字键盘最右列完整可见。
- [ ] 默认窗口 `1120x720` 下不出现右侧按键被裁切。
- [ ] 点击虚拟键和按物理键仍能高亮 pending action。

## Priority 4: Mouse Picker Layout Fix

截图问题：

- [ ] Mouse 页左侧按钮区、中间鼠标轮廓、侧键标签存在重叠。
- [ ] 中间鼠标图形压到左侧按钮区域，侧键标签贴得过近。
- [ ] 右侧 `鼠标移动` 区和左侧内容之间间距不稳定。
- [ ] 整页内容集中在左上，右侧和底部留白过大。
- [ ] 当前 Left / Right / Middle 使用 `keyWidth: 2.2`，单个 tile 约 `106px`，三枚加 gap 约 `334px`，但所在 Column 宽度只有 `220px`，这是左侧按钮区溢出并挤压鼠标图形的直接根因。

Implementation plan：

- [ ] 将 Mouse 页改成稳定三列布局：mouse buttons / mouse visual / movement preview。
- [ ] 三列使用明确宽度和 gap，不依赖元素自然挤压。
- [ ] 修正 Left / Right / Middle tile 尺寸或左侧列宽，确保三枚按钮实际宽度不超过按钮列可用宽度。
- [ ] 鼠标轮廓居中，侧键标签与鼠标边缘保持固定间距，不覆盖按钮区。
- [ ] 左侧 Left / Right / Middle 作为本轮唯一 enabled tiles。
- [ ] Wheel / side buttons / movement tiles 保持 disabled，不产生 action selected。
- [ ] 内容整体在 Dialog content 区水平和垂直居中。

Acceptance：

- [ ] Left / Right / Middle 三个可用按钮不被鼠标图形覆盖。
- [ ] 侧键标签不与鼠标轮廓或左侧按钮重叠。
- [ ] Disabled movement tiles 可见但明显不可点。
- [ ] Confirm 只对 Left / Right / Middle 可用。

## Priority 5: DInput Picker Layout Fix

截图问题：

- [ ] DInput 页上方三个分区靠左排列，右侧空白过大。
- [ ] 下方 Button 分区跨度大，但按钮网格没有充分利用空间。
- [ ] 内容整体垂直位置偏高，Footer 附近留白过多。
- [ ] 部分 disabled tile 文字过暗，结构可读性不足。

Implementation plan：

- [ ] DInput 页保持 disabled preview，不添加任何可绑定 action。
- [ ] 上方分区使用三列等高布局：摇杆/轴、扳机、D-Pad。
- [ ] 下方 Button 分区使用等宽网格，B1-B16 分两行并居中。
- [ ] 整页在 content 区内居中，避免贴左上角。
- [ ] Disabled 状态可读但明确不可操作。

Acceptance：

- [ ] DInput 页所有 tile disabled。
- [ ] Confirm 在 DInput 页不可用。
- [ ] 三个上方分区与下方 Button 分区对齐清晰。

## Priority 6: Verification

- [ ] `git diff --check`
- [ ] `cmake --build build --config Debug`
- [ ] `.\build\Debug\MappyZQmlSmokeTests.exe`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] 手动打开应用检查三页 picker：
  - [ ] Keyboard 页无横向裁切。
  - [ ] Mouse 页无重叠。
  - [ ] DInput 页禁用态清晰。
  - [ ] Confirm / Cancel 始终可见。
  - [ ] 绑定 Keyboard / MouseButton 后 Current mappings 立即更新。
