# TODO: Input Backend Interface Module

## Next Step

下一步实现 `Backends/Input` 接口层。这个模块定义输入后端的统一契约，并提供一个可测试的假输入后端，用来在不接入 SDL3、不启动真实输入线程、不碰 QML 的情况下验证设备枚举、设备连接/断开回调和输入事件回调。

优先做这个模块的理由：

- `Core Input Model` 已完成，`SDeviceInfo`、`SDeviceId`、`SInputEvent` 已经可以作为后端边界数据。
- Milestone 1 的 `ZSdlInputBackend` 需要先有稳定的 `IInputBackend` 接口。
- 直接接 SDL3 会同时引入第三方依赖、设备生命周期、事件轮询和线程模型；本轮先把接口和测试替身固定下来，风险更小。
- 假输入后端可以后续用于 Runtime、UI Bridge 和绑定流程测试，不依赖真实手柄。

## Scope

本轮只覆盖输入后端接口层和测试替身。

包含：

- [x] `source/Backends/Input/InputBackend.h`
- [x] `source/Backends/Input/FakeInputBackend.h`
- [x] `source/Backends/Input/FakeInputBackend.cpp`
- [x] `tests/Backends/Input/FakeInputBackendTests.cpp`
- [x] CMake target 和 CTest 接入

不做：

- [x] 不接入 SDL3。
- [x] 不实现 `ZSdlInputBackend`。
- [x] 不启动输入线程或事件轮询线程。
- [x] 不访问 Qt、QML、Win32 或 SDL 头文件。
- [x] 不实现 `ZDeviceManager`、`ZInputRuntime` 或 Runtime 队列。
- [x] 不把假输入后端暴露给最终用户 UI。

## Design Decisions

- [x] `IInputBackend` 放在 `source/Backends/Input/InputBackend.h`，只依赖 `Core/DeviceId.h`、`Core/InputEvent.h` 和标准库回调类型。
- [x] `IInputBackend::Start()` 返回 `TResult<void>`，用于表达后端初始化失败。
- [x] `IInputBackend::Stop()` 为幂等操作，多次调用不应失败。
- [x] `IInputBackend::ListDevices()` 返回当前后端已知的设备快照，不暴露内部容器引用。
- [x] 回调保留在接口层，后续 Runtime 通过这些回调订阅设备和输入事件。
- [x] 回调成员允许为空；后端触发回调前必须检查 `std::function` 是否有效。
- [x] `FakeInputBackend` 是测试替身，不是生产后端；它只用于单元测试和后续无硬件集成测试。

## Interface Contract

- [x] `IInputBackend` 定义虚析构函数。
- [x] `IInputBackend` 禁止拷贝和移动，避免后端生命周期、回调和线程所有权不清晰。
- [x] `IInputBackend` 提供：
  - `ZERO_NODISCARD virtual TResult<void> Start() = 0`
  - `virtual void Stop() = 0`
  - `ZERO_NODISCARD virtual bool IsRunning() const noexcept = 0`
  - `ZERO_NODISCARD virtual TVector<SDeviceInfo> ListDevices() const = 0`
- [x] `IInputBackend` 暴露以下回调成员：
  - `std::function<void(const SDeviceInfo& DeviceInfo)> OnDeviceConnected`
  - `std::function<void(const SDeviceId& DeviceId)> OnDeviceDisconnected`
  - `std::function<void(const SInputEvent& Event)> OnInputEvent`
- [x] 回调调用顺序由具体后端保证；本轮假后端应按调用者触发顺序同步调用。

## Fake Backend Behavior

- [x] `ZFakeInputBackend` 实现 `IInputBackend`。
- [x] `ZFakeInputBackend` 初始为 stopped 状态。
- [x] `Start()` 将状态切到 running；重复调用返回成功且不重复派发连接事件。
- [x] `Stop()` 将状态切到 stopped；重复调用安全无副作用。
- [x] `AddDevice(const SDeviceInfo& DeviceInfo)` 加入设备快照。
- [x] 如果 backend 正在 running，`AddDevice` 应同步触发 `OnDeviceConnected`。
- [x] 如果 `AddDevice` 收到已存在的 `SDeviceId`，不重复添加、不覆盖原设备、不触发连接回调。
- [x] 重复设备 ID 应记录 warning 级别调试日志；本轮可先通过 Qt logging category 或 ZeroStyle/标准错误日志中较轻的方式实现，避免引入正式日志系统。
- [x] `RemoveDevice(const SDeviceId& DeviceId)` 从设备快照移除设备。
- [x] 如果 backend 正在 running 且设备存在，`RemoveDevice` 应同步触发 `OnDeviceDisconnected`。
- [x] `EmitInput(const SInputEvent& Event)` 在 running 状态同步触发 `OnInputEvent`。
- [x] stopped 状态下 `EmitInput` 不触发回调。
- [x] 回调未设置时跳过调用，不抛异常、不崩溃；可输出 debug 级别日志说明事件没有订阅者。
- [x] `ListDevices()` 返回当前设备快照，调用方修改返回值不影响后端内部状态。
- [x] 本轮假后端不做线程安全承诺；线程安全留给真实后端或 Runtime 层设计。

## CMake Plan

- [x] 新建 `MappyZInputBackends` target。
  - 如果只有接口和一个 `.cpp`，使用 static library。
  - 链接 `MappyZCore`。
  - include 根继续通过 target 传递，不手动散落 include path。
- [x] 主应用本轮不强制链接 `MappyZInputBackends`，除非需要编译探针。
- [x] 新增 `MappyZInputBackendTests` 测试目标。
  - 链接 `MappyZCore`、`MappyZInputBackends`、`Catch2::Catch2WithMain`。
  - 通过 `catch_discover_tests(MappyZInputBackendTests)` 注册到 CTest。
- [x] 保持 Catch2 v3 获取策略不变：优先 vcpkg `find_package`，兜底 FetchContent。

## Tests

- [x] `ZFakeInputBackend` 默认 stopped，设备列表为空。
- [x] `Start()` / `Stop()` 状态切换正确，重复调用安全。
- [x] `AddDevice` 能更新 `ListDevices()`。
- [x] running 状态下 `AddDevice` 触发一次 `OnDeviceConnected`。
- [x] stopped 状态下 `AddDevice` 不触发连接回调。
- [x] 重复 `SDeviceId` 的 `AddDevice` 不改变设备数量、不覆盖原设备、不触发连接回调。
- [x] 未设置 `OnDeviceConnected` 时，running 状态下 `AddDevice` 不崩溃。
- [x] running 状态下 `RemoveDevice` 触发一次 `OnDeviceDisconnected`。
- [x] 移除不存在的设备不触发断开回调。
- [x] 未设置 `OnDeviceDisconnected` 时，running 状态下 `RemoveDevice` 不崩溃。
- [x] running 状态下 `EmitInput` 触发 `OnInputEvent`，事件内容保持不变。
- [x] stopped 状态下 `EmitInput` 不触发输入回调。
- [x] 未设置 `OnInputEvent` 时，running 状态下 `EmitInput` 不崩溃。
- [x] `ListDevices()` 返回快照，修改返回值不污染后端内部设备列表。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] 新增输入后端头文件不包含 Qt、QML、SDL 或 Win32 头。
- [x] `MappyZInputBackends` target 存在并链接 `MappyZCore`。
- [x] `MappyZInputBackendTests` target 存在并通过 CTest 运行。
- [x] `IInputBackend` 可以直接作为后续 `ZSdlInputBackend` 的实现接口。
- [x] 所有回调触发路径都先检查回调是否已设置。
- [x] 重复设备 ID 行为有测试覆盖。
- [x] `ZFakeInputBackend` 可以用于后续 Runtime 和 UI Bridge 的无硬件测试。
- [x] 提交前所有新增文本文件使用 CRLF 行尾。

## Follow-Up Module

- [x] 下一模块建议实现 `Runtime/DeviceManager` 的最小版本，消费 `IInputBackend` 的设备连接/断开回调并维护设备快照。
- [x] 再下一步实现 `ZSdlInputBackend`，把 SDL3 设备和事件转换成 `SDeviceInfo` 与 `SInputEvent`。
- [x] 在进入 `ZMappingEngine` 前补齐 `source/Core/MappingRule.h` 和 `source/Core/MappingProfile.h`，定义 `SMappingRule`、`SMappingProfile` 与最小匹配字段。
