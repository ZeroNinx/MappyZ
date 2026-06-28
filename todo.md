# TODO: Mapping Picker Visual Regression Fix

## Goal

修复上一轮映射选择器布局调整带来的视觉回归。当前版本已经完成 QML 目录拆分，但 picker 的可读性和按键布局还没有达标。

本轮只修映射选择器 UI，不新增映射功能，不改 Core / Runtime / Backend。

## Scope

包含：

- [ ] 回退失败的低对比度改动，让可用/禁用/选中状态重新肉眼可辨。
- [ ] 修复 Keyboard 页右侧区域和底部行的对齐问题。
- [ ] 修复 Mouse 页按键重叠、尺寸比例失衡和鼠标移动布局错误。
- [ ] 保持当前能力边界：Keyboard / MouseButton 可绑定，DInput 仅占位，MouseMove 不提供新建入口。

不包含：

- [ ] 不新增 DInput 映射。
- [ ] 不恢复 MouseMove / Cursor 新建入口。
- [ ] 不新增 MouseWheel / Button4 / Button5。
- [ ] 不新增组合键、宏、turbo、toggle。
- [ ] 不继续调整主界面三栏布局。

## Priority 0: Contrast Rollback

当前问题：

- [ ] 上一轮对比度调整失败，禁用按键、禁用文字、背景之间区分过低。
- [ ] 肉眼已经很难辨认键盘上的 disabled key、鼠标页 disabled movement tile、DInput disabled preview。
- [ ] 当前状态不是“降低视觉干扰”，而是“信息不可读”。

Implementation plan：

- [ ] 回退 `PickerKey` disabled 视觉到更可读的版本。
- [ ] disabled tile 仍要明显不可点，但文字和边框必须可辨认。
- [ ] enabled tile、selected tile、disabled tile 使用三档清晰层级：
  - [ ] enabled：正常文字亮度和正常边框。
  - [ ] selected：accent border + accent fill，必须最醒目。
  - [ ] disabled：低亮度但仍可读，不能接近背景色。
- [ ] 避免通过整体页面 opacity 降低可读性；优先在 tile 内部分别控制 text / border / fill。
- [ ] 不要让 disabled 状态影响布局尺寸。

Acceptance：

- [ ] Keyboard 页 disabled function keys 仍能读出标签。
- [ ] Mouse 页 disabled wheel / movement tiles 仍能读出标签。
- [ ] DInput 页所有 disabled controls 可读，但明确不可点击。
- [ ] 当前选中的 `Keyboard: Space` 在视觉上明显强于 enabled/disabled 普通按键。

## Priority 1: Keyboard Grid Alignment

当前问题：

- [ ] Keyboard 页下面三列布局有明显错位。
- [ ] `Enter` 右侧没有和右边导航/小键盘区域形成稳定对齐。
- [ ] 右侧 `Shift` 行没有和 `Enter`、方向键、导航区对齐。
- [ ] 小键盘变成阶梯式排列：`7/8/9`、`4/5/6`、`1/2/3` 每行水平位置不一致。
- [ ] 底部一栏完全没有对齐：Space、右侧 Ctrl/Fn/Alt、方向键、数字小键盘 `0` 之间像是多个 Row 临时拼接。
- [ ] 当前基于多个 `Row + Item spacer` 的布局很难保证列对齐，缩放后问题更明显。

Implementation plan：

- [ ] 不再用自由 `Row` 加 spacer 拼整张键盘。
- [ ] 把 Keyboard 页拆成明确的区域：
  - [ ] main keys：主键区。
  - [ ] nav keys：Insert/Home/PageUp、Delete/End/PageDn。
  - [ ] arrows：方向键。
  - [ ] numpad：数字小键盘。
- [ ] 每个区域使用固定列宽和固定行高，区域之间使用固定 gap。
- [ ] 小键盘必须使用同一个 3 列 grid，三行左边界一致。
- [ ] 方向键使用倒 T 布局，`Up` 居中在 `Left/Down/Right` 上方。
- [ ] `Enter`、右 `Shift`、底部右侧 modifier 区不要再通过临时 spacer 假装对齐。
- [ ] 如果继续保留缩放，缩放应作用于整个 keyboard grid 的统一容器，不能破坏区域内部行列关系。
- [ ] 优先考虑通过可变单位尺寸生成布局，而不是对已排好的整树做 `Scale` transform。

Acceptance：

- [ ] `Enter` 行、右 `Shift` 行、底部 modifier 行与右侧区域边界稳定对齐。
- [ ] 小键盘 `7/8/9`、`4/5/6`、`1/2/3` 三行垂直列完全一致。
- [ ] 方向键保持标准倒 T，不出现横向漂移。
- [ ] 底部 `Space` 行和右侧方向键/小键盘 `0` 行视觉上属于同一基线系统。
- [ ] 选中 `Space` 后不影响任何其他按键位置。
- [ ] 最小窗口 `1040x700` 下不出现横向裁切。

## Priority 2: Mouse Picker Layout Fix

当前问题：

- [ ] Mouse 页发生按键重叠，侧键标签压到鼠标按钮区或鼠标轮廓附近。
- [ ] 左侧鼠标按钮、鼠标轮廓、右侧移动按钮尺寸差距过大，视觉比例失衡。
- [ ] 左键/右键/中键过大，侧键过小，鼠标轮廓又偏窄，导致整页不像同一套控件。
- [ ] 鼠标移动现在是 2x2 方块排列，但它表达的是方向，应改成十字键 / D-pad 形式。
- [ ] 当前三列虽然比上一版清晰，但仍缺少统一基线和稳定间距。

Implementation plan：

- [ ] 重新定义 Mouse 页三块区域：
  - [ ] left：鼠标按钮，包括 Left / Right / Middle / Wheel Up / Wheel Down。
  - [ ] center：鼠标示意图，包括侧键 preview。
  - [ ] right：鼠标移动方向 preview。
- [ ] 三块区域使用统一行高、统一标题样式、统一 tile 尺寸体系。
- [ ] Left / Right / Middle 尺寸不要明显大于移动方向 tile。
- [ ] Side Button 1 / Side Button 2 只作为 disabled preview，必须不覆盖 Left / Right / Middle。
- [ ] 鼠标轮廓保持居中，侧键标签与轮廓保持固定间距。
- [ ] 鼠标移动改为十字键布局：
  - [ ] 上移在上方居中。
  - [ ] 左移 / 右移在中间左右。
  - [ ] 下移在下方居中。
- [ ] 鼠标移动方向本轮仍 disabled，不产生 action selected。
- [ ] Confirm 只对 Left / Right / Middle 可用。

Acceptance：

- [ ] Mouse 页没有任何 tile 或文字重叠。
- [ ] Left / Right / Middle、Wheel、Side Button、Movement preview 尺寸比例协调。
- [ ] 鼠标移动方向呈十字键布局，不再是 2x2 方块。
- [ ] 点击 Left / Right / Middle 可以高亮并 Confirm。
- [ ] Wheel / Side Button / Movement 点击无效，Confirm 不因此启用。

## Priority 3: Dialog Content Positioning

当前问题：

- [ ] Keyboard / Mouse / DInput 内容仍然偏上，下面留白过大。
- [ ] Dialog 中段没有形成稳定的“选择器主体”区域。
- [ ] Footer 固定是正确的，但 content 区没有利用剩余空间做垂直居中。

Implementation plan：

- [ ] `MappingPickerDialog` 的 content 区保留在 selection summary 和 footer 之间。
- [ ] 每个 page 在 content 区中垂直居中；内容超过可用高度时才滚动。
- [ ] 不用页面整体 opacity 或页面级缩放去解决视觉层级。
- [ ] Tab / 当前选择 / Footer 不被 page 内容挤压或覆盖。

Acceptance：

- [ ] Keyboard 页在 dialog 中垂直位置自然，不贴上不贴下。
- [ ] Mouse 页主体居中，Footer 附近不出现大面积空白。
- [ ] DInput 页主体居中，预览面板和 Footer 间距合理。
- [ ] 切换三个 tab 时 Footer 不跳动。

## Priority 4: Verification

- [ ] `git diff --check`
- [ ] `cmake --build build --config Debug`
- [ ] `.\build\Debug\MappyZQmlSmokeTests.exe`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`
- [ ] 手动检查：
  - [ ] Keyboard 页标签可读。
  - [ ] Keyboard 页网格对齐。
  - [ ] Mouse 页无重叠。
  - [ ] Mouse movement 是十字键布局。
  - [ ] DInput 页 disabled preview 可读。
  - [ ] Keyboard / MouseButton 绑定仍能成功写入 Current mappings。
