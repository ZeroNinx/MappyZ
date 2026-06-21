# TODO: Core Input Model Module

## Next Step

建议下一步实现 `Core Input Model` 模块。这个模块只定义项目内部输入、设备、控件和动作数据结构，不接入 Qt/QML、SDL 或 Win32。它是 Milestone 1 的 `SInputEvent` 和后续 Milestone 2 的 `ZMappingEngine` 的共同基础。

优先做这个模块的理由：

- 当前仓库已经能启动 UI，但还没有稳定的 C++ 数据契约。
- SDL3 输入后端必须先把原始事件转换成项目内部 `SInputEvent`。
- 映射引擎后续只应该依赖纯 Core 类型，不能依赖 SDL、Qt 或平台 API。
- 范围小，可独立编译验证，不需要先决定完整运行时线程模型。

## Scope

本轮只覆盖 `source/Core/` 的基础类型和最小构建接入。

不做：

- [ ] 不接入 SDL3。
- [ ] 不实现输入线程。
- [ ] 不实现 QML 实时输入显示。
- [ ] 不实现 profile JSON 读写。
- [ ] 不实现 `ZMappingEngine`。
- [ ] 不实现 Windows 输出。

目录取舍：

- [ ] `SDeviceInfo` 本轮放在 `source/Core/DeviceId.h`。这有意偏离 blueprint 中 `Runtime/DeviceInfo.h` 的目录草案，因为 `IInputBackend` 和后续假输入后端都需要返回 `TVector<SDeviceInfo>`；把它放 Core 可以避免 Backend 反向依赖 Runtime。

测试框架取舍：

- [ ] 本轮选择 Catch2 v3，不使用 GoogleTest。理由是 Catch2 v3 对初期 CMake 项目更轻，测试写法更短，不需要预编译库；后续如果项目规模扩大，再评估是否需要迁移。

## Implementation Plan

- [ ] 新建 `source/Core/ProjectCore.h`，提供项目基础类型别名和 `ZERO_NODISCARD` 兼容定义。
  - 直接在 `ProjectCore.h` 内用标准库别名落地，例如 `using StdString = std::string`、`template <typename T> using TVector = std::vector<T>`、`template <typename T> using TOptional = std::optional<T>`。
  - 不创建假的 `ZeroStyle.h` stub，也不引入虚假的 `Zero::` 命名空间。
  - 如果 ZeroStyle 基础库之后接入，再把别名切换到 Zero 命名空间。
  - 保持 Core 不依赖 Qt、QML、SDL、Win32。

- [ ] 新建 `source/Core/DeviceId.h`，定义设备标识和设备匹配所需的最小字段。
  - `SDeviceId`
  - `SDeviceInfo`
  - 字段优先覆盖 `Name`、`Backend`、`VendorId`、`ProductId`、`Guid`、`InstanceId`。
  - 先只做数据结构，不做匹配算法。

- [ ] 新建 `source/Core/ControlId.h`，集中定义标准控件命名约定。
  - 保留 `StdString ControlId` 的开放形式，避免过早枚举化导致 SDL 映射受限。
  - 增加常用控件常量，例如 `button_south`、`right_trigger`、`left_stick`。
  - 明确字符串使用 `snake_case`，与 profile 的 `control_id` 对齐。

- [ ] 新建 `source/Core/InputEvent.h`，实现 blueprint 中的输入事件契约。
  - `EInputControlType`
  - `EInputEventType`
  - `SAxis2DValue`
  - `SInputEvent`
  - `Timestamp` 使用 `std::chrono::steady_clock::time_point`。
  - `Value` 表达按钮、扳机和单轴值。
  - `Axis2D` 表达摇杆双轴值，包含 `X` 和 `Y` 两个 `float32`，避免后续接摇杆鼠标映射时修改 `SInputEvent` 契约。

- [ ] 新建 `source/Core/Action.h`，定义平台无关输出动作。
  - `EActionType`
  - `SKeyboardAction`
  - `SMouseButtonAction`
  - `SMouseMoveAction`
  - `SAction`
  - 不包含 Win32 virtual-key、scan-code 或 SDL 枚举。

- [ ] 更新 `CMakeLists.txt`，把 Core 头文件加入目标源列表。
  - 创建 `MappyZCore` interface library target。
  - 通过 `target_include_directories(MappyZCore INTERFACE source)` 暴露 Core include 根。
  - `MappyZ` 可执行目标通过 `target_link_libraries(MappyZ PRIVATE MappyZCore)` 使用 Core。
  - 先保持 Core 实现 header-only，降低本轮范围。
  - 保证 MSVC `/W4` 下可编译。

- [ ] 引入基础测试工程，满足 Milestone 0 的测试链路要求。
  - 使用 Catch2 v3。
  - 优先 `find_package(Catch2 3 CONFIG QUIET)`，兼容 vcpkg 安装的 Catch2。
  - 如果本地没有 vcpkg 包，再用 `FetchContent` 拉取 Catch2 v3 作为 fallback。
  - fallback 版本固定到一个明确 tag，例如 `v3.7.1`，避免每次配置拉到不同代码。
  - 新增 `MappyZCoreTests` 测试目标，链接 `MappyZCore`。
  - `MappyZCoreTests` 链接 `Catch2::Catch2WithMain`，不手写测试 main。
  - 通过 CTest 注册测试，保证 `ctest --test-dir build` 可运行。
  - 不把测试依赖暴露给主应用目标。

- [ ] 添加最小 Core 编译测试，验证 Core 头和基础默认值。
  - 包含 `ProjectCore.h`、`DeviceId.h`、`ControlId.h`、`InputEvent.h`、`Action.h`。
  - 构造一个按钮按下 `SInputEvent` 并检查默认字段。
  - 构造一个键盘 `SAction` 并检查类型和 payload。
  - 不在 `source/App/Main.cpp` 中加入临时探针，避免应用入口承担测试职责。

## Test Framework Plan

- [ ] 在根 `CMakeLists.txt` 增加 `include(CTest)`，只在 `BUILD_TESTING` 为 ON 时配置测试。
- [ ] 新建 `tests/Core/CoreTypesTests.cpp`，使用 Catch2 v3 的 `TEST_CASE` 和 `REQUIRE`。
- [ ] CMake 获取 Catch2 的优先级：
  - 首选：`find_package(Catch2 3 CONFIG QUIET)`，适配 vcpkg toolchain。
  - 兜底：`FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1)`。
- [ ] 测试目标结构：
  - `add_executable(MappyZCoreTests tests/Core/CoreTypesTests.cpp)`
  - `target_link_libraries(MappyZCoreTests PRIVATE MappyZCore Catch2::Catch2WithMain)`
  - `include(Catch)` 后调用 `catch_discover_tests(MappyZCoreTests)`。
- [ ] 默认开发命令：
  - 配置：`cmake -S . -B build -DBUILD_TESTING=ON`
  - 构建：`cmake --build build`
  - 测试：`ctest --test-dir build --output-on-failure`
- [ ] 如果离线或网络受限，要求开发者通过 vcpkg 安装 Catch2，而不是提交第三方源码到仓库。

## Acceptance Criteria

- [ ] `cmake --build build` 可以通过。
- [ ] `source/Core/` 下的类型命名遵守 ZeroStyle：`S` 数据结构、`E` 枚举、`T` 模板别名。
- [ ] Core 头文件不包含 Qt、QML、SDL 或 Win32 头。
- [ ] `ProjectCore.h` 不依赖不存在的 `ZeroStyle.h`，也不声明假的 `Zero::` 兼容命名空间。
- [ ] `SInputEvent` 能表达按钮按下/抬起、轴变化、扳机变化和方向键输入。
- [ ] `SInputEvent` 能直接表达摇杆 `Axis2D` 的 X/Y 双轴值。
- [ ] `SAction` 能表达键盘键、鼠标按钮和鼠标移动。
- [ ] `MappyZCore` interface library target 存在，主程序和测试都通过 target 链接使用它。
- [ ] Catch2 v3 可以通过 vcpkg `find_package` 或 FetchContent fallback 获取。
- [ ] 至少一个 Core 测试目标可通过 `ctest --test-dir build --output-on-failure` 运行。
- [ ] 后续 `IInputBackend` 可以只依赖 `SInputEvent` 和 `SDeviceInfo`。
- [ ] 后续 `ZMappingEngine` 可以只依赖 `SInputEvent`、`SAction` 和 profile 数据。

## Follow-Up Module

- [ ] 完成本模块后，下一模块建议实现 `Backends/Input` 的接口层：`IInputBackend` 和空实现/假实现。
- [ ] 在进入 `ZMappingEngine` 前补齐 `source/Core/MappingRule.h` 和 `source/Core/MappingProfile.h`，定义 `SMappingRule`、`SMappingProfile` 与最小匹配字段。
- [ ] 再下一步接入 `ZSdlInputBackend`，把 SDL3 设备和事件转换成 `SDeviceInfo` 与 `SInputEvent`。
