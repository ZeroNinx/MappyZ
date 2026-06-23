# TODO: QML Device Panel Binding Module

## Next Step

下一步更新 `ui/Main.qml`，把设备面板从静态 `devicesModel` 切换到 `appController.deviceModel`，并接入 `ZAppController` 已有的 runtime lifecycle/pump 接口，让 UI 可以显示真实运行时设备列表。

优先做这个模块的理由：

- [x] `ZDeviceModel` 已完成，QML 可通过 `appController.deviceModel` 访问设备列表。
- [x] `ZAppController` 已注入 QML root context，且已有 initialize/start/stop/pump timer 接口。
- [x] 当前 `Main.qml` 仍使用静态 `devicesModel`，用户看不到真实后端枚举和热插拔结果。
- [x] 设备列表是后续 `ZInputStateModel`、绑定 UI、profile 设备匹配的视觉入口。
- [x] 这一阶段可以只改 UI 绑定，不扩大 C++ Runtime 或 Backend 边界。

## Scope

本轮优先只把现有 QML 设备面板接入真实 `DeviceModel` 和 runtime 状态，不做新的 C++ model。

包含：

- [ ] 修改 `ui/Main.qml`
- [ ] 只有 QML 无法可靠处理选中设备显示、设备数量或设备存在性检查时，才在 `ZDeviceModel` / `ZAppController` 增加极小 QML-friendly property/invokable，并补对应测试
- [ ] 移除或停用静态 `devicesModel`
- [ ] `Devices` 面板的 `Repeater/ListView` 使用 `appController.deviceModel`
- [ ] 设备卡片显示 `displayName`、`backend`、`vendorId`、`productId`、`guid` 或 `instanceId`
- [ ] `selectedDevice` 从设备名称切换为稳定 `deviceId`
- [ ] 空设备列表时显示明确 empty state
- [ ] 顶部或状态栏显示 `runtimeState`、`runtimeMessage`、`outputState`
- [ ] UI 触发 `initializeRuntime(true)` / `startRuntime()` / `stopRuntime()`
- [ ] UI 启动 pump timer，确保热插拔事件能更新 model
- [ ] `mappingEnabled` 绑定到 `appController.mappingEnabled`

不做：

- [ ] 不实现 `ZInputStateModel`
- [ ] 不实现 `ZLogModel`
- [ ] 不实现 profile 列表、active profile 选择或自动匹配
- [ ] 不实现绑定编辑器真实保存
- [ ] 不实现设备图标、厂商数据库或手柄外观识别
- [ ] 不直接从 QML 访问 `IInputBackend`、`ZRuntimeHost` 或 `ZRuntimeEventPump`
- [ ] 不让 QML 直接读取 SDL 或发送 Win32 输出
- [ ] 不重构整套 QML 布局；只替换必要的数据绑定和状态展示

## Architecture Boundary

- [ ] QML 只通过 `appController` 和 `appController.deviceModel` 访问运行时状态。
- [ ] QML 不知道 SDL、Win32、Backend、RuntimeHost 的具体类型。
- [ ] `ZDeviceModel` 继续是设备列表唯一 QML model 来源。
- [ ] `ZAppController` 继续负责 runtime lifecycle 和 pump 调度。
- [ ] 本轮不修改 Core、Runtime、Backend 接口。
- [ ] 如果需要辅助 QML 状态，优先使用已有 `ZAppController` properties；确实需要新增 API 时，只放在 UI Bridge 层，不修改 Core、Runtime、Backend。

## UI Behavior

- [ ] 应用加载后自动调用一次 `initializeRuntime(true)`，先使用 NullOutput 避免 UI 预览时产生真实键鼠输出。
- [ ] initialize 成功后调用 `startRuntime()`。
- [ ] start 成功后调用 `startPumpTimer(16)` 或类似间隔。
- [ ] initialize/start 失败时，在现有 event/status 区显示错误消息。
- [ ] 设备列表为空时显示 “No gamepads connected” 或等价短文案。
- [ ] 新设备插入时自动显示在设备面板。
- [ ] 设备断开时从设备面板移除。
- [ ] 当前选中设备断开时，选择回退为空或第一台设备。
- [ ] 点击设备卡片时更新 `selectedDevice` 为该行 `deviceId`。
- [ ] Gamepad View 标题使用选中设备 `displayName`，无选择或选中设备断开时显示空状态。
- [ ] 状态栏设备数量来自设备列表控件的 `count` 或等价可绑定状态，不假设 QML 可以直接稳定绑定 `deviceModel.rowCount()`。

## QML Binding Plan

- [ ] 将 `selectedDevice` 改为保存 device id，而不是 display name。
- [ ] 需要标题显示时，维护 `selectedDeviceDisplayName` 或等价 UI 状态，避免把 display name 当主键。
- [ ] 设备卡片高亮条件改为 `root.selectedDevice === deviceId`。
- [ ] 设备名称显示 `displayName`。
- [ ] backend 显示 `backend`。
- [ ] profile 字段暂时显示 `"Unassigned"` 或 `"Manual"`，避免误导为真实 profile 匹配。
- [ ] state tag 可基于 runtime state 显示 `"Ready"` / `"Running"` / `"Stopped"`，不伪造设备自身状态。
- [ ] 状态栏使用 `appController.runtimeState`、`appController.outputState`、`appController.lastDrainedEventCount` 和设备列表控件的 `count`。
- [ ] top bar 的 Mapping On/Off 按钮读写 `appController.mappingEnabled`。
- [ ] 可保留静态 mapping/event mock 数据，但标注为后续模块，不参与设备列表。

## Lifecycle Plan

- [ ] `Component.onCompleted` 中初始化 runtime。
- [ ] initialize/start 失败时不启动 pump timer。
- [ ] start 成功后启动 pump timer。
- [ ] `Window.onClosing` 或组件销毁路径调用 `appController.stopPumpTimer()` 和 `appController.stopRuntime()`，如果 QML lifecycle 支持。
- [ ] 不在 QML 中手动 drain backend queue；只调用 `appController.pumpOnce()` 或 pump timer。
- [ ] 保持 `useNullOutput = true`，真实输出接入留给后续明确开关，避免启动 UI 后立刻发送系统输入。

## Tests

第一版 QML 绑定以构建验证和手动 smoke test 为主；不强行引入复杂 QML 自动化。

- [ ] `cmake --build build --config Debug` 能通过 QML 编译/cache。
- [ ] `ctest --test-dir build --output-on-failure -C Debug` 继续通过。
- [ ] 现有 `ZAppController` / `ZDeviceModel` 测试不需要修改或继续通过。
- [ ] 如果新增 UI Bridge invokable/property，补充对应 C++ 单元测试。
- [ ] QML 启动时不存在 `ReferenceError`、role 名拼写错误或 binding loop。
- [ ] 无手柄启动时显示 empty state。
- [ ] Fake/真实后端枚举到设备时，设备面板显示真实设备条目。
- [ ] 热插拔时设备列表随 pump 更新。
- [ ] Mapping On/Off 按钮能更新 `appController.mappingEnabled`。
- [ ] runtime 状态栏能反映 created/ready/running/error 基本状态。

## Manual Test Checklist

- [ ] 无手柄启动，设备面板显示空状态，应用不崩溃。
- [ ] 启动前连接 Xbox 风格手柄，设备面板显示真实设备。
- [ ] 运行中插入手柄，设备面板新增设备。
- [ ] 运行中拔出手柄，设备面板移除设备。
- [ ] 点击设备卡片后 Gamepad View 标题更新。
- [ ] Mapping On/Off 切换不会影响设备列表显示。
- [ ] 关闭应用后 runtime 停止，终端无明显析构/线程错误。

## Acceptance Criteria

- [ ] `ui/Main.qml` 不再使用静态 `devicesModel` 作为设备面板数据源。
- [ ] QML 可通过 `appController.deviceModel` 显示设备列表。
- [ ] 设备面板能显示 empty state 和至少一台真实/测试设备。
- [ ] QML 不直接访问 backend/runtime 内部对象。
- [ ] `cmake --build build` 通过。
- [ ] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [ ] `git diff --check` 通过。
- [ ] 新增和修改的文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 后续实现 `ZInputStateModel`，显示最近输入和控件状态。
- [ ] 后续实现 `ZLogModel` 或轻量 event log bridge。
- [ ] 后续实现 profile directory/active profile 管理。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
- [ ] 后续增加真实输出开关，允许 UI 从 NullOutput 切换到 Windows SendInput。
