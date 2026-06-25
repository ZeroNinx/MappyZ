# TODO: Functional Runtime Pass

## Next Step

下一步目标是让软件具备实际可用闭环：连接手柄 -> 选择输入 -> 绑定输出 -> 保存/加载 -> 映射实时生效 -> 可删除或修改映射。

本轮不继续扩展 action 种类，也不做完整设置页。重点修正当前几个破坏可用性的设计问题：

- [x] 删除 `Real Output` / `NullOutput` 这条产品流程；只要 runtime 在运行，输出就应实时生效。
- [x] 绑定规则必须能删除。
- [x] 运行中 Apply / Load / Delete 后，映射必须立即生效，不需要重启软件。
- [x] UI 上的 `Runtime running`、`Mapping On/Off`、`Default` 等文案必须变成用户能理解的状态。

## Scope

包含：

- [x] 移除输出模式切换：不再暴露 `Real Output` / `NullOutput` 用户态概念，也不保留运行中切换 output backend 的产品能力。
- [x] runtime 启动成功后直接使用真实输出；输出后端不可用时直接报错，不进入”运行但不生效”的状态。
- [x] 删除 Mapping On/Off 顶部按钮，避免把内部调试开关暴露成主流程功能。
- [x] TopBar profile 标签明确显示 `Profile: <name>`，不再只显示 `Default`。
- [x] DevicesPanel 移除永久 `Runtime` 占位卡。
- [x] Device card 不再用 runtime state 作为设备状态 tag，改成 `Connected`。
- [x] Current mappings 增加删除入口。
- [x] 点击 mapping row 能选中对应 input，并把 action picker 切到该规则的 action。
- [x] Apply / Load / Delete 在 runtime running 时立即替换 active profile，后续输入马上按新规则 dispatch。
- [x] 增加测试覆盖 runtime running 时输出实时生效、配置变更立即 dispatch、映射仍可删除和修改。

不做：

- [ ] 不新增更多 keyboard/mouse action。
- [ ] 不做复杂 profile 管理页。
- [ ] 不做 per-device / per-app profile 自动切换。
- [ ] 不做组合键、宏、长按、双击。
- [ ] 不做系统权限 UI。

## Root Causes

### Output Toggle Splits Running And Live Output

当前 UI / Bootstrap 允许先以 `NullOutput` 启动，再要求用户通过 `ZAppController::setRealOutputEnabled(...)` 把输出切到真实后端。这个设计把“runtime running”和“输出实时生效”拆成了两个产品状态。

这会导致：

- [x] 用户看到 runtime 已 running，但实际上输出还没生效。
- [x] TopBar 多出一个必须解释的 `Real Output` 步骤。
- [x] 为了切换输出模式，引入 `Reinitialize()`、profile snapshot、状态恢复等额外复杂度。
- [x] 输出问题会误伤设备列表、输入状态和当前选择。

正确语义：

- [x] runtime running = output live。
- [x] 不存在 `Real Output` 开关，也不存在 `NullOutput` / `RealOutput` 两套产品模式。
- [x] 真实输出后端不可用时，`Initialize()` / `StartRuntime()` 直接失败并给出错误。
- [x] 保存、加载、编辑映射不会触发输出模式切换。

### UI Exposes Internal State As Product Concepts

当前 UI 把一些内部实现概念直接摆到主界面：

- [x] DevicesPanel 里有一个 `Runtime` 卡，看起来像设备列表占位。
- [x] 每个设备卡显示 `running`，但这是 runtime 状态，不是设备状态。
- [x] TopBar `Mapping On/Off` 没有解释，用户不知道它是暂停映射还是别的内部状态。
- [x] TopBar `Real Output` 把内部安全/调试模式暴露成主流程步骤。
- [x] TopBar `Default` 没有前缀，用户不知道它是 profile 名称。

本轮要把主流程 UI 改成用户任务语言：

- [x] 设备列表只显示设备。
- [x] Runtime 状态只放 StatusBar / EventLog。
- [x] Profile tag 显示 `Profile: Default`。
- [x] 顶部保留核心操作：Save Profile。

### Mapping Cannot Be Managed

当前可以新增/替换同一 input 的 mapping，但 Current mappings 只有展示，没有删除入口，也不能点击回填编辑器。

这会导致：

- [x] 用户无法删除已保存的错误绑定。
- [x] 用户不知道”再次选择同一 control Apply”可以替换。
- [x] 已保存 profile 一旦有错误规则，UI 无法修复。

本轮需要让 Current mappings 成为可操作列表。

## API Plan

### Remove Output Mode Toggle

`SApplicationBootstrapOptions` / `ZApplicationBootstrap`：

- [x] 删除 `SApplicationBootstrapOptions::bUseNullOutput`。
- [x] 删除 `ZAppController::initializeRuntime(bool useNullOutput)` 参数，改为无参 `initializeRuntime()`。
- [x] 删除 `ZApplicationBootstrap::IsUsingNullOutput()`。
- [x] 删除 `ZApplicationBootstrap::Reinitialize(...)`。
- [x] 删除 `SApplicationBootstrapOptions::bSkipProfileSetup`。
- [x] 放弃 `SwitchOutputBackend(...)` 方案，不再实现运行中 output mode 切换。
- [x] `Initialize()` 直接创建真实 output backend；创建失败时返回 error。
- [x] `StartRuntime()` 成功即意味着后续 dispatch 会落到真实输出 backend。
- [x] 不再以 `NullOutput` 作为默认产品路径。
- [x] `ZNullOutputBackend` 暂不从代码库彻底删除：保留为测试注入用无副作用 backend，但不再作为产品概念、默认启动路径或 UI 模式出现。

`ZAppController`：

- [x] 删除 `realOutputEnabled` / `outputModeSwitching` 属性。
- [x] 删除 `setRealOutputEnabled(bool enabled)` invokable。
- [x] `initializeRuntime()` / `startRuntime()` 成功后不需要额外”开启输出”步骤。
- [x] 输出后端不可用时通过现有 runtime error / log 暴露失败原因。
- [x] `RegisterEventHandlers()` helper 保留；即使只在 initialize 后调用一次，也继续集中管理 event pump handler 注册逻辑。
- [x] `mappingEnabled` 运行时能力保留，但不再作为 TopBar 主流程操作；后续若需要，可在设置页重新暴露。

### Mapping Rule Management

`ZAppController` 新增：

```cpp
Q_INVOKABLE bool removeBinding(QString ruleId);
```

Behavior:

- [x] Runtime 未 initialize 时返回 false，写 Error log。
- [x] `ruleId` 为空时返回 false，写 Error log。
- [x] 找到 matching `SMappingRule::Id` 后删除。
- [x] 删除后 `RuntimeHost.ReplaceProfile(updatedProfile)`。
- [x] 删除后 `RefreshMappingRuleModelFromHost()`。
- [x] 删除成功写 Success log。
- [x] 未找到 ruleId 返回 false，不修改 profile，写 Warning 或 Error log；本轮选择 Error，保持 Apply 失败语义一致。
- [x] Runtime running 时删除立即影响后续 mapping dispatch。

`ZMappingRuleModel` roles 调整：

- [x] 保留 `ruleId`。
- [x] 保留 `input`。
- [x] 保留 `output` 作为展示文本，例如 `Space` / `Left Click`。
- [x] 新增或调整 `actionKind` 为 catalog kind：`Keyboard` / `MouseButton`，不再返回当前这种展示向的 `Mouse`。
- [x] 新增 `actionValue` 为 catalog value：`Space` / `Left` / `Right` / `Middle`。
- [x] 本轮一并新增 `displayKind` role，职责固定为 UI tag 展示文案：`Keyboard` / `Mouse`。
- [x] `displayKind` 不参与 action catalog 回填；不要让展示文案污染业务参数。

`BindingEditor.qml`：

- [x] Current mappings 每行增加删除按钮。
- [x] 删除按钮调用 `appController.removeBinding(ruleId)`。
- [x] 删除成功后如果当前 selectedControl 等于该 mapping input，则清空 selectedControl 或显示 `Binding removed`。
- [x] 点击 mapping row：选中对应 input，并把 action picker 切到该行 `actionKind/actionValue`。
- [x] row delegate 直接读取当前 model row 的 `actionKind/actionValue`，由 `BindingEditor` 内部查找 catalog row 并更新 `_selectedActionIndex`。
- [x] mapping row 的 Tag 使用 `displayKind`，避免把 `MouseButton` 直接暴露成 UI 文案。
- [x] 对外只需要发 `mappingSelected(controlId)` 这一个信号给 `Main.qml` 更新 `selectedControl`；不把 action 选择状态再绕一圈传回根组件。

## QML Plan

### TopBar

- [x] 移除 `Mapping On/Off` 按钮。
- [x] 移除 `Real Output` / `Confirm Real Output` 按钮。
- [x] Profile tag 文案改为 `Profile: <activeProfileName>`。
- [x] 保留 `Save Profile`。

### DevicesPanel

- [x] 删除底部 `Runtime` 卡。
- [x] 设备卡右上角 tag 改为 `Connected`，不再显示 runtime state。
- [x] Runtime 状态只在 StatusBar 显示。
- [x] 无设备时仍显示 empty state。

### StatusBar

- [x] 保留 Runtime 状态。
- [x] `outputState` 保留为底层稳定字符串，主要供测试和调试使用。
- [x] `outputDisplayText` 保留为用户态文案，但只表达 backend 可用性或错误，不再区分 `NullOutput` / `RealOutput`。
- [x] `outputDisplayText` 建议语义收敛为：`Unavailable` / `Ready` / `Live Output` / `Output Error`。
- [x] Mapping 状态能力保留；如继续显示，应使用 `Mapping: active/paused`，但不再提供 TopBar 主按钮。
- [x] 不再把 mapping pause 作为主流程操作。

### BindingEditor

- [x] Current mappings 行可点击回填编辑器。
- [x] Current mappings 行有删除按钮。
- [x] 删除按钮不能被 row click 抢事件。
- [x] 删除失败显示 inline feedback。
- [x] 删除成功显示 inline feedback。

## Tests

### Runtime Dispatch

- [x] Running 状態下 dispatch 直接调用当前 output backend，不依赖额外 output enable 步骤。
- [x] `ReplaceProfile()` 不改变 running state。
- [x] `ReplaceProfile()` 后下一次输入立即按新规则 dispatch。

### ApplicationBootstrap

- [x] `Initialize()` 创建真实 output backend，成功后 `StartRuntime()` 可直接 dispatch。
- [x] output backend factory 失败时返回 error，不进入 Ready / Running。
- [x] 启动路径不再包含 output mode 切换或 fallback 到 `NullOutput`。
- [x] 删除围绕 `Reinitialize()` / `bSkipProfileSetup` / `IsUsingNullOutput()` 的测试，改为覆盖无参 initialize 和失败路径。

### AppController

- [x] `initializeRuntime()` / `startRuntime()` 成功后无需额外调用即可 dispatch。
- [x] 不再暴露 `setRealOutputEnabled(...)`。
- [x] 测试从 `initializeRuntime(true)` 迁移到无参 `initializeRuntime()`；NullOutput 场景通过测试构造函数注入 `MakeNullOutputFactory()` 保持。
- [x] Apply while Running 后下一次输入立即使用新 mapping。
- [x] loadProfile while Running 后下一次输入立即使用 loaded mapping。
- [x] removeBinding existing rule 成功，model rowCount 减少。
- [x] removeBinding unknown rule 返回 false，不修改 profile。
- [x] removeBinding while Running 后下一次输入不再 dispatch 被删除规则。

### QML Smoke / UI Bridge

- [x] TopBar 不再有 Mapping On/Off 按钮文本。
- [x] TopBar 不再有 `Real Output` / `Confirm Real Output` 文本。
- [x] TopBar profile tag 包含 `Profile:`。
- [x] DevicesPanel 不再出现永久 `Runtime` 卡。
- [x] Device card tag 使用 `Connected`。
- [x] BindingEditor mapping row 暴露删除按钮。
- [x] BindingEditor mapping row 点击能触发回填信号或更新 action picker。
- [x] QML smoke 无 binding/import/property warning。

## Acceptance Criteria

- [x] 打开软件后设备列表只显示真实设备，不再有 Runtime 占位卡。
- [x] runtime 启动成功后，已有 mapping 无需额外开启输出即可 dispatch。
- [x] runtime 运行中手柄输入持续更新 UI。
- [x] 运行中新增 binding 后，不重启软件即可 dispatch。
- [x] 运行中删除 binding 后，后续输入不再 dispatch。
- [x] 已保存并重新加载的 mapping 可以删除。
- [x] TopBar 的 profile 标签能看出是 profile 名称。
- [x] TopBar 不再出现含义不明的 Mapping On/Off 主按钮。
- [x] TopBar 不再出现 `Real Output` 主按钮。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续做设置页，把 Mapping Pause、启动行为放到设置里。
- [ ] 后续显示输出后端的权限/平台可用性和失败原因。
- [ ] 后续支持 rule enable/disable，而不只是删除。

## Suggested Execution Split

- [x] Step 1 先做删除/清理：移除 output toggle、`initializeRuntime(bool)`、`Reinitialize()`、`bSkipProfileSetup`、TopBar Mapping/Real Output 按钮、DevicesPanel Runtime 卡和相关测试。
- [x] Step 2 再做新增能力：`removeBinding`、MappingRuleModel 新 role、mapping row 回填 action picker、对应测试和 QML smoke。
