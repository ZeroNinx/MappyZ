# TODO: Core Mapping Profile Contract Module

## Next Step

下一步实现 `Core/MappingRule.h` 和 `Core/MappingProfile.h` 的最小版本。这个模块只定义映射规则和 profile 快照的数据契约，为后续 `ZMappingEngine`、JSON profile loader、绑定 UI 和 `ZMappingSession` 提供稳定输入。

优先做这个模块的理由：

- `SInputEvent`、`SAction`、`ZInputRuntime` 已经存在，但中间缺少“输入如何匹配到动作”的数据结构。
- `ZMappingEngine` 不应该直接读 JSON 或 UI 状态，它应该只消费 `SMappingProfile` 快照。
- Profile schema、绑定 UI、保存加载和映射执行都会依赖同一组 Core 类型，先稳定契约可以减少后续破坏性修改。
- 这是纯 Core 模块，不依赖 Qt、SDL、Win32，适合用单元测试快速锁定默认值和字段语义。

## AntiMicroX Architecture Reference

AntiMicroX 的整体思路里，设备对象下挂多个 set，每个 set 内包含按钮、轴、摇杆、方向键等控件对象，控件对象再持有各自的映射 slot。这种结构的优点是功能表达直接，复杂绑定和 set 切换容易挂在控件对象上。

MappyZ 本轮只借鉴其中的架构意图，不照搬对象图：

- [x] 借鉴”一个设备 profile 包含多条控件映射规则”的模型。
- [x] 借鉴”规则需要稳定 ID，方便 UI 编辑、保存、冲突提示和日志定位”的做法。
- [x] 借鉴”输入匹配和输出动作配置分开表达”，便于后续支持不同输出后端。
- [x] 改进为纯数据 `SMappingProfile` 快照，不使用 QObject 控件树。
- [x] 改进为规则列表 + 独立 `ZMappingEngine`，不让控件对象直接执行映射。
- [x] 暂不实现 AntiMicroX 风格的 set/layer/mode shift；MVP 先做单 active profile，后续再加 layer 字段或 profile group。
- [x] 不复制 AntiMicroX 的代码、XML 格式、类结构、资源或文案。

## Scope

本轮只覆盖 Core 映射数据契约。

包含：

- [x] `source/Core/MappingRule.h`
- [x] `source/Core/MappingProfile.h`
- [x] `tests/Core/MappingProfileTests.cpp`
- [x] CMake 测试目标接入

不做：

- [x] 不实现 `ZMappingEngine`。
- [x] 不实现 JSON profile loader/saver。
- [x] 不实现 profile schema 迁移。
- [x] 不实现 runtime profile manager。
- [x] 不实现 UI 绑定编辑。
- [x] 不实现输出后端。
- [x] 不实现 layer、mode shift、宏、连发、长按短按或组合键。

## Design Decisions

- [x] `MappingRule.h` 和 `MappingProfile.h` 属于 Core 层，只能依赖 `ProjectCore.h`、`InputEvent.h`、`DeviceId.h`、`Action.h`。
- [x] Profile 是运行时可替换的不可变快照；类型本身保持普通可复制数据结构，是否不可变由 Runtime 持有策略保证。
- [x] 规则不保存平台枚举、SDL 枚举、Win32 virtual-key 或 scan-code。
- [x] 规则中的输入侧使用项目内部 `ControlId` 字符串和 `EInputControlType`。
- [x] 规则中的输出侧复用 `SAction`，平台转换留给 `IOutputBackend`。
- [x] `SchemaVersion` 默认值为 `1`。
- [x] `SMappingProfile` 默认启用，`SMappingRule` 默认启用。
- [x] 空 profile 合法，表示没有映射规则。

## Proposed Types

### Mapping Rule

- [x] 新增 `enum class EMappingActionMode`：
  - `PressRelease`：按钮按下生成 pressed 动作，抬起生成 released 动作。
  - `Hold`：输入保持激活时保持输出状态，后续引擎处理释放。
  - `Analog`：模拟量连续映射，例如摇杆到鼠标移动。
- [x] 新增 `struct SMappingInput`：
  - `StdString ControlId`
  - `EInputControlType ControlType = EInputControlType::Button`
  - `EInputEventType EventType = EInputEventType::Pressed`
  - `float32 Threshold = 0.5f`
  - `float32 Deadzone = 0.0f`
- [x] 新增 `struct SMappingOutput`：
  - `SAction Action`
  - `EMappingActionMode Mode = EMappingActionMode::PressRelease`
  - `float32 Sensitivity = 1.0f`
- [x] 新增 `struct SMappingRule`：
  - `StdString Id`
  - `StdString DisplayName`
  - `bool bEnabled = true`
  - `SMappingInput Input`
  - `SMappingOutput Output`

### Mapping Profile

- [x] 新增 `struct SDeviceMatch`：
  - `StdString Name`
  - `StdString Backend`
  - `StdString VendorId`
  - `StdString ProductId`
  - `StdString Guid`
  - `StdString InstanceId`
- [x] 新增 `struct SMappingProfile`：
  - `uint32 SchemaVersion = 1`
  - `StdString Id`
  - `StdString Name`
  - `bool bEnabled = true`
  - `SDeviceMatch DeviceMatch`
  - `TVector<SMappingRule> Rules`

## Field Semantics

- [x] `SMappingRule::Id` 在单个 profile 内应稳定且唯一，但本轮只定义字段，不实现唯一性校验。
- [x] `SMappingRule::DisplayName` 用于 UI 展示，可为空。
- [x] `SMappingInput::Threshold` 用于 Trigger 或 Axis1D 激活判断，默认 `0.5f`。
- [x] `SMappingInput::Deadzone` 用于 Axis1D 或 Axis2D，默认 `0.0f`，合法范围约定为 `[0.0f, 1.0f]`，本轮不做 clamp。
- [x] `SMappingOutput::Sensitivity` 用于 Axis2D 到 MouseMove 等模拟输出，默认 `1.0f`。
- [x] `SDeviceMatch` 字段都可为空；后续 profile 匹配模块按可用字段评分。
- [x] `SMappingProfile::bEnabled = false` 表示整个 profile 不参与映射。
- [x] `SMappingRule::bEnabled = false` 表示单条规则不参与映射。

## Header Layout

- [x] `MappingRule.h` 包含输入匹配和输出配置：
  - `EMappingActionMode`
  - `SMappingInput`
  - `SMappingOutput`
  - `SMappingRule`
- [x] `MappingProfile.h` 包含 profile 级数据：
  - include `MappingRule.h`
  - `SDeviceMatch`
  - `SMappingProfile`
- [x] 不新增 `.cpp` 文件；本轮全部为 header-only 数据结构。

## CMake Plan

- [x] 保持 `MappyZCore` 为 interface library。
- [x] 新增 `tests/Core/MappingProfileTests.cpp`，加入 `MappyZCoreTests`。
- [x] 不新增第三方依赖。
- [x] 不修改主应用 target。

## Tests

- [x] `SMappingInput` 默认值符合预期。
- [x] `SMappingOutput` 默认值符合预期。
- [x] `SMappingRule` 默认启用，默认输入和输出字段可用。
- [x] `SDeviceMatch` 默认构造为空。
- [x] `SMappingProfile` 默认 `SchemaVersion == 1` 且 `bEnabled == true`。
- [x] 空 `SMappingProfile::Rules` 合法。
- [x] 可创建 Button -> Keyboard rule。
- [x] 可创建 Button -> MouseButton rule。
- [x] 可创建 Axis2D -> MouseMove rule，包含 deadzone 和 sensitivity。
- [x] 可创建 Trigger threshold rule。
- [x] `MappingProfile.h` 可单独 include 编译。
- [x] Core 新头文件不包含 Qt、QML、SDL 或 Win32 头。

## Acceptance Criteria

- [x] `cmake --build build` 通过。
- [x] `ctest --test-dir build --output-on-failure -C Debug` 通过。
- [x] `git diff --check` 通过。
- [x] 新增文本文件使用 CRLF 行尾。
- [x] `source/Core/MappingRule.h` 和 `source/Core/MappingProfile.h` 不依赖 Runtime、Backend、UI 或平台层。
- [x] `MappyZCoreTests` 覆盖本轮新增默认值和典型规则构造。

## Follow-Up Module

- [ ] 下一模块建议实现 `Core/MappingEngine.h/.cpp`，消费 `SInputEvent + SMappingProfile` 并输出 `TVector<SAction>`。
- [ ] 再下一步实现 `Backends/Output/OutputBackend.h` 和 `ZNullOutputBackend`，让 Runtime 可以测试动作派发。
- [ ] 后续实现 JSON profile loader/saver，把外部 JSON schema 转换为本轮定义的 `SMappingProfile`。
