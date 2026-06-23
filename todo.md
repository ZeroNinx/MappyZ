# TODO: UI Bridge Semantic Input Notifications

## Next Step

下一步暂缓新功能，先修正 UI Bridge 输入状态刷新方案：移除 `revision` / `_inputRev` 这类隐藏 invalidation token，改成真实语义事件驱动的 UI 更新。

这轮目标不是重写 Core/Runtime，而是把 UI Bridge 从“事件进入 C++，QML 靠伪 int 触发重新查询”改成更清晰的模式：

- [x] C++ 输入状态模型保存当前快照。
- [x] C++ 在状态变化时发出有业务含义的 signal。
- [x] QML 控件响应 signal，更新自己的显式状态属性。
- [x] QML 只在设备切换、初始化、重置等边界场景主动读取快照。
- [x] 不再用无业务含义的递增数字让 QML 绑定被动重算。

## Problem Statement

- [x] 当前 `revision` 能让 UI 刷新，但它不是领域数据。
- [x] 删除看似“不重要”的 `revision` 会导致 UI 不更新，说明依赖关系被隐藏了。
- [x] `Q_INVOKABLE isPressed()/value()/axisX()` 适合做快照查询，不适合作为 QML 绑定刷新机制。
- [x] `dataChanged` 只天然服务于 model/delegate 绑定，不会自动驱动固定手柄布局里的函数查询。
- [x] 固定手柄 UI 需要“某设备某控件变了”的真实事件，而不是“某个 int 变了”的间接信号。

## Scope

包含：

- [x] 给 `ZDeviceModel` 增加设备生命周期语义 signals。
- [x] 修改 `ZInputStateModel`，移除 `revision` 机制。
- [x] 给 `ZInputStateModel` 增加语义化 Qt signals。
- [x] 调整 `ApplyInputEvent()` / `RemoveDevice()` / `clear()` 的 signal 语义。
- [x] 调整 `ui/Main.qml`，用事件驱动的本地控件状态替代 `_inputRev` 查询辅助。
- [x] 修正未选中设备时事件已到但 Gamepad View 不更新的交互问题。
- [x] 更新 `InputStateModelTests` 和 `AppControllerTests`，覆盖 signal 语义和 UI Bridge 数据链路。
- [x] 保持 Core、Runtime、Backend API 不变。

不做：

- [x] 不重构 `ZRuntimeEventPump`、`ZRuntimeHost`、backend queue 或 mapping engine。
- [x] 不实现 Binding Editor 的完整 capture / save workflow。
- [x] 不实现 `ZLogModel`。
- [x] 不引入新的 profile 管理能力。
- [x] 不把所有 UI 状态强行改成纯事件流；快照查询仍保留用于初始化和设备切换。

## Target Design

采用 event-notified snapshot：

- [x] Device Snapshot：`ZDeviceModel` 继续保存当前设备列表。
- [x] Input Snapshot：`ZInputStateModel` 继续保存每个 `(deviceId, controlId)` 的最后状态。
- [x] Device Notification：设备列表变化时发出语义 signal，signal payload 说明哪个设备变化。
- [x] Input Notification：输入状态变化时发出语义 signal，signal payload 说明哪个控件变化。
- [x] UI State：QML 中每个可视控件维护自己的 `pressed/value/axisX/axisY/displayValue` 属性。
- [x] Refresh Rule：QML 收到匹配当前设备和控件的 signal 后，只刷新对应控件。
- [x] Boundary Reads：设备切换、应用启动、设备移除、reset 后允许调用快照查询函数重建 UI 状态。

核心原则：

- [x] signal 表示发生了什么，不表示“请重新算一遍”。
- [x] query API 只读当前事实，不承担订阅语义。
- [x] QML 绑定依赖真实属性，例如 `buttonSouthState.pressed`，不依赖隐藏 token。
- [x] 如果一个属性被删除会破坏刷新，测试必须能明确失败。

## ZDeviceModel API Changes

新增 signals：

- [x] `void DeviceAdded(QString deviceId);`
- [x] `void DeviceUpdated(QString deviceId);`
- [x] `void DeviceRemoved(QString deviceId);`
- [x] `void DeviceModelReset();`

行为：

- [x] `AddOrUpdateDevice()` 插入新设备后发 `DeviceAdded(deviceId)`。
- [x] `AddOrUpdateDevice()` 更新已有设备后发 `DeviceUpdated(deviceId)`。
- [x] `RemoveDevice()` 找到并删除设备后发 `DeviceRemoved(deviceId)`。
- [x] `RemoveDevice()` 找不到设备时不发 `DeviceRemoved`。
- [x] `clear()` 只在确实清空非空 model 后发 `DeviceModelReset()`。
- [x] `ReplaceDevices()` 继续执行 model reset，并在 reset 后发 `DeviceModelReset()`。
- [x] `rowsInserted` / `dataChanged` / `rowsRemoved` / `modelReset` 继续保留，服务于 Qt view/model 机制。
- [x] 语义 signals 必须在内部设备快照更新完成后发出；slot 内调用 `deviceIdAt()` / `displayNameAt()` / `ListDevicesSnapshot()` 应读到新状态。

## ZInputStateModel API Changes

移除：

- [x] 移除 `Q_PROPERTY(int revision READ Revision NOTIFY RevisionChanged)`。
- [x] 移除 `int Revision() const`。
- [x] 移除 `void RevisionChanged()` signal。
- [x] 移除 `int CurrentRevision`。
- [x] 移除所有 `++CurrentRevision`。

新增 signals：

- [x] `void ControlStateChanged(QString deviceId, QString controlId);`
- [x] `void DeviceStateRemoved(QString deviceId);`
- [x] `void InputStateReset();`

保留 snapshot 查询 API：

- [x] `Q_INVOKABLE bool isPressed(QString deviceId, QString controlId) const`
- [x] `Q_INVOKABLE double value(QString deviceId, QString controlId) const`
- [x] `Q_INVOKABLE double axisX(QString deviceId, QString controlId) const`
- [x] `Q_INVOKABLE double axisY(QString deviceId, QString controlId) const`
- [x] `Q_INVOKABLE QString displayValue(QString deviceId, QString controlId) const`
- [x] `Q_INVOKABLE QString latestControlId(QString deviceId = QString()) const`

行为：

- [x] `ApplyInputEvent()` 插入新状态后发 `ControlStateChanged(deviceId, controlId)`。
- [x] `ApplyInputEvent()` 更新已有状态后发 `ControlStateChanged(deviceId, controlId)`。
- [x] `RemoveDevice()` 只在确实删除了至少一条状态时发 `DeviceStateRemoved(deviceId)`。
- [x] `clear()` 只在确实清空了非空状态时发 `InputStateReset()`。
- [x] 继续保留 `rowsInserted` / `dataChanged` / `rowsRemoved` / `modelReset`，服务于未来 model/delegate 使用场景。

## QML Binding Plan

移除：

- [x] 移除 `property int _inputRev`。
- [x] 移除所有 `void(_inputRev)`。
- [x] 移除 `inputPressed()` / `inputValue()` / `inputAxisX()` / `inputAxisY()` / `inputDisplayValue()` 这类用于强制重算的 helper。
- [x] 移除 `deviceRepeater.onCountChanged` 中的设备选择逻辑；设备选择改由 `DeviceModel` 语义 signal handlers 完全接管。

新增一个 QML 侧小型状态绑定组件，名称暂定 `InputControlState`：

- [x] 暴露 `deviceId`、`controlId`。
- [x] 暴露 `pressed`、`value`、`axisX`、`axisY`、`displayValue`。
- [x] `refresh()` 从 `appController.inputStateModel` 读取当前快照。
- [x] `reset()` 把状态恢复默认值。
- [x] `deviceId` 或 `controlId` 变化时调用 `refresh()`。
- [x] 监听 `inputStateModel.ControlStateChanged`，只有 device/control 匹配时调用 `refresh()`。
- [x] 监听 `inputStateModel.DeviceStateRemoved`，如果 device 匹配则调用 `reset()`；这个 signal 只表示该设备已有输入状态被清理，不表示设备一定断开。
- [x] 监听 `inputStateModel.InputStateReset`，调用 `reset()`。

实现细节：

- [x] 优先在 `Main.qml` 内用局部 `component InputControlState` 实现。
- [x] 如果 `QtObject` 不能承载 `Connections`，使用 `Item { visible: false }` 作为无视觉状态对象。
- [x] `ControlDot.active` 改为绑定到对应 `InputControlState.pressed`。
- [x] LT/RT 文案和强度绑定到对应 `InputControlState.displayValue/value`。
- [x] LS/RS 点位绑定到对应 `InputControlState.axisX/axisY`。
- [x] `Selected input` 维护显式 `latestControlForSelectedDevice` 属性，收到当前设备的 `ControlStateChanged` 时直接使用 signal payload 的 `controlId` 更新。
- [x] `ControlStateChanged` handler 不为 latest control 反查 `latestControlId()`；只有设备切换、初始化、reset 后恢复边界状态时才调用 `latestControlId(selectedDevice)`。

## Device Selection Fix

当前如果没有选中设备，事件计数会变，但 Gamepad View 必然显示默认状态。这个行为容易被误判为输入状态没刷新。

已具备前置条件：

- [x] `ZDeviceModel` 已提供 `Q_INVOKABLE QString displayNameAt(int Row) const`。
- [x] 当前 QML 自动选择首个设备时已经使用 `displayNameAt(0)`，不需要再新增该 API。

- [x] 设备选择逻辑由 `ZDeviceModel` 语义 signals 驱动，而不是从 `onCountChanged` 推断业务事件。
- [x] `DeviceAdded(deviceId)` 到达时，如果当前无选中设备，自动选择该设备。
- [x] `DeviceAdded(deviceId)` 到达时，如果当前已有选中设备，不切换选择。
- [x] `DeviceUpdated(deviceId)` 到达时，如果更新的是当前选中设备，同步刷新 `selectedDeviceDisplayName`。
- [x] `DeviceRemoved(deviceId)` 到达时，如果移除的不是当前选中设备，不切换选择。
- [x] `DeviceRemoved(deviceId)` 到达时，如果移除的是当前选中设备，自动选择剩余第 0 行；如果没有剩余设备，则清空选择。
- [x] `DeviceModelReset()` 到达时，重新选择第 0 行；如果没有设备，则清空选择。
- [x] 原因：设备连接后如果从未产生输入，`InputStateModel` 没有可删除状态，`DeviceStateRemoved` 不会触发；但 `DeviceModel.DeviceRemoved` 一定能表达设备生命周期变化。
- [x] `InputStateModel.DeviceStateRemoved` 只服务于 `InputControlState.reset()`，不承担设备选择职责。
- [x] 自动选择后立即刷新所有 `InputControlState`。
- [x] 保留手动点击设备切换选择的行为。
- [x] 自动选择时使用 `deviceIdAt(0)` 和 `displayNameAt(0)` 同步更新 `selectedDevice` / `selectedDeviceDisplayName`。

## Tests

`ZDeviceModel`：

- [x] `AddOrUpdateDevice()` 插入新设备时发一次 `DeviceAdded(deviceId)`。
- [x] `AddOrUpdateDevice()` 更新已有设备时发一次 `DeviceUpdated(deviceId)`。
- [x] `RemoveDevice()` 删除已有设备时发一次 `DeviceRemoved(deviceId)`。
- [x] `RemoveDevice()` 找不到设备时不发 `DeviceRemoved`。
- [x] `clear()` 清空非空 model 时发一次 `DeviceModelReset()`。
- [x] `clear()` 空 model 时不发 `DeviceModelReset()`。
- [x] `ReplaceDevices()` 发一次 `DeviceModelReset()`。
- [x] `DeviceRemoved` 发射时设备快照已经更新；slot 内遍历 `deviceIdAt()` 不应再找到被移除设备。
- [x] `DeviceAdded` / `DeviceUpdated` 发射时设备快照已经更新；slot 内调用 `displayNameAt()` 应读到新值。

`ZInputStateModel`：

- [x] meta-object 中不存在 `revision` property。
- [x] `ApplyInputEvent()` 插入新控件时发一次 `ControlStateChanged(deviceId, controlId)`。
- [x] `ApplyInputEvent()` 更新已有控件时发一次 `ControlStateChanged(deviceId, controlId)`。
- [x] `ControlStateChanged` 发射时快照已经写入；在 signal slot 内调用 `isPressed()` / `value()` / `axisX()` / `axisY()` / `displayValue()` 应读到新值而不是旧值。
- [x] 不同设备同名控件发出的 `deviceId` 可区分。
- [x] Trigger 更新时 signal payload 为对应 trigger control id。
- [x] Axis2D 更新时 signal payload 为对应 stick control id。
- [x] `RemoveDevice()` 删除已有状态时发一次 `DeviceStateRemoved(deviceId)`。
- [x] `RemoveDevice()` 找不到设备时不发 `DeviceStateRemoved`。
- [x] `clear()` 清空非空 model 时发一次 `InputStateReset()`。
- [x] `clear()` 空 model 时不发 `InputStateReset()`。
- [x] snapshot 查询函数行为保持不变。

`ZAppController`：

- [x] fake backend `EmitInput` + `pumpOnce()` 会触发 `InputStateModel.ControlStateChanged`。
- [x] fake backend `RemoveDevice` + `pumpOnce()` 会触发 `InputStateModel.DeviceStateRemoved`。
- [x] fake backend `AddDevice` + `pumpOnce()` 会触发 `DeviceModel.DeviceAdded`。
- [x] fake backend duplicate/update path 如果走 `AddOrUpdateDevice()` 更新已有设备，会触发 `DeviceModel.DeviceUpdated`。
- [x] fake backend `RemoveDevice` + `pumpOnce()` 会触发 `DeviceModel.DeviceRemoved`。
- [x] fake backend 添加设备但不发送输入，再 `RemoveDevice` + `pumpOnce()` 时，`DeviceModel` 会移除设备；`InputStateModel.DeviceStateRemoved` 不触发，这是预期行为。
- [x] Error 后重新 initialize 清理状态时触发 `InputStateReset()`。
- [x] 幂等 initialize 不清理状态，也不触发 `InputStateReset()`。

QML 手工验证：

- [x] 启动后连接一台手柄，UI 自动选中该设备。
- [x] 不点击设备卡片也能看到按钮、LT/RT、摇杆随输入变化。
- [x] 切换设备后，显示切换到对应设备的快照状态。
- [x] 拔出非当前设备后，当前选择不变化。
- [x] 拔出当前设备后自动选择剩余设备；无剩余设备则回到空状态。
- [x] 连接一台设备但不按任何输入，直接拔出该设备，当前选择仍能正确清空或切到剩余设备。
- [x] 设备名称更新时，如果更新的是当前设备，Gamepad View 标题同步刷新。
- [x] 删除 `revision` 相关代码后 UI 仍然更新。

## Acceptance Criteria

- [x] `revision` / `_inputRev` 从 C++ 和 QML 中完全移除。
- [x] 设备选择刷新依赖 `DeviceAdded` / `DeviceUpdated` / `DeviceRemoved` / `DeviceModelReset` 等真实 device lifecycle signal。
- [x] 输入状态刷新依赖 `ControlStateChanged` / `DeviceStateRemoved` / `InputStateReset` 等真实 signal。
- [x] QML 可视控件绑定到明确命名的本地状态属性，而不是绑定到 `Q_INVOKABLE` + hidden token。
- [x] 按键事件、LT/RT 数值和摇杆位置在不手动点击设备卡片的情况下也能更新。
- [x] 设备切换和设备拔出不会显示 stale input state。
- [x] `cmake --build build --config Debug` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 后续实现 Binding Capture，用同一套 `ControlStateChanged` 事件捕获下一次输入。
- [ ] 后续实现 `ZLogModel`，把 runtime pump records 暴露给事件日志。
- [ ] 后续实现 Binding Editor 的 rule 创建和 profile 保存。
- [ ] 后续考虑把 `InputControlState` 从 QML 局部组件提升为可复用 QML 文件或 C++ QObject proxy。
