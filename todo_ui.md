# TODO: Fixed Gamepad Mapping Panels

## Goal

把当前 `EditableGamepadMappingView` 从“围绕手柄散放多个小卡片”的雏形，收敛成更稳定的四固定区域布局：

- 左上/左侧区域：左摇杆。
- 左下区域：十字键。
- 右下区域：右摇杆。
- 右上/右侧区域：功能键区域。
- 手柄图示整体上移，作为中心视觉锚点。

本轮不做多手柄类型适配，只把布局拆分边界和固定点位建立好。未来可在同一接口下根据手柄类型替换不同 layout preset。

## Non-Goals

- [ ] 不做 Xbox / DualShock / Switch Pro 等不同手柄布局自动适配。
- [ ] 不做布局编辑器。
- [ ] 不做用户自定义卡片位置。
- [ ] 不重写映射引擎、Profile schema 或输出后端。
- [ ] 不新增 DInput 功能。
- [ ] 不做完整视觉体系重做，只修当前中间映射面板的信息架构和空间分配。

## Priority 0: Component Split

目标：先把中间映射面板拆成清晰的内部组件，避免继续在一个 QML 文件里堆坐标和控件。

- [ ] 保留 `EditableGamepadMappingView.qml` 作为容器组件，只负责：
  - [ ] 接收 `appController`、`selectedDevice`、`selectedControl`。
  - [ ] 维护 selected / double-click 信号转发。
  - [ ] 选择当前布局 preset。
  - [ ] 摆放四个固定区域和中心手柄图示。
- [ ] 新增或整理 `GamepadMappingPanel.qml`，作为四个区域的通用面板容器：
  - [ ] `title`
  - [ ] `controls`
  - [ ] `mappingRuleModel`
  - [ ] `selectedControl`
  - [ ] `mappingRevision`
  - [ ] `controlClicked(controlId)`
  - [ ] `controlDoubleClicked(controlId)`
- [ ] 如果现有 `MappingGroupCard.qml` 能承担该职责，则重命名或扩展它；避免同时存在两个职责重复的卡片组件。
- [ ] 新增或拆出 `GamepadGlyph.qml`，承载手柄图示：
  - [ ] 手柄轮廓。
  - [ ] LT / RT / LB / RB。
  - [ ] LS / RS。
  - [ ] D-Pad。
  - [ ] ABXY。
  - [ ] Back / Guide / Start。
- [ ] 中心手柄图示内的控件仍要能根据 `InputControlState` 实时响应。
- [ ] 不把四个区域的 row 绘制逻辑写进 `GamepadGlyph.qml`。

## Priority 1: Fixed Four-Zone Layout

目标：建立四个固定区域，后续不同手柄布局只替换区域内容和手柄图示，不影响右侧 Inspector。

- [ ] 在 `EditableGamepadMappingView` 内定义四个固定 slot：
  - [ ] `leftTopSlot`：左摇杆区域。
  - [ ] `leftBottomSlot`：十字键区域。
  - [ ] `rightTopSlot`：功能键区域。
  - [ ] `rightBottomSlot`：右摇杆区域。
- [ ] 四个 slot 使用相对父容器的比例定位，不使用散落的 magic pixel 坐标。
- [ ] 本轮固定布局建议：
  - [ ] 左摇杆区域放在左侧中上，包含 `Click / Up / Left / Down / Right`。
  - [ ] 十字键区域放在左侧中下，包含 `Up / Down / Left / Right`。
  - [ ] 右摇杆区域放在右侧中下，包含 `Click / Up / Down / Left / Right`。
  - [ ] 功能键区域放在右侧中上，包含 `A / B / X / Y / LB / LT / RB / RT / Back / Guide / Start`。
- [ ] 功能键区域内部可以继续分组显示，但外部只占一个固定区域。
- [ ] 四个区域的宽高应按当前中间面板实际尺寸计算：
  - [ ] 默认窗口下不重叠。
  - [ ] 放大窗口后区域应同步变大，而不是保持小卡片集中在中心。
  - [ ] 最小窗口下允许内部 elide，但不允许区域互相覆盖。
- [ ] 区域位置要给中心手柄留足空间，不能压到手柄图示。

## Priority 2: Move Gamepad Glyph Up

目标：让手柄图示作为中间视觉锚点，而不是被底部卡片挤压或缩在中心。

- [ ] 手柄图示整体向上移动，给下方左右两个区域留空间。
- [ ] 手柄图示宽度使用中间面板宽度的合理比例，例如 `45% - 55%`，不要被高度缩放压得过小。
- [ ] 手柄图示高度独立控制，不继续直接继承旧 SVG 主区域比例。
- [ ] 手柄图示与四个区域之间保持可读间距：
  - [ ] 左摇杆区域不贴住手柄左侧。
  - [ ] 功能键区域不贴住 ABXY。
  - [ ] 底部区域不遮挡 D-Pad / RS。
- [ ] 连接线本轮可弱化或暂时移除，优先保证面板可读。
- [ ] 如果保留连接线：
  - [ ] 连接线绘制在卡片和文字后面。
  - [ ] 只连接到区域边缘和手柄控件附近。
  - [ ] 不穿过文字。

## Priority 3: Restore And Clarify Gamepad Glyph Controls

目标：恢复丢失或弱化的手柄图示信息，保证用户能从图上识别控件位置。

- [ ] D-Pad 比例修正：
  - [ ] 四方向紧凑成十字结构。
  - [ ] 中心块和四方向尺寸协调。
  - [ ] 不再像散开的四个小方块。
- [ ] ABXY 比例修正：
  - [ ] 四个按钮大小与手柄比例匹配。
  - [ ] 菱形布局清晰。
  - [ ] A/B/X/Y 文本可读。
- [ ] 左右摇杆恢复动态响应：
  - [ ] 摇杆帽根据 Axis2D 输入偏移。
  - [ ] 方向虚拟按钮按下时有方向 indicator。
  - [ ] 不再只是固定 `LS` / `RS` 文本。
- [ ] LS / RS click 明确呈现：
  - [ ] 手柄图上有可识别的 LS / RS click 控件。
  - [ ] 左摇杆区域和右摇杆区域都列出 `Click` 行。
  - [ ] 点击图上的 LS / RS 与点击区域中的 `Click` 行选中同一个 controlId。
- [ ] LB / RB / LT / RT 保持可识别：
  - [ ] LT/RT 显示 trigger 数值。
  - [ ] LB/RB 显示 pressed 状态。
- [ ] Back / Guide / Start 不应只出现在右侧功能区，手柄图上也要保留可识别点位。

## Priority 4: Selection And Inspector Sync

目标：修复点击中间 row 后右侧 Inspector 不同步 action output / Clear 状态的问题。

- [ ] 点击任意区域 row 时：
  - [ ] 更新 `selectedControl`。
  - [ ] Inspector selected control 立即同步。
  - [ ] Inspector action output 读取该 control 当前绑定并同步。
  - [ ] 如果该 control 未绑定，action output 显示 `No action selected` 或同等明确文案。
- [ ] 双击任意区域 row 时：
  - [ ] 选中该 control。
  - [ ] 打开 `MappingPickerDialog`。
  - [ ] picker 初始值使用该 control 当前绑定；未绑定时使用默认 `Keyboard / Space`。
- [ ] `Clear` 按钮规则：
  - [ ] 当前 control 有绑定时启用。
  - [ ] 当前 control 无绑定时禁用。
  - [ ] Clear 成功后区域 row 立即显示 `Unassigned`。
  - [ ] Clear 成功后 Inspector action output 立即变为未选择状态。
- [ ] 选中态要双向一致：
  - [ ] 区域 row 高亮。
  - [ ] 手柄图上对应控件高亮。
  - [ ] Inspector 显示同一个 control。

## Priority 5: Layout Data Boundary For Future Presets

目标：为未来不同手柄布局适配留出干净边界，但本轮只实现一个固定 Xbox-like preset。

- [ ] 新增一个内部 layout data 对象，例如 `defaultGamepadLayout`。
- [ ] layout data 至少包含：
  - [ ] 四个 slot 的相对位置和尺寸。
  - [ ] 每个 slot 的 title。
  - [ ] 每个 slot 的 controls 列表。
  - [ ] 手柄图示的相对位置和尺寸。
- [ ] 当前 preset 固定为 `defaultGamepadLayout`，不暴露 UI 切换。
- [ ] 不在多个 QML 组件里重复写同一组 controlId。
- [ ] controlId 常量本轮可继续用字符串，但应集中在 layout data 内。

## Priority 6: Visual And Spacing Acceptance

- [ ] 默认窗口大小下：
  - [ ] 手柄图示比当前版本更大，且整体上移。
  - [ ] 四个区域不重叠。
  - [ ] 四个区域不挤压 Inspector。
  - [ ] 字体可读，不像缩略图。
- [ ] 放大窗口后：
  - [ ] 手柄图示和四个区域跟随扩大或重新分配空间。
  - [ ] 不出现“中间一小团、周围大片空白”。
- [ ] 最小窗口下：
  - [ ] 四个区域仍可操作。
  - [ ] 文本可 elide，但不能互相覆盖。
  - [ ] 如确实放不下，允许中间面板内部滚动。
- [ ] D-Pad 和 ABXY 的比例接近手柄常识布局。
- [ ] LS / RS click 与摇杆方向语义清晰分离。
- [ ] 低对比度问题不再出现，`Unassigned` 仍可读。

## Priority 7: Verification

- [ ] `git diff --check`
- [ ] CRLF line ending check
- [ ] `cmake --build build --config Debug`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] QML smoke 无 warning。
- [ ] 手动验证：
  - [ ] 点击左摇杆 `Click / Up / Left / Down / Right` 都能同步 Inspector。
  - [ ] 点击十字键四方向都能同步 Inspector。
  - [ ] 点击右摇杆 `Click / Up / Down / Left / Right` 都能同步 Inspector。
  - [ ] 点击功能键区域中的 ABXY / LB / LT / RB / RT / Back / Guide / Start 都能同步 Inspector。
  - [ ] 绑定后对应区域 row 立即更新输出。
  - [ ] Clear 后对应区域 row 立即变为 `Unassigned`。
  - [ ] 手柄实际输入时，图示和 row 都有可见反馈。
