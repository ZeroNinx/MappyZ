# TODO: UI Bridge InputStateModel Module

## Next Step

下一步实现 `source/UI/Bridge/InputStateModel.h/.cpp`，把运行时 `SInputEvent` 暴露给 QML，让 Gamepad View 能显示真实按钮、扳机、摇杆和方向键状态。

优先做这个模块的理由：

- [x] `ZDeviceModel` 已完成，设备列表已经接入真实 runtime。
- [x] `ZAppController` 已通过 pump timer 持续驱动 `ZRuntimeEventPump`。
- [x] `ZRuntimeEventPump` 已有 `SetInputEventHandler()`，可以在不修改 Backend/Core 的前提下把输入事件送到 UI Bridge。
- [x] 当前 Gamepad View 仍显示静态 `LT 0.18`、`RT 0.74` 和固定高亮，用户还看不到实时输入。
- [x] 实时输入状态是后续“等待输入进行绑定”和事件日志的基础。

## Scope

本轮实现 UI Bridge 层输入状态 model，并把现有 QML Gamepad View 绑定到它。

包含：

- [ ] 新增 `source/UI/Bridge/InputStateModel.h`
- [ ] 新增 `source/UI/Bridge/InputStateModel.cpp`
- [ ] 新增 `tests/UI/Bridge/InputStateModelTests.cpp`
- [ ] `ZInputStateModel : QAbstractListModel`
- [ ] `ZAppController` 增加 `inputStateModel` 只读 Q_PROPERTY
- [ ] AppController 初始化 runtime 后注册 `RuntimeEventPump` input event handler
- [ ] AppController `pumpOnce()` 后输入状态随事件刷新
- [ ] `ui/Main.qml` 的 Gamepad View 使用真实输入状态高亮按钮、扳机和摇杆
- [ ] CMake 把 InputStateModel 加入 `MappyZUIBridge` 和 `MappyZUIBridgeTests`

不做：

- [ ] 不实现 `ZLogModel`
- [ ] 不实现 profile 列表、active profile 选择或自动匹配
- [ ] 不创建、保存或应用新的 mapping rule
- [ ] 不实现完整 Binding Editor 工作流
- [ ] 不实现设备外观识别或真实手柄皮肤
- [ ] 不让 QML 直接访问 `IInputBackend`、`ZRuntimeHost` 或 `ZRuntimeEventPump`
- [ ] 不修改 Core、Runtime、Backend 的公开输入事件契约，除非发现必要缺口并先补测试
- [ ] 不引入 Qt 到 Core、Runtime 或 Backend

## Architecture Boundary

- [ ] `ZInputStateModel` 属于 UI Bridge 层。
- [ ] 依赖 Qt Core 和 Core `SInputEvent` 数据类型。
- [ ] 不依赖 SDL、Win32、QML 文件或输出后端。
- [ ] `ZInputStateModel` 不拥有 runtime，不调用 backend。
- [ ] 输入事件来源是 `ZRuntimeEventPump` 已标准化的 `SInputEvent`。
- [ ] QML 只通过 `appController.inputStateModel` 读取输入状态。
- [ ] `ZDeviceModel` 继续只负责设备列表，不混入输入状态。
- [ ] `ZAppController` 继续负责 runtime lifecycle、pump 调度和 UI Bridge models 的连接。

## Proposed Types

- [ ] 新增 `class ZInputStateModel final : public QAbstractListModel`
- [ ] 每行表示一个 `(deviceId, controlId)` 的最近状态，重复输入更新同一行。
- [ ] Roles：
  - `DeviceIdRole`
  - `ControlIdRole`
  - `ControlTypeRole`
  - `EventTypeRole`
  - `ValueRole`
  - `AxisXRole`
  - `AxisYRole`
  - `PressedRole`
  - `DisplayValueRole`
  - `SequenceRole`
- [ ] Public API：
  - `int rowCount(const QModelIndex& Parent = QModelIndex()) const override`
  - `QVariant data(const QModelIndex& Index, int Role) const override`
  - `QHash<int, QByteArray> roleNames() const override`
  - `void ApplyInputEvent(const SInputEvent& Event)`
  - `void RemoveDevice(const SDeviceId& DeviceId)`
  - `void ClearDevice(const SDeviceId& DeviceId)`
  - `Q_INVOKABLE void clear()`
  - `Q_INVOKABLE bool isPressed(QString deviceId, QString controlId) const`
  - `Q_INVOKABLE double value(QString deviceId, QString controlId) const`
  - `Q_INVOKABLE double axisX(QString deviceId, QString controlId) const`
  - `Q_INVOKABLE double axisY(QString deviceId, QString controlId) const`
  - `Q_INVOKABLE QString displayValue(QString deviceId, QString controlId) const`
  - `Q_INVOKABLE QString latestControlId(QString deviceId = QString()) const`

## Model Behavior

- [ ] `rowCount()` 返回当前已知控件状态数量。
- [ ] `data()` 对无效 index、非 0 列或未知 role 返回空 `QVariant`。
- [ ] `ApplyInputEvent()`：
  - 新 `(deviceId, controlId)` 插入一行并发出 `rowsInserted`
  - 已存在状态更新该行并发出 `dataChanged`
  - 更新 `Sequence`，便于 UI 判断最近输入
- [ ] `PressedRole`：
  - Button/Hat 的 Pressed 为 true，Released 为 false
  - Trigger 可按阈值 `Value > 0.5` 返回 true
  - Axis/Axis2D 默认 false，避免把摇杆偏移误判为按钮按下
- [ ] `DisplayValueRole`：
  - Button/Hat 显示 `"pressed"` / `"released"`
  - Trigger/Axis1D 显示归一化数值，保留 2 位小数
  - Axis2D 显示 `"(x, y)"`，各保留 2 位小数
- [ ] `RemoveDevice()` 删除该设备全部控件状态并发出正确 rowsRemoved/reset 信号。
- [ ] `clear()` 清空所有状态，重复调用安全。
- [ ] 查询 invokable 找不到设备或控件时返回安全默认值。
- [ ] `latestControlId()` 默认返回全局最近输入；传入 deviceId 时返回该设备最近输入。

## AppController Integration

- [ ] `ZAppController` 内部持有 `ZInputStateModel InputStateModelInstance`。
- [ ] 新增 `Q_PROPERTY(QObject* inputStateModel READ InputStateModel CONSTANT)`。
- [ ] 新增 `QObject* InputStateModel()`。
- [ ] `initializeRuntime()` 成功后给 `GetRuntimeHost().GetEventPump()` 设置 input handler：
  - input -> `InputStateModelInstance.ApplyInputEvent(Event)`
- [ ] device disconnected handler 同时调用：
  - `DeviceModelInstance.RemoveDevice(DeviceId)`
  - `InputStateModelInstance.RemoveDevice(DeviceId)`
- [ ] `stopRuntime()` 不清空 input state；停止运行时不等于忘记最后已知状态。
- [ ] `initializeRuntime()` 失败时不修改 input state model。
- [ ] 如果 runtime 被重新 initialize，先清理或重建 input state，避免旧设备状态误导 UI。

## QML Binding Plan

- [ ] `Gamepad View` 标题仍来自选中设备 `displayName`。
- [ ] 无选中设备时显示空状态，控件恢复未按下视觉。
- [ ] Face buttons / shoulders / start / back / dpad 高亮读取 `inputStateModel.isPressed(selectedDevice, controlId)`。
- [ ] LT/RT 文案读取 `inputStateModel.displayValue(selectedDevice, "left_trigger")` 和 `"right_trigger"`。
- [ ] LT/RT 填充或高亮强度读取 `inputStateModel.value(...)`。
- [ ] LS/RS 摇杆点位读取 `axisX/axisY`，保持在稳定边界内，不让布局跳动。
- [ ] `Selected input` 可显示 `latestControlId(selectedDevice)`，但不创建绑定规则。
- [ ] 保留静态 mapping/event mock 数据，标注为后续模块，不参与输入状态。

## Tests

- [ ] `ZInputStateModel` 默认 rowCount 为 0。
- [ ] `ApplyInputEvent()` 新控件插入一行并提供所有 roles。
- [ ] `ApplyInputEvent()` 同一 `(deviceId, controlId)` 更新行并发出 dataChanged。
- [ ] 不同 device 的同名 control 分成不同行。
- [ ] Button Pressed/Released 正确更新 `PressedRole` 和 `DisplayValueRole`。
- [ ] Trigger value 正确更新 `ValueRole`、`PressedRole` 和 `DisplayValueRole`。
- [ ] Axis2D 正确更新 `AxisXRole`、`AxisYRole` 和 `DisplayValueRole`。
- [ ] `latestControlId()` 返回最近输入，按 deviceId 过滤时正确。
- [ ] `RemoveDevice()` 删除指定设备所有状态。
- [ ] `clear()` 清空 model 并发出 reset。
- [ ] 查询 invokable 对未知设备/控件返回安全默认值。
- [ ] `ZAppController::inputStateModel` 返回非空 QObject。
- [ ] AppController start 后 fake backend `EmitInput` + `pumpOnce` 更新 InputStateModel。
- [ ] AppController fake backend RemoveDevice + pumpOnce 清理该设备输入状态。
- [ ] header 不包含 SDL 或 Win32 头。

## Manual Test Checklist

- [ ] 无手柄启动，Gamepad View 显示空状态，应用不崩溃。
- [ ] 连接手柄后点击设备，Gamepad View 显示该设备名称。
- [ ] 按 A/B/X/Y，按钮高亮随按下和松开变化。
- [ ] 按 LT/RT，数值和高亮强度随扳机变化。
- [ ] 推动左右摇杆，LS/RS 指示点随轴值移动。
- [ ] 按 D-pad，上下左右高亮正确。
- [ ] 切换选中设备时，Gamepad View 显示对应设备的输入状态。
- [ ] 拔出当前设备后，Gamepad View 回到无选择/空状态。
- [ ] Mapping On/Off 不影响原始输入状态显示。
- [ ] 关闭应用后 runtime 停止，终端无明显析构/线程错误。

## Acceptance Criteria

- [ ] QML 可通过 `appController.inputStateModel` 读取实时输入状态。
- [ ] `ui/Main.qml` 的 Gamepad View 不再使用静态 LT/RT 数值。
- [ ] 选中设备的按钮、扳机、摇杆和 D-pad 能随真实输入变化。
- [ ] QML 不直接访问 backend/runtime 内部对象。
- [ ] Core、Runtime、Backend 不依赖 Qt。
- [ ] `cmake --build build --config Debug` 通过。
- [ ] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [ ] `git diff --check` 通过。
- [ ] 新增和修改的文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 后续实现 `ZLogModel` 或轻量 event log bridge。
- [ ] 后续实现 Binding Capture：等待下一次输入并填充 Binding Editor。
- [ ] 后续实现 profile directory/active profile 管理。
- [ ] 后续实现绑定 UI：选择输出动作、创建规则、保存 profile。
- [ ] 后续增加真实输出开关，允许 UI 从 NullOutput 切换到 Windows SendInput。
