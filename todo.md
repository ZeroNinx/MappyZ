# TODO: SDL3 Input Backend Module

## Next Step

下一步实现 `source/Backends/Input/SdlInputBackend.h/.cpp` 的最小真实输入后端，负责把物理手柄转换为现有 `SDeviceInfo` 和 `SInputEvent`。

优先做这个模块的理由：

- [x] Core 层的 `SInputEvent`、`SDeviceInfo`、`SMappingProfile`、`ZMappingEngine` 已经具备稳定数据契约。
- [x] Runtime 层已经有 `ZDeviceManager`、`ZInputRuntime`、`ZMappingSession`、`ZActionDispatcher`，但目前只能消费 fake input。
- [x] `ZProfileManager` 已经完成 JSON profile 读写，后续需要真实设备信息验证 `device_match` 字段是否够用。
- [x] AntiMicroX 的关键经验是：SDL 输入采集应独立于 UI 和平台输出，先把设备生命周期和原始输入稳定地转成内部事件。
- [x] Windows 输出后端可以稍后接入；没有真实输入入口时，输出后端只能靠单元测试验证，无法形成可观察的硬件链路。

## AntiMicroX Reference Notes

只参考架构经验，不复制、不改写、不翻译 AntiMicroX 代码。

- [x] AntiMicroX 使用 SDL worker 负责设备枚举、事件读取、热插拔和刷新，说明输入采集需要清晰生命周期。
- [x] AntiMicroX 把设备身份拆成名称、GUID、vendor/product、unique id 等字段，MappyZ 的 `SDeviceInfo` 应在 SDL 后端里尽量填充这些字段。
- [x] AntiMicroX 的输入对象、Qt signal 和 UI 耦合较深，MappyZ 不应照搬；本项目继续使用 `IInputBackend` 回调和 Core 数据结构隔离后端。
- [x] AntiMicroX 的平台输出 handler 独立于 SDL 输入读取，MappyZ 应继续保持 `IInputBackend` 和 `IOutputBackend` 分离。

## Qt Boundary

Qt 只负责 UI 必要部分，不进入核心运行时。

- [x] Core 只包含纯数据、映射规则、映射引擎和通用错误类型，不使用 `QObject`、`QString`、`QVariant`、`QTimer`、`QThread` 或 Qt 容器。
- [x] Backends 只面向 `IInputBackend` / `IOutputBackend` 接口，不依赖 Qt；SDL 后端使用标准 C++ 线程和回调。
- [x] Runtime 只组合 Core 和 Backend，保持标准 C++ API；不直接暴露 Qt property、signal/slot 或 QML 类型。
- [x] UI Bridge 后续单独放在 `source/UI/Bridge`，可以使用 `QObject`、`QAbstractListModel`、queued connection 等 Qt 机制，把 Runtime 快照适配给 QML。
- [x] QML 只负责显示、交互和调用 UI Bridge，不直接读取 SDL、不直接发送键鼠、不直接解析 profile 文件。

## Scope

本轮只做 SDL3 输入后端，不做 UI 绑定、不做真实键鼠输出、不做 profile 自动选择。

包含：

- [x] `source/Backends/Input/SdlInputBackend.h`
- [x] `source/Backends/Input/SdlInputBackend.cpp`
- [x] `tests/Backends/Input/SdlInputBackendTests.cpp`
- [x] CMake 中 SDL3 可选依赖接入
- [x] CMake 中后端源文件和测试文件的条件编译
- [x] SDL-free helper 测试文件，用于覆盖不需要真实 SDL runtime 的转换逻辑
- [x] 设备枚举、热插拔、按钮事件、摇杆/扳机轴事件标准化
- [x] 后端生命周期：`Start()`、`Stop()`、`IsRunning()`、析构自动停止

不做：

- [ ] 不实现 Windows `SendInput` 输出。
- [ ] 不实现 UI/QML 设备面板或输入状态面板。
- [ ] 不实现 profile 目录扫描、自动匹配或 active profile 管理。
- [ ] 不实现 per-app profile。
- [ ] 不实现传感器、陀螺仪、触摸板、rumble、灯光或电量信息。
- [ ] 不实现 SDL joystick raw mode；第一版只使用 SDL Gamepad 标准布局。
- [ ] 不把 AntiMicroX 的类结构、XML 配置或 Qt signal 网络搬进本项目。
- [ ] 不让 SDL 后端、Core 或 Runtime 引入任何 Qt 类型；Qt 集成留给后续 UI Bridge。

## Dependency Plan

- [x] 新增 `option(MAPPYZ_ENABLE_SDL3_INPUT "Build SDL3 input backend" ON)`。
- [x] 使用 `find_package(SDL3 CONFIG QUIET)` 查找 vcpkg/系统 SDL3。
- [x] 找到 SDL3 时编译 `ZSdlInputBackend` 并链接 `SDL3::SDL3`。
- [x] 找不到 SDL3 时保持项目可配置、可编译，输出 CMake warning，并跳过 SDL 后端和对应测试。
- [x] 不在本轮用 `FetchContent` 拉 SDL3，避免把大型 native 依赖引入当前 bootstrap 流程。
- [x] `SdlInputBackend.h` 不包含 SDL 头；使用 Pimpl，SDL handle、设备 map、轴缓存、worker 状态全部藏在 `.cpp`。
- [x] `MappyZCore` 继续不依赖 SDL。
- [x] `MappyZRuntime` 继续只依赖 `IInputBackend`，不直接依赖 SDL。

## Proposed API

- [x] 新增 `class ZSdlInputBackend final : public IInputBackend`。
- [x] 头文件只持有 `TUniquePtr<SImpl> Impl`，`struct SImpl` 在 `.cpp` 中定义。
- [x] `TResult<void> Start() override`
- [x] `void Stop() override`
- [x] `bool IsRunning() const noexcept override`
- [x] `TVector<SDeviceInfo> ListDevices() const override`
- [x] 构造函数不做 SDL 初始化，`Start()` 才初始化输入子系统。
- [x] 析构函数调用 `Stop()`，确保线程和 SDL gamepad handle 被释放。
- [x] 析构函数在 `.cpp` 中定义，避免 incomplete pimpl type 影响头文件使用者。
- [x] 禁止拷贝和移动。

## Threading And Lifecycle

- [x] `Start()` 幂等：已运行时返回 `Ok()`。
- [x] `Stop()` 幂等：未运行时安全无副作用。
- [x] 使用 `std::jthread` 和 `std::stop_token` 管理输入轮询线程。
- [x] worker 使用 `SDL_WaitEventTimeout(..., 4~8ms)` 等待事件，避免 `SDL_PollEvent()` 紧循环烧 CPU。
- [x] 每次 wait 收到事件后，用 `SDL_PollEvent()` drain 当前队列，避免事件 burst 时逐个等待 timeout。
- [x] `Stop()` 返回前必须请求停止并等待 worker 退出。
- [x] 后端内部设备列表使用 mutex 保护，`ListDevices()` 返回快照拷贝。
- [x] 回调触发必须串行化，不允许同一后端并发调用多个回调。
- [x] 本轮明确回调线程语义：SDL 后端回调来自输入 worker；UI 集成前需要 Runtime/EventQueue 或 Qt queued connection 做线程切换。
- [x] 不让 `ZDeviceManager` 或 `ZInputRuntime` 在本轮承担 SDL 线程安全责任。
- [x] 不使用 `QThread`、`QTimer` 或 Qt event loop 驱动 SDL 后端；Qt 线程投递只允许出现在后续 UI Bridge。

## Device Contract

- [x] 只接受 SDL 识别为 Gamepad 的设备。
- [x] `Start()` 只初始化 SDL gamepad 输入相关子系统，例如 `SDL_INIT_GAMEPAD`；不初始化 `SDL_INIT_VIDEO`，窗口生命周期交给 Qt。
- [x] `Stop()` 对应释放所有 gamepad handle，并调用匹配的 SDL gamepad 子系统退出逻辑。
- [x] `SDeviceInfo::Backend` 固定为 `"sdl3"`。
- [x] `SDeviceInfo::Id.Value` 使用稳定会话内 ID，例如 `"sdl3:<instance_id>"`。
- [x] `SDeviceInfo::InstanceId` 保存 SDL joystick instance id 字符串。
- [x] `SDeviceInfo::Name` 保存 SDL 设备名，缺失时使用空字符串。
- [x] `SDeviceInfo::Guid` 保存 SDL GUID 字符串，缺失时使用空字符串。
- [x] `SDeviceInfo::VendorId` 和 `ProductId` 使用 4 位小写十六进制字符串，缺失时使用空字符串。
- [x] 热插拔新增设备时更新设备列表并触发 `OnDeviceConnected`。
- [x] 热插拔移除设备时关闭对应 handle、更新设备列表并触发 `OnDeviceDisconnected`。
- [x] 重复 added/remapped 事件不应重复插入同一个 instance id。
- [x] `SDL_EVENT_GAMEPAD_REMAPPED` 第一版只刷新该设备 metadata 或记录调试信息，不重开设备、不重复触发 connected。

## Event Mapping Contract

- [x] SDL gamepad button down 映射为 `EInputEventType::Pressed`，`Value = 1.0f`。
- [x] SDL gamepad button up 映射为 `EInputEventType::Released`，`Value = 0.0f`。
- [x] 面板按钮映射到 `button_south/east/west/north`。
- [x] Start/Back/Guide 映射到 `button_start/button_back/button_guide`。
- [x] 肩键映射到 `left_shoulder/right_shoulder`。
- [x] 摇杆按下映射到 `left_stick_button/right_stick_button`。
- [x] D-pad 四方向映射到 `dpad_up/down/left/right`。
- [x] SDL trigger axis 映射为 `EInputControlType::Trigger`，范围归一化到 `[0.0, 1.0]`。
- [x] SDL left/right stick axis 映射为 `EInputControlType::Axis2D`，`ControlId` 为 `left_stick/right_stick`。
- [x] 摇杆 X/Y 轴事件需要合并最近一次同摇杆另一轴状态后再发出完整 `SAxis2DValue`。
- [x] 摇杆值归一化到 `[-1.0, 1.0]`，不在后端应用 deadzone。
- [x] 不认识的 SDL button/axis 只记录调试信息，不产生 `SInputEvent`。
- [x] `Timestamp` 使用 `std::chrono::steady_clock::now()`，不把 SDL timestamp 暴露到 Core。

## Error Semantics

- [x] SDL 初始化失败时 `Start()` 返回 `TResult<void>::Err(...)`。
- [x] SDL gamepad subsystem 不可用时返回可诊断错误消息。
- [x] 单个设备打开失败不导致整个后端停止，但要跳过该设备并记录错误。
- [x] worker 运行中遇到单个事件解析失败时跳过该事件，不停止后端。
- [x] `Start()` 部分成功后如果 worker 创建失败，必须关闭已打开设备并释放 SDL 状态。
- [x] `Stop()` 不返回错误；关闭阶段只做 best-effort 清理。

## CMake Plan

- [x] 将 `source/Backends/Input/SdlInputBackend.cpp` 条件加入 `MappyZInputBackends`。
- [x] `MAPPYZ_ENABLE_SDL3_INPUT=OFF` 时完全跳过 `find_package(SDL3 ...)`，不输出 SDL 缺失 warning。
- [x] `MAPPYZ_ENABLE_SDL3_INPUT=ON` 且找到 SDL3 时编译 SDL 后端和 SDL 后端测试。
- [x] `MAPPYZ_ENABLE_SDL3_INPUT=ON` 但未找到 SDL3 时输出 warning，跳过 SDL 后端和 SDL 后端测试，其他 target 正常配置。
- [x] SDL3 可用时让 `MappyZInputBackends` `PRIVATE` 链接 `SDL3::SDL3`。
- [x] 新增 `MAPPYZ_HAS_SDL3_INPUT` 编译定义，供测试和后续 App bootstrap 判断。
- [x] 新增 `tests/Backends/Input/SdlInputBackendTests.cpp`，只在 SDL3 可用时加入 `MappyZInputBackendTests`。
- [x] 不修改 `MappyZCore` target。
- [x] 不修改主应用启动逻辑；App bootstrap 后续单独接入真实后端。

## Tests

- [x] SDL3 可用时，`ZSdlInputBackend` 默认状态为 stopped，设备列表为空。
- [x] `Start()` 后 `IsRunning()` 为 true。
- [x] 重复 `Start()` 返回成功且不启动第二个 worker。
- [x] `Stop()` 后 `IsRunning()` 为 false。
- [x] 重复 `Stop()` 安全。
- [x] 析构 running backend 不崩溃。
- [x] 无手柄环境下 `Start()` 成功，`ListDevices()` 返回空或当前系统设备快照。
- [x] `ListDevices()` 返回快照，不暴露内部容器引用。
- [x] 空回调时设备和输入事件处理不崩溃。
- [x] 将按钮枚举到 `ControlId`、axis 归一化、Axis2D 合并拆成 `.cpp` 内部 helper，优先用不启动 SDL runtime 的单元测试覆盖。
- [x] 按钮枚举到 `ControlId` 的转换覆盖 south/east/west/north、肩键、stick button、D-pad。
- [x] trigger 归一化覆盖最小值、中间值、最大值。
- [x] stick 归一化覆盖负向、中心、正向。
- [x] Axis2D 合并逻辑覆盖先 X 后 Y、先 Y 后 X。
- [x] `SDL_EVENT_GAMEPAD_REMAPPED` 不会产生重复 connected 事件。
- [x] 后端头文件不包含 Qt、QML、Win32 或 SDL 头。
- [x] Core/Runtime 现有测试不需要 SDL3 也能继续通过。

## Acceptance Criteria

- [x] 未安装 SDL3 时，`cmake -S . -B build` 可以完成配置并跳过 SDL 后端。
- [x] 安装 SDL3 时，`cmake -S . -B build -DMAPPYZ_ENABLE_SDL3_INPUT=ON` 能启用 SDL 后端。
- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZSdlInputBackend` 不依赖 UI、QML 或 Win32。
- [x] `ZSdlInputBackend`、`MappyZRuntime` 和 `MappyZCore` 不包含 Qt 头，也不在 public API 中出现 Qt 类型。
- [x] `MappyZCore` 不引入 SDL 依赖。
- [ ] 连接普通 Xbox 风格手柄时，可以通过后端回调观察设备连接和按钮/轴输入事件。

## Follow-Up Module

- [ ] 后续实现 Runtime/EventQueue 或 UI Bridge 的线程切换层，安全接收 SDL worker 回调。
- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现 `ZWindowsSendInputBackend`，把 `SAction` 转换为 Windows 键盘和鼠标输出。
- [ ] 后续实现基础 QML 输入状态面板，显示设备和最近输入事件。
