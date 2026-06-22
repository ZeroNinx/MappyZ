# TODO: Windows SendInput Output Backend Module

## Next Step

下一步实现 `source/Backends/Output/WindowsSendInputBackend.h/.cpp` 的最小真实输出后端，负责把现有 `SAction` 转换为 Windows 键盘和鼠标输入。

优先做这个模块的理由：

- [x] Core 层已有平台无关的 `SAction`、`SKeyboardAction`、`SMouseButtonAction`、`SMouseMoveAction`、`SMouseWheelAction`。
- [x] Runtime 层已有 `ZActionDispatcher`，可以把 `SAction` 发送给任意 `IOutputBackend`。
- [x] `ZNullOutputBackend` 已能验证派发链路，但不会产生真实系统输入。
- [x] SDL3 输入后端已完成，下一步应补齐 “输入 -> 映射 -> 输出” 的硬件闭环。
- [x] Windows 是 MVP 首发平台，`SendInput` 后端是 v0.1 可用性的关键模块。

## Architecture Boundary

本轮继续保持 Qt、Core、Runtime、平台 API 的边界清晰。

- [x] `MappyZCore` 不依赖 Win32，不包含 `Windows.h`。
- [x] `MappyZRuntime` 不直接依赖 Win32，只通过 `IOutputBackend` 派发 `SAction`。
- [x] `WindowsSendInputBackend.h` 不包含 `Windows.h`，public API 不出现 Win32 类型。
- [x] Win32 类型、`INPUT` 构造、virtual-key / mouse flag 映射全部放在 `.cpp` 或私有 helper 中。
- [x] UI/QML 不直接调用 `SendInput`，后续只通过 Runtime / UI Bridge 控制后端选择和状态展示。
- [x] 不修改 `SAction` 的平台无关语义，平台差异由输出后端吸收。

## Scope

本轮只做 Windows 键盘/鼠标真实输出后端，不做 UI 绑定、不做 profile 自动选择、不做虚拟手柄。

包含：

- [x] `source/Backends/Output/WindowsSendInputBackend.h`
- [x] `source/Backends/Output/WindowsSendInputBackend.cpp`
- [x] 可选的内部 helper，例如 `source/Backends/Output/WindowsSendInputHelpers.h`
- [x] `tests/Backends/Output/WindowsSendInputBackendTests.cpp`
- [x] `tests/Backends/Output/WindowsSendInputHelperTests.cpp`
- [x] CMake 中 Windows 输出后端的条件编译
- [x] 键盘按下 / 抬起输出
- [x] 鼠标左键 / 右键 / 中键按下和抬起输出
- [x] 相对鼠标移动输出
- [x] 鼠标滚轮输出
- [x] 后端状态查询：Ready / Unavailable / Error

不做：

- [ ] 不实现 UI/QML 后端切换面板。
- [ ] 不实现按下手柄输入进行绑定。
- [ ] 不实现 profile 目录扫描、自动匹配或 active profile 管理。
- [ ] 不实现虚拟 Xbox / DS4 手柄输出。
- [ ] 不实现原始物理手柄隐藏。
- [ ] 不实现低级键盘/鼠标 hook。
- [ ] 不实现管理员权限提升。
- [ ] 不实现 per-app profile。
- [ ] 不实现宏时间线、连发、组合键或 Layer / Mode Shift。
- [ ] 不让 Core、Runtime、SDL 后端或 UI Bridge 引入 Win32 类型。

## Dependency Plan

- [x] 新增 `option(MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT "Build Windows SendInput output backend" ON)`。
- [x] 仅在 `WIN32` 且 option 为 ON 时编译 `ZWindowsSendInputBackend`。
- [x] 非 Windows 平台即使 option 为 ON，也输出 CMake warning 并跳过该后端。
- [x] 新增 `MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT` 编译定义，供测试和后续 App bootstrap 判断。
- [x] `MappyZOutputBackends` 在 Windows 后端启用时条件加入源文件。
- [x] `MappyZOutputBackends` 不需要链接额外第三方库。
- [x] 不引入 Qt、SDL、nlohmann_json 或其他依赖。
- [x] 不在 public header 中包含 `Windows.h`，避免 `min/max` 宏和平台类型污染。

## Proposed API

- [x] 新增 `class ZWindowsSendInputBackend final : public IOutputBackend`。
- [x] `ZERO_NODISCARD TResult<void> SendAction(const SAction& Action) override`
- [x] `ZERO_NODISCARD SOutputBackendStatus GetStatus() const override`
- [x] 构造后默认状态为 `Ready`，除非当前平台/运行环境不可用。
- [x] 支持 `SetStatusForTesting()` 或测试 seam 时，保持在测试文件可控范围内，不污染 Runtime API。
- [x] 禁止拷贝和移动。
- [x] 如需缓存鼠标小数移动残差，使用私有 Pimpl 或私有成员，不暴露 Win32 类型。

## Key Mapping Contract

第一版只支持明确、可测试、跨 profile 稳定的 key name。

- [x] 单字符字母：`A` 到 `Z`，大小写输入统一映射到大写 virtual-key。
- [x] 单字符数字：`0` 到 `9`。
- [x] 常用功能键：`Space`、`Enter`、`Escape`、`Tab`、`Backspace`。
- [x] 方向键：`ArrowUp`、`ArrowDown`、`ArrowLeft`、`ArrowRight`。
- [x] 修饰键：`Shift`、`Control`、`Alt`。
- [x] 功能键：`F1` 到 `F12`。
- [x] 未知 key name 返回 `EErrorCode::InvalidArgument`，不静默忽略。
- [x] 不在本轮实现文本输入、Unicode 输入、键盘布局相关字符转换。
- [x] 不在本轮区分 left/right Shift、Control、Alt。

## Mouse Mapping Contract

- [x] `SMouseButtonAction::Button = 0` 映射为左键。
- [x] `SMouseButtonAction::Button = 1` 映射为右键。
- [x] `SMouseButtonAction::Button = 2` 映射为中键。
- [x] 其他按钮编号返回 `EErrorCode::InvalidArgument`。
- [x] `SMouseMoveAction` 使用相对移动，不使用绝对屏幕坐标。
- [x] `DeltaX` / `DeltaY` 转换为 `LONG dx/dy`，第一版可以 round 到整数。
- [x] 小于 1 像素的连续移动如需平滑，可在后端内部缓存小数残差。
- [x] `SMouseWheelAction::Delta` 映射到 `MOUSEEVENTF_WHEEL`，按 `WHEEL_DELTA` 缩放。
- [x] 滚轮 delta 为 0 时返回成功但不发送系统事件，或作为无效输入返回错误；实现前二选一并写入测试。

## SendInput Contract

- [x] 每次 `SendAction()` 最多发送一个逻辑动作对应的一组 `INPUT`。
- [x] Keyboard pressed 使用 key down，released 使用 key up。
- [x] Mouse button pressed 使用 button down flag，released 使用 button up flag。
- [x] Mouse move 使用 `MOUSEEVENTF_MOVE`。
- [x] Mouse wheel 使用 `MOUSEEVENTF_WHEEL`。
- [x] `SendInput()` 返回数量不等于请求数量时返回错误，并更新后端状态为 `Error`。
- [x] `SendInput()` 成功后保持状态为 `Ready`。
- [x] `EActionType::None` 返回 `EErrorCode::InvalidArgument`。
- [x] action type 与 payload 不匹配时返回 `EErrorCode::InvalidArgument`。
- [x] 不抛异常作为普通错误路径。

## Testability Plan

真实 `SendInput()` 不适合在单元测试中直接触发系统输入，本轮需要把转换逻辑和系统调用分开。

- [x] 将 key name -> virtual-key 的映射提取为纯 helper。
- [x] 将 mouse button -> down/up flag 的映射提取为纯 helper。
- [x] 将 `SAction` -> 内部 command / `INPUT` 描述的转换提取为可测试 helper。
- [x] 后端内部通过很薄的 native sender 调用 `SendInput()`。
- [x] 单元测试默认使用 fake sender，记录请求数量和参数，不产生真实键鼠输入。
- [x] 真实 `SendInput()` 只在手动测试或显式集成测试中执行。
- [x] 测试 helper 不需要 Qt、SDL 或 Runtime。

## Error Semantics

- [x] 不支持的平台返回 `Unavailable` 状态和可诊断消息。
- [x] 未知 key name 返回 `EErrorCode::InvalidArgument`。
- [x] 未知 mouse button 返回 `EErrorCode::InvalidArgument`。
- [x] action payload 类型不匹配返回 `EErrorCode::InvalidArgument`。
- [x] `SendInput()` 失败返回 `EErrorCode::Unknown` 或更合适的现有错误码，并带上 `GetLastError()` 信息。
- [x] 失败后 `GetStatus()` 返回 `Error`，Message 保存最近一次失败原因。
- [x] 后续成功发送可以把状态恢复为 `Ready`，或保留 Error 直到显式 Reset；实现前二选一并写入测试。

## CMake Plan

- [x] 将 `source/Backends/Output/WindowsSendInputBackend.cpp` 条件加入 `MappyZOutputBackends`。
- [x] 将 helper 测试和后端测试条件加入 `MappyZOutputBackendTests`。
- [x] `MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT=OFF` 时完全跳过 Windows 后端源文件和测试。
- [x] 非 Windows 平台不编译 Windows 后端源文件。
- [x] Windows 后端启用时定义 `MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT`。
- [x] 不修改 `MappyZCore` target。
- [x] 不修改 `MappyZRuntime` target 的公共接口。
- [x] 不修改主应用启动逻辑；App bootstrap 后续单独选择真实输出后端。

## Tests

- [x] `ZWindowsSendInputBackend` 默认状态为 `Ready`。
- [x] `EActionType::None` 返回错误。
- [x] payload 类型与 `EActionType` 不匹配返回错误。
- [x] `KeyboardKey` pressed 生成 key down。
- [x] `KeyboardKey` released 生成 key up。
- [x] 支持 `A-Z`、`0-9`、常用功能键、方向键、`F1-F12`。
- [x] 未知 key name 返回错误且不调用 native sender。
- [x] `MouseButton` pressed / released 生成对应 down/up flag。
- [x] 未知 mouse button 返回错误且不调用 native sender。
- [x] `MouseMove` 生成相对移动 command。
- [x] `MouseWheel` 生成滚轮 command。
- [x] fake sender 返回部分成功时，`SendAction()` 返回错误并更新状态。
- [x] fake sender 成功时，`SendAction()` 返回成功并保持 Ready。
- [x] 后端头文件不包含 Qt、QML、SDL、Win32 头。
- [x] Core/Runtime 现有测试不需要 Windows 后端也能继续通过。

## Manual Test Checklist

这些不作为自动单元测试，但实现完成后需要人工验证。

- [ ] 按钮映射到 `Space` 后，按下手柄按钮能在文本框中产生空格。
- [ ] 按钮映射到鼠标左键后，按下/抬起状态符合预期。
- [ ] 右摇杆映射到鼠标移动后，光标可以相对移动。
- [ ] 扳机阈值映射到鼠标按钮后，不会在阈值附近异常连点。
- [ ] 输出后端不可用或失败时，状态消息可被 Runtime/UI 后续读取。
- [ ] 程序退出后不会残留按键按下状态；如发现风险，后续需要补 pressed-state cleanup。

## Acceptance Criteria

- [x] Windows + option ON 时，`cmake -S . -B build -DMAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT=ON` 能启用后端。
- [x] Windows + option OFF 时，项目可配置、可编译、可测试，且不编译 Windows 后端。
- [x] 非 Windows 平台可以跳过该后端并继续配置。
- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZWindowsSendInputBackend` 不依赖 UI、QML、SDL。
- [x] `ZWindowsSendInputBackend.h` 不包含 `Windows.h`。
- [x] `MappyZCore` 和 `MappyZRuntime` 不引入 Win32 依赖。
- [ ] 连接普通 Xbox 风格手柄并使用测试 profile 时，可以观察到至少一种真实键盘或鼠标输出。

## Follow-Up Module

- [ ] 后续实现 App bootstrap，把 `ZSdlInputBackend`、`ZMappingSession`、`ZWindowsSendInputBackend` 串成真实运行链路。
- [ ] 后续实现 Runtime/EventQueue 或 UI Bridge 的线程切换层，安全接收 SDL worker 回调。
- [ ] 后续实现基础 QML 输入状态面板，显示设备和最近输入事件。
- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现绑定 UI：等待输入、选择输出动作、保存 profile。
