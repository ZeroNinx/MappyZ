# TODO: UI Bridge DeviceModel Module

## Next Step

下一步实现 `source/UI/Bridge/DeviceModel.h/.cpp` 的最小版本，把运行时设备列表暴露给 QML。`ZDeviceModel` 只负责设备快照展示和热插拔更新，不读取输入事件、不处理 mapping、不直接订阅后端 callback。

优先做这个模块的理由：

- [ ] `ZAppController` 已经作为 QML 根控制器注入。
- [ ] 蓝图 v0.1 要求启动后枚举设备、支持热插拔、UI 显示设备状态。
- [ ] 当前 QML 仍使用静态 `devicesModel`，还没有真实运行时设备入口。
- [ ] 设备列表是后续输入状态面板、绑定 UI、profile 设备匹配的基础数据。

## Scope

本轮只做设备列表 model，不做输入状态 model、不做日志 model、不做 QML 大改版。

包含：

- [ ] `source/UI/Bridge/DeviceModel.h`
- [ ] `source/UI/Bridge/DeviceModel.cpp`
- [ ] `tests/UI/Bridge/DeviceModelTests.cpp`
- [ ] `ZDeviceModel : QAbstractListModel`
- [ ] `ZAppController` 增加 `deviceModel` 只读 Q_PROPERTY
- [ ] `ZApplicationBootstrap` 增加只读 `ListInputDevices()` 快照访问器
- [ ] AppController 初始化/start 后刷新设备快照
- [ ] AppController 通过 `ZRuntimeEventPump` device handlers 更新 DeviceModel
- [ ] CMake 把 DeviceModel 加入 `MappyZUIBridge` 和 `MappyZUIBridgeTests`

不做：

- [ ] 不实现 `ZInputStateModel`
- [ ] 不实现 `ZLogModel`
- [ ] 不实现设备图标、厂商数据库或友好名称映射
- [ ] 不实现 profile 自动匹配
- [ ] 不让 QML 直接访问 `IInputBackend`、`ZRuntimeHost` 或 `ZRuntimeEventPump`
- [ ] 不重新启用 `ZDeviceManager` 订阅 backend callback，避免和 `ZBackendEventQueue` 抢回调
- [ ] 不复制或移植第三方项目代码结构

## Architecture Boundary

- [ ] `ZDeviceModel` 属于 UI Bridge 层。
- [ ] 依赖 Qt Core 和 Core `SDeviceInfo` 数据类型。
- [ ] 不依赖 SDL、Win32、QML 文件或输出后端。
- [ ] `ZDeviceModel` 不拥有 runtime，不调用 backend。
- [ ] 设备热插拔来源是 `ZRuntimeEventPump` 已分发的 device events。
- [ ] 初始设备快照来源是 `ZApplicationBootstrap::ListInputDevices()`。

## Proposed Types

- [ ] 新增 `class ZDeviceModel final : public QAbstractListModel`
- [ ] Roles：
  - `DeviceIdRole = Qt::UserRole + 1`
  - `NameRole`
  - `BackendRole`
  - `VendorIdRole`
  - `ProductIdRole`
  - `GuidRole`
  - `InstanceIdRole`
  - `DisplayNameRole`
- [ ] Public API：
  - `int rowCount(const QModelIndex& Parent = QModelIndex()) const override`
  - `QVariant data(const QModelIndex& Index, int Role) const override`
  - `QHash<int, QByteArray> roleNames() const override`
  - `Q_INVOKABLE void clear()`
  - `Q_INVOKABLE QString deviceIdAt(int row) const`
  - `void ReplaceDevices(TVector<SDeviceInfo> Devices)`
  - `void AddOrUpdateDevice(const SDeviceInfo& DeviceInfo)`
  - `void RemoveDevice(const SDeviceId& DeviceId)`
  - `TVector<SDeviceInfo> ListDevicesSnapshot() const`
- [ ] `ZAppController` 新增属性：
  - `Q_PROPERTY(QObject* deviceModel READ DeviceModel CONSTANT)`
  - `QObject* DeviceModel()`

## DeviceModel Behavior

- [ ] `rowCount()` 返回当前设备数量。
- [ ] `data()` 对无效 index 或未知 role 返回空 `QVariant`。
- [ ] `DisplayNameRole` 优先返回 `Name`，为空时回退 `DeviceId.Value`。
- [ ] `ReplaceDevices()` 使用 `beginResetModel()` / `endResetModel()`。
- [ ] `ReplaceDevices()` 对重复 `DeviceId` 保留最后一个条目，避免 UI 出现重复设备。
- [ ] `AddOrUpdateDevice()`：
  - 新设备使用 `beginInsertRows()` / `endInsertRows()`
  - 已存在设备只更新该行并发出 `dataChanged()`
- [ ] `RemoveDevice()`：
  - 找到时使用 `beginRemoveRows()` / `endRemoveRows()`
  - 找不到时 no-op
- [ ] `clear()` 清空 model，重复调用安全。
- [ ] `ListDevicesSnapshot()` 返回拷贝，调用方修改不影响 model。

## AppController Integration

- [ ] `ZAppController` 内部持有 `ZDeviceModel DeviceModelInstance`。
- [ ] `DeviceModel()` 返回 `&DeviceModelInstance`。
- [ ] `initializeRuntime()` 成功后调用 `RefreshDeviceModelFromBootstrap()`。
- [ ] `startRuntime()` 成功后再次调用 `RefreshDeviceModelFromBootstrap()`，覆盖真实后端 start 后才完成枚举的场景。
- [ ] `initializeRuntime()` 成功后给 `GetRuntimeHost().GetEventPump()` 设置 device handlers：
  - connected -> `DeviceModelInstance.AddOrUpdateDevice(DeviceInfo)`
  - disconnected -> `DeviceModelInstance.RemoveDevice(DeviceId)`
- [ ] handlers 捕获 `this`，生命周期由 `ZAppController` 控制；析构时先 `Bootstrap.StopRuntime()`，host 会 detach event queue。
- [ ] `stopRuntime()` 不清空设备 model；停止运行时不等于忘记最后已知设备。
- [ ] `initializeRuntime()` 失败时不修改现有 device model。

## ApplicationBootstrap Change

- [ ] 新增 public 方法：
  - `TVector<SDeviceInfo> ListInputDevices() const`
- [ ] 未 initialize 或 input backend 不存在时返回空 vector。
- [ ] initialize 成功后委托 `InputBackend->ListDevices()`。
- [ ] 不暴露 mutable backend，也不暴露 backend 指针。
- [ ] 该方法仅用于 UI snapshot，不改变 backend 状态。

## CMake Plan

- [ ] `MappyZUIBridge` 增加：
  - `source/UI/Bridge/DeviceModel.cpp`
- [ ] `MappyZUIBridgeTests` 增加：
  - `tests/UI/Bridge/DeviceModelTests.cpp`
- [ ] 继续使用现有自定义 `tests/UI/Bridge/Main.cpp`。
- [ ] 测试继续链接 `Qt6::Test` 和 `Catch2::Catch2`。

## Tests

- [ ] `ZDeviceModel` 默认 rowCount 为 0。
- [ ] `ReplaceDevices()` 填充设备并提供所有 roles。
- [ ] `ReplaceDevices()` 对重复 DeviceId 去重，保留最后值。
- [ ] `AddOrUpdateDevice()` 新设备插入一行并发出 rowsInserted。
- [ ] `AddOrUpdateDevice()` 已存在设备更新行并发出 dataChanged。
- [ ] `RemoveDevice()` 删除存在设备并发出 rowsRemoved。
- [ ] `RemoveDevice()` 删除不存在设备 no-op。
- [ ] `clear()` 清空 model 并发出 reset。
- [ ] `deviceIdAt()` 越界返回空字符串。
- [ ] `ListDevicesSnapshot()` 返回拷贝。
- [ ] `ZAppController::deviceModel` 返回非空 QObject。
- [ ] AppController initialize 后从 fake backend 初始快照刷新 DeviceModel。
- [ ] AppController start 后 fake backend AddDevice + pumpOnce 更新 DeviceModel。
- [ ] AppController fake backend RemoveDevice + pumpOnce 删除 DeviceModel 行。
- [ ] AppController stopRuntime 不清空 DeviceModel。
- [ ] header 不包含 SDL 或 Win32 头。

## Acceptance Criteria

- [ ] `cmake --build build` 通过。
- [ ] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [ ] `git diff --check` 通过。
- [ ] 新增和修改的文本文件使用 CRLF 行尾。
- [ ] QML 可通过 `appController.deviceModel` 访问设备列表 model。
- [ ] DeviceModel 不直接订阅 backend callback，不和 event queue 抢回调。

## Follow-Up Module

- [ ] 后续实现 `ZInputStateModel`，显示最近输入和控件状态。
- [ ] 后续把 `ui/Main.qml` 的设备列表绑定到 `appController.deviceModel`。
- [ ] 后续实现 `ZLogModel` 或轻量 event log bridge。
- [ ] 后续实现 profile directory/active profile 管理。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
