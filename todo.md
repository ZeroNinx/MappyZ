# TODO: Runtime Profile JSON Module

## Next Step

下一步实现 `Runtime/ProfileManager.h/.cpp` 的最小版本，负责把磁盘上的 JSON profile 转换为现有 `SMappingProfile`，并支持把 `SMappingProfile` 序列化回 JSON。

优先做这个模块的理由：

- [x] `SMappingProfile`、`SMappingRule`、`SAction` 已经完成，具备稳定的数据契约。
- [x] `ZMappingSession` 已经可以消费 active profile，但现在还没有真实 profile 文件入口。
- [x] Profile JSON 是后续 UI 编辑、自动加载、用户配置持久化的共同基础。
- [x] 这个模块范围清晰，不依赖 SDL、Win32 或 QML，适合继续保持测试驱动。

## Scope

本轮只做 profile JSON 读写和校验，不做 profile 选择、热加载或 UI 绑定。

包含：

- [x] `source/Runtime/ProfileManager.h`
- [x] `source/Runtime/ProfileManager.cpp`
- [x] `tests/Runtime/ProfileManagerTests.cpp`
- [x] CMake target 和 CTest 接入
- [x] JSON 依赖接入

不做：

- [x] 不实现 profile 目录扫描。
- [x] 不实现设备到 profile 的自动匹配评分。
- [x] 不实现当前 active profile 管理。
- [x] 不修改 `ZMappingSession`。
- [x] 不实现 schema migration，只识别 `schema_version = 1`。
- [x] 不实现 UI model、QML bridge 或文件监听。
- [x] 不引入 Qt、SDL 或平台 API。

## Dependency Plan

- [x] 使用 `nlohmann_json` 作为本轮 JSON 库。
- [x] CMake 优先 `find_package(nlohmann_json CONFIG QUIET)`，便于 vcpkg 用户使用系统包。
- [x] 如果没有找到包，再用 `FetchContent` 拉取 `nlohmann/json` 的稳定 tag。
- [x] `ProfileManager.h` 不包含 JSON 头，JSON 细节只放在 `.cpp`。
- [x] `MappyZRuntime` 对 `nlohmann_json::nlohmann_json` 使用 `PRIVATE` 链接。

## Proposed API

- [x] 新增 `class ZProfileManager`，放在 Runtime 层。
- [x] `TResult<SMappingProfile> LoadProfile(const StdPath& ProfilePath) const`
- [x] `TResult<void> SaveProfile(const SMappingProfile& Profile, const StdPath& ProfilePath) const`
- [x] `TResult<SMappingProfile> ParseProfileJson(StdStringView JsonText) const`
- [x] `TResult<StdString> SerializeProfileJson(const SMappingProfile& Profile) const`

## JSON Contract

- [x] 顶层必须包含 `schema_version`，当前只接受 `1`。
- [x] `profile_id` 映射到 `SMappingProfile::Id`，可选，默认空字符串。
- [x] `profile_name` 映射到 `SMappingProfile::Name`，可选，默认空字符串。
- [x] `settings.enabled` 映射到 `SMappingProfile::bEnabled`，可选，默认 `true`。
- [x] `device_match.name/backend/vendor_id/product_id/guid/instance_id` 映射到 `SDeviceMatch`，字段均可选。
- [x] `mappings` 映射到 `SMappingProfile::Rules`，可选，默认空数组。
- [x] 每条 mapping 支持 `id`、`display_name`、`enabled`、`input`、`action`。

## Input Mapping Fields

- [x] `input.control_id` 映射到 `SMappingInput::ControlId`。
- [x] `input.control_type` 支持 `button`、`axis1d`、`axis2d`、`trigger`、`hat`。
- [x] `input.event` 支持 `pressed`、`released`、`changed`。
- [x] `input.threshold` 映射到 `SMappingInput::Threshold`，可选，默认使用结构体默认值。
- [x] `input.deadzone` 映射到 `SMappingInput::Deadzone`，可选，默认使用结构体默认值。
- [x] 未知 `control_type` 或 `event` 返回 `InvalidManifest`。

## Action Mapping Fields

- [x] `action.type` 支持 `keyboard_key`、`mouse_button`、`mouse_move`、`mouse_wheel`。
- [x] `action.mode` 支持 `press_release`、`hold`、`analog`。
- [x] `keyboard_key.key` 映射到 `SKeyboardAction::Key`。
- [x] `mouse_button.button` 第一版使用字符串：`left`、`right`、`middle`，分别映射到 `0`、`1`、`2`。
- [x] `mouse_move.sensitivity` 映射到 `SMappingOutput::Sensitivity`，可选，默认 `1.0`。
- [x] `mouse_wheel.delta` 映射到 `SMouseWheelAction::Delta`。
- [x] `mouse_move.curve` 如果出现，只接受 `linear`；其他值返回 `InvalidManifest`，避免静默忽略不支持的曲线。
- [x] 未知 `action.type` 或 `action.mode` 返回 `InvalidManifest`。

## Error Semantics

- [x] 文件不存在返回 `EErrorCode::FileNotFound`。
- [x] 文件读取失败返回 `EErrorCode::FileReadFailed`。
- [x] 文件写入失败返回 `EErrorCode::FileWriteFailed`。
- [x] JSON 语法错误返回 `EErrorCode::ParseFailed`。
- [x] schema 缺失、schema 不支持、字段类型错误或未知枚举返回 `EErrorCode::InvalidManifest`。
- [x] `SError::ContextPath` 在文件读写 API 中填写 profile 路径。
- [x] `ParseProfileJson()` 不知道磁盘路径时，`ContextPath` 保持为空。

## CMake Plan

- [x] 将 `source/Runtime/ProfileManager.cpp` 加入 `MappyZRuntime`。
- [x] `MappyZRuntime` 链接 `nlohmann_json::nlohmann_json`。
- [x] 新增 `tests/Runtime/ProfileManagerTests.cpp`，加入 `MappyZRuntimeTests`。
- [x] 不修改主应用 target。
- [x] 不让 Core target 依赖 JSON。

## Tests

- [x] 解析最小 profile：只有 `schema_version`。
- [x] 解析 Button -> Keyboard profile。
- [x] 解析 Button -> MouseButton profile。
- [x] 解析 Trigger threshold -> MouseButton profile。
- [x] 解析 Axis2D deadzone/sensitivity -> MouseMove profile。
- [x] 解析 disabled profile 和 disabled rule。
- [x] JSON 语法错误返回 `ParseFailed`。
- [x] 缺少 `schema_version` 返回 `InvalidManifest`。
- [x] 不支持的 schema version 返回 `InvalidManifest`。
- [x] 未知 control/action/mode 字符串返回 `InvalidManifest`。
- [x] `SerializeProfileJson()` 再 `ParseProfileJson()` 可 round-trip 一个典型 profile。
- [x] `LoadProfile()` 文件不存在返回 `FileNotFound`。
- [x] `SaveProfile()` 后 `LoadProfile()` 可 round-trip 临时文件。
- [x] 新增 Runtime 文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增和修改的文本文件使用 CRLF 行尾。
- [x] `ZProfileManager` 不依赖 UI、SDL 或平台层。
- [x] Core target 不引入 JSON 依赖。

## Follow-Up Module

- [ ] 后续实现 profile directory/active profile 管理，支持从配置目录选择 profile。
- [ ] 后续实现 `ZSdlInputBackend`，接入真实 SDL3 输入事件。
- [ ] 后续实现 `ZWindowsSendInputBackend`，把 `SAction` 转换为 Windows 键盘和鼠标输出。
