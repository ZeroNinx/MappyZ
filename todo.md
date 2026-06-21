# TODO: Runtime Device Manager Module

## Next Step

下一步实现 `Runtime/DeviceManager` 的最小版本。这个模块消费 `IInputBackend` 的设备连接/断开回调，维护运行时设备快照，并为后续 UI Bridge、输入运行时和热插拔流程提供稳定查询入口。

优先做这个模块的理由：

- `Core Input Model` 已完成，设备和输入事件数据契约已稳定。
- `Input Backend Interface` 已完成，`ZFakeInputBackend` 可作为无硬件测试替身。
- Milestone 1 要求启动后枚举手柄并支持热插拔；`ZDeviceManager` 是输入后端与 UI/Runtime 之间的第一层状态归口。
- 先做设备管理，不直接接 SDL3，可以用假后端把设备生命周期测试完整。

## Scope

本轮只覆盖 Runtime 设备管理。

包含：

- [x] `source/Runtime/DeviceManager.h`
- [x] `source/Runtime/DeviceManager.cpp`
- [x] `tests/Runtime/DeviceManagerTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不接入 SDL3。
- [x] 不实现 `ZInputRuntime`。
- [x] 不处理输入事件队列。
- [x] 不实现 QML/Qt model。
- [x] 不实现 profile 匹配或自动切换。
- [x] 不实现线程同步；本轮假定和 fake backend 一样在单线程测试。

## Design Decisions

- [x] `ZDeviceManager` 属于 Runtime 层，可以依赖 `IInputBackend` 和 Core 类型。
- [x] `ZDeviceManager` 不拥有输入后端；构造时接收 `IInputBackend&`，避免本轮处理后端生命周期。
- [x] `ZDeviceManager` 负责订阅后端回调，并维护自己的设备快照。
- [x] `ZDeviceManager` 启动时调用 `InputBackend.ListDevices()` 读取初始设备快照。
- [x] 热插拔通过 `OnDeviceConnected` 和 `OnDeviceDisconnected` 同步更新快照。
- [x] 本轮不把设备事件再广播给 UI；后续 UI Bridge 或 EventBus 模块再定义对外通知。
- [x] 本轮不做线程安全承诺；真实 SDL 后端接入前再明确线程边界和队列模型。

## Interface Contract

- [x] `ZDeviceManager` 禁止拷贝和移动。
- [x] 构造函数：`explicit ZDeviceManager(IInputBackend& Backend)`。
- [x] 提供：
  - `void Attach()`
  - `void Detach()`
  - `ZERO_NODISCARD bool IsAttached() const noexcept`
  - `ZERO_NODISCARD TVector<SDeviceInfo> ListDevices() const`
  - `ZERO_NODISCARD TOptional<SDeviceInfo> FindDevice(const SDeviceId& DeviceId) const`
  - `ZERO_NODISCARD uint32 GetDeviceCount() const noexcept`
- [x] `Attach()` 可重复调用；重复调用不应重复订阅、不应重复插入设备。
- [x] `Detach()` 可重复调用；重复调用安全无副作用。
- [x] `Detach()` 应清空后端回调，避免 manager 销毁后回调悬垂。
- [x] 析构函数应 best-effort 调用 `Detach()`。

## Device Snapshot Behavior

- [x] `Attach()` 后，manager 快照等于 `Backend.ListDevices()` 的返回值。
- [x] `OnDeviceConnected` 收到新设备时加入快照。
- [x] `OnDeviceConnected` 收到重复 `SDeviceId` 时不新增设备，可保留原设备信息并输出 warning 日志。
- [x] `OnDeviceDisconnected` 收到存在的 `SDeviceId` 时移除设备。
- [x] `OnDeviceDisconnected` 收到不存在的 `SDeviceId` 时忽略，可输出 debug/warning 日志。
- [x] `ListDevices()` 返回快照拷贝，调用方修改返回值不影响 manager 内部状态。
- [x] `FindDevice()` 找到时返回设备副本，找不到时返回空 `TOptional`。

## Callback Ownership

- [x] `Attach()` 设置后端的 `OnDeviceConnected` 和 `OnDeviceDisconnected`。
- [x] 如果后端已有回调，本轮可以覆盖，但必须在注释中明确此限制；后续 EventBus 或组合订阅器再解决多订阅者。
- [x] `Detach()` 只清空由 manager 设置的回调。
- [x] 为避免误清用户后来替换的回调，可用一个简单标记或约束说明：本轮约定 manager attach 期间不允许外部改写后端回调。
- [x] 本轮不订阅 `OnInputEvent`；输入事件归 `ZInputRuntime` 后续模块处理。

## CMake Plan

- [x] 新建 `MappyZRuntime` static library target。
  - 源文件包含 `source/Runtime/DeviceManager.cpp`。
  - 链接 `MappyZCore` 和 `MappyZInputBackends`。
  - include 根通过 target 传递，不散落 include path。
- [x] 主应用本轮不强制链接 `MappyZRuntime`，除非需要编译探针。
- [x] 新增 `MappyZRuntimeTests` 测试目标。
  - 链接 `MappyZRuntime`、`MappyZInputBackends`、`Catch2::Catch2WithMain`。
  - 通过 `catch_discover_tests(MappyZRuntimeTests)` 注册到 CTest。

## Tests

- [x] 构造后默认未 attached，设备数量为 0。
- [x] `Attach()` 读取 fake backend 已存在设备。
- [x] `Attach()` 重复调用不会重复设备。
- [x] attached 后 fake backend `AddDevice` 会更新 manager 快照。
- [x] attached 后 fake backend `RemoveDevice` 会更新 manager 快照。
- [x] 重复设备 ID 不会让 manager 快照出现重复项。
- [x] 断开不存在设备不会改变 manager 快照。
- [x] `FindDevice()` 可找到已连接设备。
- [x] `FindDevice()` 对未知设备返回空。
- [x] `ListDevices()` 返回快照拷贝，修改返回值不影响 manager。
- [x] `Detach()` 后 fake backend 设备变化不再影响 manager。
- [x] manager 析构后 fake backend 继续触发设备回调不崩溃。
- [x] 本轮不测试多线程行为。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。
- [x] `MappyZRuntime` target 存在并链接必要 target。
- [x] `MappyZRuntimeTests` target 存在并通过 CTest。
- [x] `ZDeviceManager` 能通过 `ZFakeInputBackend` 验证初始枚举和热插拔。
- [x] `Detach()` 后不存在悬垂回调风险。
- [x] 提交前所有新增文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 下一模块建议实现 `ZInputRuntime` 的最小版本，订阅 `IInputBackend::OnInputEvent` 并维护最近输入事件快照。
- [ ] 再下一步实现 `ZSdlInputBackend`，把 SDL3 设备和事件转换成 `SDeviceInfo` 与 `SInputEvent`。
- [ ] 在进入 `ZMappingEngine` 前补齐 `source/Core/MappingRule.h` 和 `source/Core/MappingProfile.h`，定义 `SMappingRule`、`SMappingProfile` 与最小匹配字段。