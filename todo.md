# TODO: Runtime UX Semantics Cleanup

## Goal

把当前已经能运行的 remapping 流程表达清楚：用户应该能一眼看懂 runtime 是否在跑、映射输出是否启用、profile 是否已保存，以及设备列表里哪些内容是真设备。

本轮只做 UI / UI Bridge 语义清理，不新增后端能力，不做 profile 管理页。

## Priority 0: Verify Existing Device Panel Cleanup

`DevicesPanel` 当前已经只显示真实输入设备和空状态提示，`todo_ui.md` P7 已完成 Runtime 卡清理。本轮不再重复实现，只做回归确认。

- [x] `DevicesPanel` 只显示真实输入设备和空状态，不再放永久 `Runtime running` 占位卡片。
- [x] Runtime 状态只出现在全局区域：`TopBar` / `StatusBar` / `EventLogPanel`。
- [x] 设备列表为空时继续保留明确 empty state，不用 Runtime 卡片填空间。
- [x] 没有与 Runtime 卡片相关的 QML 属性传递和死布局代码。
- [x] 跑 QML smoke，确认当前状态无 binding/import/property warning。

## Priority 1: Add Explicit Remap Toggle

旧的 `Mapping On/Off` 主按钮已在 `todo_ui.md` P7 移除。本轮是在 `TopBar` 重新接入一个语义明确的 Remap Toggle，不是给现有按钮改名。

实际语义是：

- 输入采集、按键显示、capture 仍然工作。
- 只有 mapped action dispatch 会被启用或暂停。

实现计划：

- [x] 在 `TopBar` 的右侧 Row 中新增 Remap Toggle，位置靠近 `Save Profile`。
- [x] 按钮文案根据 `appController.mappingEnabled` 显示 `Remap Active` / `Remap Paused`。
- [x] active 状态使用 primary 样式；paused 状态使用普通样式，避免误以为输出仍然激活。
- [x] 点击后执行 `appController.mappingEnabled = !appController.mappingEnabled`，继续复用现有 `SetMappingEnabled()` 行为。
- [x] `TopBar` 内增加短暂 inline feedback：active 时 `Mapped output dispatch enabled`，paused 时 `Mapped output dispatch paused`。
- [x] Event Log 继续记录 `Mapping enabled/disabled`，后续可统一为 `Remap active/paused`，本轮先不迁移历史日志语义。
- [x] 增加或更新 QML smoke，确认按钮文案切换无 warning。

## Priority 2: Replace Symbolic Profile Suffix With Explicit Text

当前 `Profile: Default *` / `Profile: Default !` 对普通用户不够明确。保存状态应使用文字，不依赖符号。

实现计划：

- [x] `ZAppController` 增加只读 `profileDisplayText`，由 C++ 统一生成用户可读 profile 名称，不包含 `Profile:` 前缀。
- [x] `profileDisplayText` 示例：
  - [x] clean: `Default`
  - [x] dirty: `Default (unsaved)`
  - [x] error: `Default (save error)`
- [x] `TopBar` 显示 `Profile: ` + `appController.profileDisplayText`，移除 QML 内部对 `profileSaveState` 的字符串拼接。
- [x] `StatusBar` 不复用完整 profile 名称；新增或复用 C++ 提供的短文案，例如 `profileSaveDisplayText`：`Saved` / `Unsaved` / `Save Error`。
- [x] 保留底层 `profileSaveState` 给测试和后续逻辑使用，但 QML 展示层不直接解释它。
- [x] `profileDisplayText` 的 NOTIFY 选择 `profileStatusChanged`；加载 profile 成功、保存状态变化都必须触发它。
- [x] AppController 测试覆盖三种 profile display 文案和三种 profile save display 文案。

## Priority 3: Make Runtime / Output Status Copy Consistent

状态栏目前信息很多，但文案粒度不统一。目标是压缩且直观。

实现计划：

- [x] `ZAppController` 增加只读显示文本属性，QML 不直接解释内部状态：
  - [x] `runtimeDisplayText`: `Created` / `Ready` / `Running` / `Error`
  - [x] `remapDisplayText`: `Active` / `Paused`
  - [x] `profileSaveDisplayText`: `Saved` / `Unsaved` / `Save Error`
- [x] `StatusBar` 文案统一成稳定格式，例如：
  `Devices: 1 | Runtime: Running | Remap: Active | Output: Live | Profile: Saved`
- [x] Runtime state 使用 `runtimeDisplayText`，不直接展示内部小写 `runtimeState`。
- [x] Output 继续使用 `outputDisplayText`，不恢复 Real Output 开关。
- [x] Remap 使用 `remapDisplayText`，与 TopBar 文案一致。
- [x] Profile 使用 `profileSaveDisplayText`，不显示路径。
- [x] StatusBar 使用 ` | ` 分隔符替换当前多空格拼接。

## Priority 4: Tests And Acceptance

- [x] AppController 测试覆盖 profile display text。
- [x] AppController 测试覆盖 remap display text 或 mapping enabled 状态文案。
- [x] QML smoke 通过且无 warning。
- [x] `git diff --check` 无输出。
- [x] 修改文本文件保持 CRLF 行尾。

## Non-Goals

- [ ] 不做 Profile Manager：rename / duplicate / delete profile。
- [ ] 不做 Save As / Open File 对话框。
- [ ] 不做多 profile 切换。
- [ ] 不做 real output 开关或后端重建流程。
- [ ] 不改 Core mapping 行为。
- [ ] 不改 SDL 输入枚举逻辑。

## Acceptance Criteria

- [x] 设备列表里不再出现非设备的 Runtime 卡片。
- [x] 用户能从 TopBar 看懂 remapping 输出是 active 还是 paused。
- [x] 用户能从 TopBar 看懂当前 profile 是否 saved / unsaved / save error。
- [x] StatusBar 的 Runtime / Remap / Output / Profile 文案互相一致。
- [x] 当前 Apply / Delete / Autosave / Startup load 行为不回退。
