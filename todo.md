# TODO: UI Bridge Binding Capture

## Next Step

下一步实现轻量 Binding Capture：当用户点击 `Capture Input` 后，UI 进入等待输入状态；下一次来自当前设备的真实输入会填充 `selectedControl`，并退出 capture。

这轮只做“捕获输入控件”这一层，不创建 mapping rule，不保存 profile。

优先做这个模块的理由：

- [x] `ZInputStateModel` 已有 `ControlStateChanged(deviceId, controlId)` 语义 signal。
- [x] QML 已经用真实输入状态驱动 Gamepad View。
- [x] Binding Editor 目前有 `Capture Input` 按钮和 `selectedControl` 展示，但还只是静态/手动选择。
- [x] 捕获下一次输入是后续创建映射规则的前置步骤。

## Scope

包含：

- [x] 新增 UI Bridge 层 capture 状态对象，暂定 `ZInputCaptureModel`。
- [x] `ZAppController` 持有并暴露 `inputCapture` 只读 Q_PROPERTY。
- [x] `ZAppController` 在 input event handler 中同时更新 `InputStateModel` 和 capture 状态。
- [x] QML `Capture Input` 按钮改为调用 capture API，而不是直接切换本地 bool。
- [x] QML `selectedControl` 在 capture 完成后更新为捕获到的 `controlId`。
- [x] QML 在 capture active 时显示等待态。
- [x] 增加单元测试覆盖 capture 生命周期和 AppController 集成。
- [x] 更新 CMake。

不做：

- [x] 不创建、修改或保存 `SMappingRule`。
- [x] 不修改 profile 文件。
- [x] 不实现 action output capture。
- [x] 不实现组合键、长按、双击或宏输入。
- [x] 不修改 Core、Runtime、Backend 的输入事件契约。
- [x] 不把 capture 逻辑放进 Core/Runtime；它属于 UI Bridge workflow。

## Proposed Types

新增 `source/UI/Bridge/InputCaptureModel.h/.cpp`：

- [x] `class ZInputCaptureModel final : public QObject`
- [x] QML properties：
  - `bool active READ IsActive NOTIFY CaptureStateChanged`
  - `QString deviceId READ DeviceId NOTIFY CaptureCompleted`
  - `QString controlId READ ControlId NOTIFY CaptureCompleted`
  - `QString displayText READ DisplayText NOTIFY CaptureStateChanged`
- [x] Public API：
  - `Q_INVOKABLE void begin(QString deviceId = QString())`
  - `Q_INVOKABLE void cancel()`
  - `void HandleInputEvent(const SInputEvent& Event)`
  - `bool IsActive() const`
  - `QString DeviceId() const`
  - `QString ControlId() const`
  - `QString DisplayText() const`
- [x] Signals：
  - `void CaptureStateChanged()`
  - `void CaptureCompleted(QString deviceId, QString controlId)`
  - `void CaptureCancelled()`

内部状态：

- [x] `bool bActive = false`
- [x] `QString TargetDeviceId`
- [x] `QString CapturedDeviceId`
- [x] `QString CapturedControlId`

## Behavior

- [x] `begin(deviceId)`：
  - 设置 active 为 true。
  - 记录 target device；空字符串表示接受任意设备输入。
  - 清空上一轮 captured device/control。
  - 发 `CaptureStateChanged()`。
- [x] `cancel()`：
  - 如果 inactive，no-op。
  - 如果 active，切回 inactive，发 `CaptureStateChanged()` 和 `CaptureCancelled()`。
- [x] `HandleInputEvent(Event)`：
  - inactive 时 no-op。
  - target device 非空且不匹配时 no-op，继续等待。
  - target device 匹配后，先判断是否是有效 capture 输入；无效输入 no-op，继续等待。
  - 有效输入才记录 `Event.DeviceId.Value` 和 `Event.ControlId`。
  - 设置 active 为 false。
  - 先发 `CaptureStateChanged()`，再发 `CaptureCompleted(deviceId, controlId)`。
- [x] 有效 capture 输入过滤规则：
  - Button / Hat：仅 `Pressed` 事件触发 capture，`Released` 忽略。
  - Trigger：`Value > 0.5` 才触发 capture。
  - Axis1D：`abs(Value) > 0.7` 才触发 capture。
  - Axis2D：向量幅度 `sqrt(X*X + Y*Y) > 0.7` 才触发 capture。
  - 其他未知类型默认忽略。
- [x] 过滤规则应封装为私有 helper，暂定 `static bool IsCaptureWorthyInput(const SInputEvent& Event)`，避免散落在 `HandleInputEvent()` 中。
- [x] capture 完成只捕获 control id，不判断 mapping 是否存在。
- [x] capture 不吞掉输入事件；Gamepad View 仍然正常更新输入状态。
- [x] `DisplayText()`：
  - active 且有 target device：`Waiting for input...`
  - active 且无 target device：`Waiting for any device input...`
  - inactive 且已有 captured control：返回 captured control id。
  - inactive 且未捕获：返回空字符串。

## AppController Integration

- [x] `ZAppController` 内部持有 `ZInputCaptureModel InputCaptureInstance`。
- [x] 新增 `Q_PROPERTY(QObject* inputCapture READ InputCapture CONSTANT)`。
- [x] 新增 `QObject* InputCapture()`。
- [x] input event handler 中调用顺序：
  - `InputStateModelInstance.ApplyInputEvent(Event)`
  - `InputCaptureInstance.HandleInputEvent(Event)`
- [x] 顺序理由：capture 完成时，QML 读取 `inputStateModel` 快照应已经是新状态。
- [x] `stopRuntime()` 不自动取消 capture；用户可停运行时后继续保持等待态。
- [x] `initializeRuntime()` Error 重建路径不需要清空 capture，除非后续发现 UX 问题。

## QML Binding Plan

- [x] 移除本地 `captureMode` 作为权威状态。
- [x] `Capture Input` 按钮调用 `appController.inputCapture.begin(root.selectedDevice)`。
- [x] `Cancel` 或再次点击 active 状态按钮调用 `appController.inputCapture.cancel()`。
- [x] selected control 文案：
  - active 时显示 `appController.inputCapture.displayText`
  - inactive 时显示 `root.selectedControl`
- [x] 监听 `appController.inputCapture.CaptureCompleted(deviceId, controlId)`：
  - 如果 `deviceId === root.selectedDevice`，设置 `root.selectedControl = controlId`。
  - 如果捕获来自其他设备且当前没有选中设备，可选择该设备；本轮先不做跨设备自动切换。
- [x] `ControlDot` 点击仍可手动设置 `selectedControl`，并取消 capture。
- [x] 设备切换时不自动取消 capture；capture 以 begin 时传入的 target device 为准。

## Tests

`ZInputCaptureModel`：

- [x] 默认 inactive，device/control/displayText 为空。
- [x] `begin("dev_1")` 进入 active，发一次 `CaptureStateChanged()`。
- [x] active 时 `HandleInputEvent()` 对非 target device no-op。
- [x] active 时 `HandleInputEvent()` 对 target device 完成 capture。
- [x] Button Released 不完成 capture。
- [x] Hat Released 不完成 capture。
- [x] Trigger `Value <= 0.5` 不完成 capture，`Value > 0.5` 完成 capture。
- [x] Axis1D `abs(Value) <= 0.7` 不完成 capture，超过阈值完成 capture。
- [x] Axis2D 向量幅度 `<= 0.7` 不完成 capture，超过阈值完成 capture。
- [x] 摇杆微小漂移事件不会抢先完成 capture。
- [x] capture 完成后 active 为 false，device/control 可查询。
- [x] capture 完成时发 `CaptureStateChanged()` 和 `CaptureCompleted(deviceId, controlId)`。
- [x] inactive 时 `HandleInputEvent()` 不发信号。
- [x] `cancel()` active capture 时发 `CaptureStateChanged()` 和 `CaptureCancelled()`。
- [x] `cancel()` inactive 时 no-op。
- [x] `begin()` 覆盖上一轮 captured control。

`ZAppController`：

- [x] `inputCapture` property 返回非空 QObject。
- [x] begin capture 后，fake backend `EmitInput` + `pumpOnce()` 完成 capture。
- [x] capture 完成时 `InputStateModel` 快照已更新。
- [x] 非 target device 输入不会完成 capture。
- [x] capture 不影响 `LastPumpSummary` 统计。

QML 手工验证：

- [ ] 点击 `Capture Input` 后显示等待态。
- [ ] 按当前设备 A/B/X/Y 后，Selected control 更新为对应 control id。
- [ ] 按 LT/RT 后，Selected control 更新为对应 trigger id。
- [ ] capture 等待期间 Gamepad View 仍实时高亮。
- [ ] 点击手柄图上的控件会取消 capture 并手动选择该控件。
- [ ] 当前设备未选中时，点击 capture 不崩溃；本轮可保持等待任意输入或提示等待状态。

## Acceptance Criteria

- [x] Capture workflow 不依赖 `revision`、polling 或 magic int。
- [x] Capture workflow 使用真实输入事件，而不是从 UI 高亮状态反推。
- [x] 捕获完成后 `selectedControl` 能由真实输入更新。
- [x] 不产生任何 mapping rule 或 profile 写入。
- [x] Core、Runtime、Backend 不依赖 Qt。
- [x] `cmake --build build --config Debug` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 后续实现 Binding Editor 创建 `SMappingRule`。
- [ ] 后续实现 active profile 修改和保存。
- [ ] 后续实现 `ZLogModel`，把 runtime pump records 暴露给事件日志。
- [ ] 后续考虑把 QML 局部 `InputControlState` 提升为可复用 QML 文件或 C++ QObject proxy。
