# TODO: Functional Runtime Pass

## Next Step

下一步目标是让软件具备实际可用闭环：连接手柄 -> 选择输入 -> 绑定输出 -> 保存/加载 -> 开启真实输出 -> 映射实时生效 -> 可删除或修改映射。

本轮不继续扩展 action 种类，也不做完整设置页。重点修正当前几个破坏可用性的设计问题：

- [ ] 真实输出开关不能重建输入后端，不能清空设备列表。
- [ ] 已连接设备必须在切换输出模式后继续可见、继续产生输入事件。
- [ ] 绑定规则必须能删除。
- [ ] 运行中 Apply / Load / Delete 后，映射必须立即生效，不需要重启软件。
- [ ] UI 上的 `Runtime running`、`Mapping On/Off`、`Default` 等文案必须变成用户能理解的状态。

## Scope

包含：

- [ ] 输出后端热切换：只替换 output backend，不重建 input backend / event queue / runtime host。
- [ ] 删除 Mapping On/Off 顶部按钮，避免把内部调试开关暴露成主流程功能。
- [ ] TopBar profile 标签明确显示 `Profile: <name>`，不再只显示 `Default`。
- [ ] DevicesPanel 移除永久 `Runtime` 占位卡。
- [ ] Device card 不再用 runtime state 作为设备状态 tag，改成 `Connected`。
- [ ] Current mappings 增加删除入口。
- [ ] 点击 mapping row 能选中对应 input，并把 action picker 切到该规则的 action。
- [ ] Apply / Load / Delete 在 runtime running 时立即替换 active profile，后续输入马上按新规则 dispatch。
- [ ] 增加测试覆盖真实输出切换后设备不丢、输入不断、映射仍可 dispatch。

不做：

- [ ] 不新增更多 keyboard/mouse action。
- [ ] 不做复杂 profile 管理页。
- [ ] 不做 per-device / per-app profile 自动切换。
- [ ] 不做组合键、宏、长按、双击。
- [ ] 不做系统权限 UI。

## Root Causes

### Real Output Switch Rebuilds Input

当前 `ZAppController::setRealOutputEnabled(...)` 通过 `ZApplicationBootstrap::Reinitialize(...)` 切换输出模式。`Reinitialize()` 会销毁 `RuntimeHost`、`InputBackend`、`OutputBackend`，再重新创建整套 runtime。

这会导致：

- [ ] SDL input backend 被重建。
- [ ] event queue / callback 被 detach 后重新 attach。
- [ ] DeviceModel 从新 backend 重新枚举，真实设备可能暂时或永久消失。
- [ ] InputStateModel 被清空。
- [ ] 用户看到“点 Real Output 后手柄没了”，实际是输出开关误伤输入链路。

正确语义：

- [ ] 输出模式只影响 action dispatch 目标。
- [ ] 输出模式不应影响设备枚举、输入事件、当前选中设备、当前按键状态。
- [ ] 输出切换失败时，保留原 output backend，不改变 input/runtime 状态。

### UI Exposes Internal State As Product Concepts

当前 UI 把一些内部实现概念直接摆到主界面：

- [ ] DevicesPanel 里有一个 `Runtime` 卡，看起来像设备列表占位。
- [ ] 每个设备卡显示 `running`，但这是 runtime 状态，不是设备状态。
- [ ] TopBar `Mapping On/Off` 没有解释，用户不知道它是暂停映射还是输出模式。
- [ ] TopBar `Default` 没有前缀，用户不知道它是 profile 名称。

本轮要把主流程 UI 改成用户任务语言：

- [ ] 设备列表只显示设备。
- [ ] Runtime 状态只放 StatusBar / EventLog。
- [ ] Profile tag 显示 `Profile: Default`。
- [ ] 顶部保留核心操作：Real Output、Save Profile。

### Mapping Cannot Be Managed

当前可以新增/替换同一 input 的 mapping，但 Current mappings 只有展示，没有删除入口，也不能点击回填编辑器。

这会导致：

- [ ] 用户无法删除已保存的错误绑定。
- [ ] 用户不知道“再次选择同一 control Apply”可以替换。
- [ ] 已保存 profile 一旦有错误规则，UI 无法修复。

本轮需要让 Current mappings 成为可操作列表。

## API Plan

### Output Backend Hot Swap

`ZActionDispatcher`：

```cpp
explicit ZActionDispatcher(IOutputBackend& OutputBackend);
void SetOutputBackend(IOutputBackend& OutputBackend) noexcept;
SOutputBackendStatus GetOutputStatus() const;
```

Implementation note:

- [ ] `ZActionDispatcher` 内部从 `IOutputBackend& Backend` 改为 `IOutputBackend* Backend`。
- [ ] `DispatchAction()` / `GetOutputStatus()` 使用当前 backend 指针。
- [ ] `SetOutputBackend()` 只替换指针，不清空 recent records，不改变 enabled 状态。

`ZRuntimeHost`：

```cpp
void ReplaceOutputBackend(IOutputBackend& NewOutputBackend) noexcept;
```

Behavior:

- [ ] 更新内部 output backend 指针。
- [ ] 调用 `Dispatcher.SetOutputBackend(NewOutputBackend)`。
- [ ] 不 stop input backend。
- [ ] 不 detach event queue。
- [ ] 不修改 `HostState`。
- [ ] 不修改 active profile。
- [ ] 不清空 mapping/input records。

`ZApplicationBootstrap`：

```cpp
TResult<void> SwitchOutputBackend(bool bUseNullOutput);
```

Behavior:

- [ ] 要求 `Initialize()` 已成功，即状态为 Ready 或 Running。
- [ ] 创建新的 output backend。
- [ ] 创建失败时返回 error，保留旧 output backend 和 `CachedOptions.bUseNullOutput` 不变。
- [ ] 创建成功后调用 `RuntimeHost.ReplaceOutputBackend(*NewBackend)`。
- [ ] 成功后再用 `OutputBackend = std::move(NewBackend)` 接管所有权。
- [ ] 成功后更新 `CachedOptions.bUseNullOutput`。
- [ ] 不创建新的 input backend。
- [ ] 不创建新的 runtime host。
- [ ] 不调用 `StopRuntime()`。
- [ ] 不调用 `StartRuntime()`。

`ZAppController::setRealOutputEnabled(bool enabled)`：

- [ ] 改为调用 `Bootstrap.SwitchOutputBackend(!enabled)`。
- [ ] 不再调用 `Bootstrap.Reinitialize(...)`。
- [ ] 不再保存/恢复 profile snapshot。
- [ ] 不再 stop/start pump timer。
- [ ] 不再 clear `InputStateModel`。
- [ ] 不再 refresh `DeviceModel`，因为设备没有变化。
- [ ] 切换成功后 emit `outputModeChanged()` 和 `runtimeStatusChanged()`。
- [ ] 切换失败后保持原 output mode，写 Error log，emit `runtimeError()`。

### Mapping Rule Management

`ZAppController` 新增：

```cpp
Q_INVOKABLE bool removeBinding(QString ruleId);
```

Behavior:

- [ ] Runtime 未 initialize 时返回 false，写 Error log。
- [ ] `ruleId` 为空时返回 false，写 Error log。
- [ ] 找到 matching `SMappingRule::Id` 后删除。
- [ ] 删除后 `RuntimeHost.ReplaceProfile(updatedProfile)`。
- [ ] 删除后 `RefreshMappingRuleModelFromHost()`。
- [ ] 删除成功写 Success log。
- [ ] 未找到 ruleId 返回 false，不修改 profile，写 Warning 或 Error log；本轮选择 Error，保持 Apply 失败语义一致。
- [ ] Runtime running 时删除立即影响后续 mapping dispatch。

`ZMappingRuleModel` roles 调整：

- [ ] 保留 `ruleId`。
- [ ] 保留 `input`。
- [ ] 保留 `output` 作为展示文本，例如 `Space` / `Left Click`。
- [ ] 新增或调整 `actionKind` 为 catalog kind：`Keyboard` / `MouseButton`。
- [ ] 新增 `actionValue` 为 catalog value：`Space` / `Left` / `Right` / `Middle`。
- [ ] 如需展示 `Mouse` tag，新增 `displayKind`，不要让展示文案污染业务参数。

`BindingEditor.qml`：

- [ ] Current mappings 每行增加删除按钮。
- [ ] 删除按钮调用 `appController.removeBinding(ruleId)`。
- [ ] 删除成功后如果当前 selectedControl 等于该 mapping input，则清空 selectedControl 或显示 `Binding removed`。
- [ ] 点击 mapping row：选中对应 input，并把 action picker 切到该行 `actionKind/actionValue`。
- [ ] 为此新增信号，例如 `mappingSelected(controlId)`，由 `Main.qml` 更新 `selectedControl`。
- [ ] `BindingEditor` 内部根据 `actionKind/actionValue` 查找 catalog row 并更新 `_selectedActionIndex`。

## QML Plan

### TopBar

- [ ] 移除 `Mapping On/Off` 按钮。
- [ ] Profile tag 文案改为 `Profile: <activeProfileName>`。
- [ ] 保留 `Real Output` 两步确认按钮。
- [ ] 保留 `Save Profile`。
- [ ] `Real Output` 成功/失败继续通过 EventLog/runtimeError 提示。

### DevicesPanel

- [ ] 删除底部 `Runtime` 卡。
- [ ] 设备卡右上角 tag 改为 `Connected`，不再显示 runtime state。
- [ ] Runtime 状态只在 StatusBar 显示。
- [ ] 无设备时仍显示 empty state。

### StatusBar

- [ ] 保留 Runtime 状态。
- [ ] 保留 Output 显示。
- [ ] Mapping 状态如果继续显示，应使用 `Mapping: active/paused`；但主按钮移除后默认应保持 active。
- [ ] 不再把 mapping pause 作为主流程操作。

### BindingEditor

- [ ] Current mappings 行可点击回填编辑器。
- [ ] Current mappings 行有删除按钮。
- [ ] 删除按钮不能被 row click 抢事件。
- [ ] 删除失败显示 inline feedback。
- [ ] 删除成功显示 inline feedback。

## Tests

### ActionDispatcher / RuntimeHost

- [ ] `ActionDispatcher.SetOutputBackend()` 后后续 dispatch 使用新 backend。
- [ ] `SetOutputBackend()` 不改变 enabled 状态。
- [ ] `RuntimeHost.ReplaceOutputBackend()` 不改变 running state。
- [ ] `RuntimeHost.ReplaceOutputBackend()` 不 detach event queue。
- [ ] `RuntimeHost.ReplaceOutputBackend()` 不修改 active profile。

### ApplicationBootstrap

- [ ] `SwitchOutputBackend(true)` 切到 NullOutput。
- [ ] `SwitchOutputBackend(false)` 调用真实 output factory。
- [ ] output factory 失败时保留旧 output mode。
- [ ] output factory 失败时保留旧 OutputBackend 对象。
- [ ] output switch 不调用 input factory 第二次。
- [ ] output switch 不替换 RuntimeHost。
- [ ] Running 状态下 output switch 后仍 Running。
- [ ] Ready 状态下 output switch 后仍 Ready。

### AppController

- [ ] `setRealOutputEnabled(true)` 不清空 `DeviceModel`。
- [ ] `setRealOutputEnabled(true)` 不清空 `InputStateModel`。
- [ ] `setRealOutputEnabled(true)` 不停止 pump timer。
- [ ] `setRealOutputEnabled(true)` 不改变 selected profile/mapping rules。
- [ ] 真实输出失败时仍保留 NullOutput，设备列表不变。
- [ ] 切换输出后继续 `EmitInput + pumpOnce`，mapping dispatch 仍发生。
- [ ] Apply while Running 后下一次输入立即使用新 mapping。
- [ ] loadProfile while Running 后下一次输入立即使用 loaded mapping。
- [ ] removeBinding existing rule 成功，model rowCount 减少。
- [ ] removeBinding unknown rule 返回 false，不修改 profile。
- [ ] removeBinding while Running 后下一次输入不再 dispatch 被删除规则。

### QML Smoke / UI Bridge

- [ ] TopBar 不再有 Mapping On/Off 按钮文本。
- [ ] TopBar profile tag 包含 `Profile:`。
- [ ] DevicesPanel 不再出现永久 `Runtime` 卡。
- [ ] Device card tag 使用 `Connected`。
- [ ] BindingEditor mapping row 暴露删除按钮。
- [ ] BindingEditor mapping row 点击能触发回填信号或更新 action picker。
- [ ] QML smoke 无 binding/import/property warning。

## Acceptance Criteria

- [ ] 打开软件后设备列表只显示真实设备，不再有 Runtime 占位卡。
- [ ] 点击 Real Output 后设备列表不清空。
- [ ] 点击 Real Output 后手柄输入仍然更新 UI。
- [ ] 点击 Real Output 后已有 mapping 仍然存在。
- [ ] 运行中新增 binding 后，不重启软件即可 dispatch。
- [ ] 运行中删除 binding 后，后续输入不再 dispatch。
- [ ] 已保存并重新加载的 mapping 可以删除。
- [ ] TopBar 的 profile 标签能看出是 profile 名称。
- [ ] TopBar 不再出现含义不明的 Mapping On/Off 主按钮。
- [ ] 所有测试通过。
- [ ] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续做设置页，把 Mapping Pause、真实输出偏好、启动行为放到设置里。
- [ ] 后续保存用户上次选择的 output mode。
- [ ] 后续显示真实输出后端的权限/平台可用性。
- [ ] 后续支持 rule enable/disable，而不只是删除。
