# TODO: Runtime Input State Module

## Next Step

下一步实现 `Runtime/InputRuntime` 的最小版本。这个模块消费 `IInputBackend::OnInputEvent`，维护最近输入事件和当前控件状态，为后续 `ZMappingEngine`、输入调试 UI 和热插拔清理提供稳定入口。

优先做这个模块的理由：

- `ZDeviceManager` 已经负责设备生命周期，输入事件需要一个独立 Runtime 归口。
- 参考 AntiMicro 系列项目的架构，输入采集后会先沉淀成设备/控件运行时状态，再驱动映射和输出。
- MappyZ 不应把 SDL 事件、QML 状态和映射执行混在一起；先补输入状态层可以保持 Core/Runtime/Backend 边界清晰。
- `ZFakeInputBackend` 已经能注入输入事件，适合先用测试锁定行为。

## Reference Architecture Notes

AntiMicro 系列项目的方案可以概括为：

- SDL 事件读取器负责初始化 SDL、轮询事件和刷新设备。
- 输入调度层把 SDL 事件分发到设备对象和控件对象。
- 设备模型包含手柄、按钮、轴、方向键、摇杆、传感器和多个可切换 set。
- 控件对象自身持有映射配置，并通过 Qt signal/slot 向上冒泡状态变化。
- 输出侧通过 event handler factory 选择平台后端，例如 SendInput、XTest 或 uinput。
- 配置通过 XML 读写和迁移模块落盘，另有自动 profile watcher 做应用切换。

MappyZ 可以借鉴：

- [x] 输入读取、设备生命周期、输入状态、映射执行、输出注入分层，不让 UI 直接消费底层事件。
- [x] 为输入状态保留当前快照，而不只是最近事件日志；后续断线、释放按键和调试 UI 都需要它。
- [x] 输出侧后续使用工厂或注册表选择平台后端，但接口保持 Core 类型，不暴露平台 API。
- [x] profile 匹配需要设备 GUID、厂商、产品、实例 ID 等字段，后续 SDL 后端接入时补齐。
- [x] 借鉴 fake backend / fake classes 的测试思路，优先用无硬件测试覆盖运行时行为。

MappyZ 应避免：

- [x] 不复制 AntiMicro 的大型 Qt QObject 对象图。
- [x] 不让控件对象直接拥有映射执行逻辑；MappyZ 后续用独立 `ZMappingEngine`。
- [x] 不把 GUI、配置读写、输入线程和输出平台后端耦合在同一层。

## Scope

本轮只覆盖 Runtime 输入状态。

包含：

- [x] `source/Runtime/InputRuntime.h`
- [x] `source/Runtime/InputRuntime.cpp`
- [x] `tests/Runtime/InputRuntimeTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不接入 SDL3。
- [x] 不实现 `ZMappingEngine`。
- [x] 不执行键盘、鼠标或虚拟手柄输出。
- [x] 不实现 profile 匹配或配置读写。
- [x] 不实现 QML/Qt model。
- [x] 不承诺线程安全；本轮沿用 fake backend 单线程测试假设。

## Design Decisions

- [x] `ZInputRuntime` 属于 Runtime 层，可以依赖 `IInputBackend` 和 Core 输入事件类型。
- [x] `ZInputRuntime` 不拥有输入后端；构造时接收 `IInputBackend&`。
- [x] `ZInputRuntime` 只订阅 `OnInputEvent`，不订阅设备连接/断开；设备生命周期仍归 `ZDeviceManager`。
- [x] `ZInputRuntime` 维护两个数据：
  - 最近输入事件列表，供调试 UI 和测试观察。
  - 当前控件状态快照，供后续映射引擎查询。
- [x] 最近事件列表设置固定容量，默认值为 256；实现中使用命名常量，超过容量时丢弃最旧事件。
- [x] 当前控件状态以 `(DeviceId, ControlId)` 为 key，保存最后一次标准化输入事件。
- [x] `Detach()` 清空自己设置的 `OnInputEvent`，避免 runtime 销毁后回调悬垂。
- [x] 本轮约定 runtime attach 期间不允许外部改写后端 `OnInputEvent`；后续 EventBus 或组合订阅器再支持多订阅者。

## Interface Contract

- [x] `ZInputRuntime` 禁止拷贝和移动。
- [x] 构造函数：`explicit ZInputRuntime(IInputBackend& Backend)`。
- [x] 提供：
  - `void Attach()`
  - `void Detach()`
  - `ZERO_NODISCARD bool IsAttached() const noexcept`
  - `ZERO_NODISCARD TVector<SInputEvent> ListRecentEvents() const`
  - `ZERO_NODISCARD uint32 GetRecentEventCount() const noexcept`
  - `ZERO_NODISCARD TOptional<SInputEvent> FindControlState(const SDeviceId& DeviceId, StdStringView ControlId) const`
  - `ZERO_NODISCARD uint32 GetTrackedControlCount() const noexcept`
  - `void Clear()`
- [x] `Attach()` 可重复调用；重复调用不应重复订阅或清空状态。
- [x] `Detach()` 可重复调用；重复调用安全无副作用。
- [x] `Detach()` 后再次 `Attach()` 只重新订阅输入回调，不清空已经积累的最近事件和当前控件状态。
- [x] `Clear()` 清空最近事件和当前控件状态，但不改变 attach 状态。
- [x] 析构函数应 best-effort 调用 `Detach()`。

## Input State Behavior

- [x] attached 后，fake backend `EmitInput` 会被 runtime 记录。
- [x] stopped backend 不会触发事件，runtime 状态不变化。
- [x] detached 后，fake backend `EmitInput` 不再影响 runtime。
- [x] 同一设备同一控件的多次事件只保留最后一次当前状态。
- [x] 不同设备的同名控件应分别保存。
- [x] `ListRecentEvents()` 返回快照拷贝，调用方修改返回值不影响 runtime 内部状态。
- [x] `FindControlState()` 找到时返回事件副本，找不到时返回空。
- [x] 最近事件超过容量后丢弃最旧事件，保留最新事件。

## CMake Plan

- [x] 将 `source/Runtime/InputRuntime.cpp` 加入 `MappyZRuntime`。
- [x] 将 `tests/Runtime/InputRuntimeTests.cpp` 加入 `MappyZRuntimeTests`。
- [x] 保持 `MappyZRuntime` 链接 `MappyZCore` 和 `MappyZInputBackends`。
- [x] 不新增第三方依赖。

## Tests

- [x] 构造后默认未 attached，事件数量和控件数量为 0。
- [x] `Attach()` 后输入事件会进入最近事件列表。
- [x] `Attach()` 重复调用不会清空已有状态。
- [x] 同一控件状态被后续事件覆盖。
- [x] 不同设备同名控件分别保存。
- [x] `ListRecentEvents()` 返回快照拷贝。
- [x] `FindControlState()` 对未知控件返回空。
- [x] `Clear()` 清空事件和状态，但不 detach。
- [x] `Detach()` 后输入事件不再影响 runtime。
- [x] `Detach()` 可重复调用且安全无副作用。
- [x] `Detach()` 后再次 `Attach()` 会恢复接收输入，并保留 re-attach 前已有状态。
- [x] runtime 析构后 fake backend 继续触发输入回调不崩溃。
- [x] 最近事件容量上限生效。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。
- [x] `ZInputRuntime` 能通过 `ZFakeInputBackend` 验证输入事件和当前状态。
- [x] `Detach()` 后不存在悬垂回调风险。
- [x] 提交前所有新增文本文件使用 CRLF 行尾。

## Follow-Up Module

- [ ] 下一模块建议实现 `Core/MappingRule.h` 和 `Core/MappingProfile.h`，为 `ZMappingEngine` 做数据契约。
- [ ] 再下一步实现 `ZMappingEngine` 最小版本，将输入状态变化转换成动作请求。
- [ ] 后续实现 `ZSdlInputBackend`，把 SDL3 设备和事件转换成 `SDeviceInfo` 与 `SInputEvent`。
