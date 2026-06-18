# MVP 技术草案：现代手柄映射软件

## 1. 项目定位

本项目是一个跨平台手柄映射软件，目标是将物理手柄输入转换为键盘、鼠标以及后续可扩展的动作输出。

MVP 阶段不追求完整覆盖成熟映射软件的所有功能，而是优先实现一条稳定、可观测、可扩展的输入重映射链路：

```text
Physical Gamepad
    ↓
Input Backend
    ↓
SInputEvent
    ↓
ZDeviceManager
    ↓
ZMappingSession
    ↓
ZMappingEngine
    ↓
SAction
    ↓
IOutputBackend
    ↓
Keyboard / Mouse / Future Virtual Device
```

项目长期目标不是单纯做一个“手柄映射 GUI”，而是实现一个现代化输入重映射运行时。UI 只是运行时的配置、观察和调试入口。

---

## 2. 技术栈

```text
语言：C++20 / C++23
输入层：SDL3
UI：Qt 6 + QML + Qt Quick
构建：CMake
配置格式：JSON
风格基础：ZeroStyle
日志：spdlog 或 Qt logging category
测试：GoogleTest / Catch2
包管理：vcpkg 或 Conan
格式化：clang-format
静态检查：clang-tidy
动态检查：ASan / UBSan / TSan
```

MVP 以 Windows 为首发平台。Linux 后端在接口层预留，但不作为 v0.1 的强制交付目标。

---

## 3. ZeroStyle 适配原则

项目 C++ 代码统一遵守 ZeroStyle。

### 3.1 命名原则

项目自有 C++ 代码使用 `PascalCase`。类型、函数、变量、成员变量都采用同一主视觉风格，并通过前缀表达语义类别。

| 前缀    | 用途                       | 示例                  |
| ----- | ------------------------ | ------------------- |
| `Z`   | 有行为、生命周期、不变量或资源管理的 class | `ZMappingEngine`    |
| `S`   | 数据、配置、描述符、事件、结果结构        | `SInputEvent`       |
| `T`   | 模板类型或模板 alias            | `TVector<SAction>`  |
| `I`   | 纯接口                      | `IInputBackend`     |
| `E`   | `enum class`             | `EInputControlType` |
| `C`   | concept                  | `CActionPayload`    |
| `b`   | 布尔变量                     | `bEnabled`          |
| `Out` | 输出参数                     | `OutDeviceInfo`     |

普通输入参数不使用 `In` 前缀。必须使用非返回值输出参数时，使用 `Out` 前缀。优先通过返回值、`TResult<T>` 或结构体表达输出。

### 3.2 Qt / QML 例外

C++ 层遵守 ZeroStyle。

QML 层允许遵守 Qt / QML 生态惯例：

```qml
property bool mappingEnabled: true
property string profileName: "Default"
signal bindingRequested(string controlId)
```

C++ 暴露给 QML 的类型仍使用 ZeroStyle：

```cpp
class ZDeviceModel;
class ZProfileModel;
class ZInputStateModel;
class ZAppController;
```

Qt 元对象系统需要的属性名可以使用 QML 更自然的 lowerCamelCase，但 C++ 内部成员与函数仍使用 `PascalCase`。

---

## 4. MVP 范围

### 4.1 v0.1 必须实现

1. 启动后枚举当前已连接手柄。
2. 支持手柄热插拔。
3. 实时显示按钮、摇杆、扳机和方向键状态。
4. 支持“按下手柄输入进行绑定”。
5. 支持按钮映射到键盘按键。
6. 支持按钮映射到鼠标点击。
7. 支持摇杆映射到鼠标移动。
8. 支持基础摇杆死区。
9. 支持 profile 保存和加载。
10. 支持 profile 手动切换。
11. 支持映射启停。
12. 支持基础事件日志。
13. 支持 Windows 键盘 / 鼠标输出。
14. 为 Linux 输出后端预留接口。

### 4.2 v0.1 暂不实现

1. 虚拟 Xbox / DS4 手柄输出。
2. 原始物理手柄隐藏。
3. 脚本系统。
4. 复杂宏时间线。
5. 插件系统。
6. 云同步。
7. Steam Input 兼容。
8. Wayland 特殊适配。
9. per-app 自动 profile 切换。
10. 多手柄复杂场景。
11. 驱动级输入拦截。

---

## 5. 工程目录

文件名使用 `PascalCase`。类型名前缀不强制同步到文件名；文件名按主要概念命名。

```text
source/
  App/
    Main.cpp
    ApplicationBootstrap.h
    ApplicationBootstrap.cpp

  Core/
    ProjectCore.h
    Types.h
    InputEvent.h
    DeviceId.h
    ControlId.h
    Action.h
    MappingRule.h
    MappingProfile.h
    MappingEngine.h
    AxisCurve.h
    Deadzone.h

  Runtime/
    DeviceManager.h
    DeviceManager.cpp
    EventBus.h
    EventBus.cpp
    InputRuntime.h
    InputRuntime.cpp
    MappingSession.h
    MappingSession.cpp
    ActionDispatcher.h
    ActionDispatcher.cpp
    ProfileManager.h
    ProfileManager.cpp
    LogSink.h
    LogSink.cpp

  Backends/
    Input/
      InputBackend.h
      SdlInputBackend.h
      SdlInputBackend.cpp

    Output/
      OutputBackend.h
      NullOutputBackend.h
      NullOutputBackend.cpp
      WindowsSendInputBackend.h
      WindowsSendInputBackend.cpp

  Platform/
    Windows/
      WindowsHandle.h
      WindowsHandle.cpp
      WindowsError.h
      WindowsError.cpp

  Ui/
    Bridge/
      AppController.h
      AppController.cpp
      DeviceModel.h
      DeviceModel.cpp
      ProfileModel.h
      ProfileModel.cpp
      InputStateModel.h
      InputStateModel.cpp
      LogModel.h
      LogModel.cpp

  Qml/
    Main.qml
    DevicePanel.qml
    GamepadView.qml
    BindingEditor.qml
    ProfilePanel.qml
    EventLogPanel.qml

Tests/
  Core/
  Runtime/
  Backends/
Docs/
  MvpTechDraft.md
  Architecture.md
  ProfileSchema.md
  AiCodingRules.md
```

---

## 6. 公共基础头

项目应建立一个公共基础头，例如：

```cpp
// source/Core/ProjectCore.h
#pragma once

#include "ZeroStyle.h"

namespace ZeroMapper {

using Zero::EErrorCode;
using Zero::SError;
using Zero::StdPath;
using Zero::StdString;
using Zero::StdStringView;
using Zero::TOptional;
using Zero::TResult;
using Zero::TSharedPtr;
using Zero::TUniquePtr;
using Zero::TVector;
using Zero::float32;
using Zero::int32;
using Zero::uint32;
using Zero::uint64;

}  // namespace ZeroMapper
```

公共头文件中禁止 `using namespace Zero;`。

业务头文件通过 `ProjectCore.h` 获取项目统一风格：

```cpp
#pragma once

#include "Core/ProjectCore.h"

namespace ZeroMapper {

struct SDeviceId
{
    StdString Value;
};

}  // namespace ZeroMapper
```

---

## 7. 核心架构原则

1. UI 不直接读取手柄。
2. UI 不直接发送键盘或鼠标事件。
3. Core 不依赖 Qt、QML、SDL、Win32。
4. SDL 事件必须先转换为项目内部 `SInputEvent`。
5. `ZMappingEngine` 只处理纯数据。
6. `IOutputBackend` 只接收 `SAction`，不理解 UI 状态。
7. 运行中的 profile 使用不可变快照。
8. 后端可替换。
9. 线程生命周期必须可控。
10. 所有平台资源必须 RAII 封装。
11. 可恢复错误使用 `TResult<T>`。
12. 查询为空使用 `TOptional<T>`。
13. 返回状态、错误、资源或查询结果的函数必须使用 `ZERO_NODISCARD`。

---

## 8. Core 模块

Core 是项目中最稳定、最纯净的部分。它不依赖 Qt、QML、SDL 或平台 API。

Core 包含：

1. 输入事件定义。
2. 设备标识。
3. 控件标识。
4. 动作定义。
5. 映射规则。
6. 映射 profile。
7. 死区计算。
8. 摇杆曲线。
9. `ZMappingEngine`。

### 8.1 输入事件

```cpp
#pragma once

#include "Core/ProjectCore.h"

#include <chrono>

namespace ZeroMapper {

enum class EInputControlType
{
    Button,
    Axis1D,
    Axis2D,
    Trigger,
    Hat,
};

enum class EInputEventType
{
    Pressed,
    Released,
    Changed,
};

struct SDeviceId
{
    StdString Value;
};

struct SInputEvent
{
    SDeviceId DeviceId;
    StdString ControlId;
    EInputControlType ControlType = EInputControlType::Button;
    EInputEventType EventType = EInputEventType::Changed;
    float32 Value = 0.0f;
    std::chrono::steady_clock::time_point Timestamp;
};

}  // namespace ZeroMapper
```

### 8.2 动作定义

```cpp
#pragma once

#include "Core/ProjectCore.h"

#include <variant>

namespace ZeroMapper {

enum class EActionType
{
    None,
    KeyboardKey,
    MouseButton,
    MouseMove,
    MouseWheel,
};

struct SKeyboardAction
{
    StdString Key;
    bool bPressed = false;
};

struct SMouseButtonAction
{
    int32 Button = 0;
    bool bPressed = false;
};

struct SMouseMoveAction
{
    float32 DeltaX = 0.0f;
    float32 DeltaY = 0.0f;
};

using TActionPayload = std::variant<SKeyboardAction, SMouseButtonAction, SMouseMoveAction>;

struct SAction
{
    EActionType Type = EActionType::None;
    TActionPayload Payload;
};

}  // namespace ZeroMapper
```

### 8.3 映射引擎

```cpp
#pragma once

#include "Core/Action.h"
#include "Core/InputEvent.h"
#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"

namespace ZeroMapper {

class ZMappingEngine
{
public:
    ZERO_NODISCARD TVector<SAction> MapInput(
        const SInputEvent& Event,
        const SMappingProfile& Profile) const;
};

}  // namespace ZeroMapper
```

`ZMappingEngine` 不允许：

1. 依赖 Qt。
2. 依赖 QML。
3. 依赖 SDL。
4. 调用 Win32 API。
5. 读取或写入 profile 文件。
6. 直接输出键鼠事件。
7. 访问 UI 状态。

---

## 9. Runtime 模块

Runtime 负责把 Core、Backend 和 UI Bridge 连接起来。

Runtime 包含：

1. `ZDeviceManager`
2. `ZInputRuntime`
3. `ZMappingSession`
4. `ZActionDispatcher`
5. `ZProfileManager`
6. `ZEventBus`
7. `ZLogSink`

### 9.1 设备管理

`ZDeviceManager` 负责设备生命周期。

职责：

1. 设备连接。
2. 设备断开。
3. 设备重连。
4. 设备状态缓存。
5. 设备标识生成。
6. 后端设备信息转换为 UI 可展示数据。

设备匹配不只依赖设备名称。profile 中应保存多种候选标识：

```json
{
  "name": "Xbox Wireless Controller",
  "vendor_id": "045e",
  "product_id": "02fd",
  "sdl_guid": "",
  "instance_id": "",
  "backend": "sdl3"
}
```

### 9.2 映射会话

`ZMappingSession` 是运行中的映射会话。

职责：

1. 持有当前 active profile snapshot。
2. 接收 `SInputEvent`。
3. 调用 `ZMappingEngine`。
4. 将 `SAction` 发送给 `ZActionDispatcher`。
5. 支持替换当前 profile。
6. 支持暂停和恢复映射。

运行时不直接修改正在被映射线程使用的 profile。UI 修改配置后，由 `ZProfileManager` 生成新的 `SMappingProfile`，再原子替换当前快照。

```text
UI Edit
    ↓
ZProfileManager
    ↓
New SMappingProfile Snapshot
    ↓
ZMappingSession.ReplaceProfile()
    ↓
Future Input Uses New Profile
```

### 9.3 动作派发

`ZActionDispatcher` 接收 `SAction` 并发送给当前 `IOutputBackend`。

MVP 阶段需要：

1. 键盘输出。
2. 鼠标按钮输出。
3. 鼠标移动输出。
4. Null output，用于测试和调试。

---

## 10. Backend 模块

Backend 是项目中最接近第三方库和平台 API 的部分。

### 10.1 输入后端接口

```cpp
#pragma once

#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"
#include "Runtime/DeviceInfo.h"

#include <functional>

namespace ZeroMapper {

class IInputBackend
{
public:
    virtual ~IInputBackend() = default;

    ZERO_NODISCARD virtual TResult<void> Start() = 0;
    virtual void Stop() = 0;

    ZERO_NODISCARD virtual TVector<SDeviceInfo> ListDevices() const = 0;

public:
    std::function<void(const SInputEvent& Event)> OnInputEvent;
    std::function<void(const SDeviceInfo& DeviceInfo)> OnDeviceConnected;
    std::function<void(const SDeviceId& DeviceId)> OnDeviceDisconnected;
};

}  // namespace ZeroMapper
```

MVP 输入后端：

```text
ZSdlInputBackend
```

职责：

1. 初始化 SDL3。
2. 枚举 gamepad。
3. 监听 SDL gamepad 事件。
4. 处理热插拔。
5. 将 SDL 事件转换为 `SInputEvent`。
6. 将 SDL 设备信息转换为 `SDeviceInfo`。
7. 不直接调用 `ZMappingEngine`。
8. 不直接访问 QML。

### 10.2 输出后端接口

```cpp
#pragma once

#include "Core/Action.h"
#include "Core/ProjectCore.h"

namespace ZeroMapper {

enum class EOutputBackendState
{
    Unavailable,
    Ready,
    Error,
};

struct SOutputBackendStatus
{
    EOutputBackendState State = EOutputBackendState::Unavailable;
    StdString Message;
};

class IOutputBackend
{
public:
    virtual ~IOutputBackend() = default;

    ZERO_NODISCARD virtual TResult<void> SendAction(const SAction& Action) = 0;
    ZERO_NODISCARD virtual SOutputBackendStatus GetStatus() const = 0;
};

}  // namespace ZeroMapper
```

MVP 输出后端：

```text
ZWindowsSendInputBackend
ZNullOutputBackend
```

后续扩展：

```text
ZLinuxUInputBackend
ZVirtualGamepadBackend
ZScriptOutputBackend
```

---

## 11. UI 设计

MVP UI 服务于三个核心任务：

1. 看见设备。
2. 看见输入。
3. 快速绑定动作。

### 11.1 主界面布局

```text
┌───────────────────────────────────────────────┐
│ Top Bar: 当前 Profile / 启停映射 / 设置        │
├───────────────┬─────────────────┬─────────────┤
│ Device Panel  │ Gamepad View     │ Binding     │
│               │                 │ Editor      │
├───────────────┴─────────────────┴─────────────┤
│ Event Log / Runtime Diagnostics                │
└───────────────────────────────────────────────┘
```

### 11.2 Device Panel

显示：

1. 已连接手柄列表。
2. 设备名称。
3. 后端。
4. GUID。
5. 连接状态。
6. 当前使用 profile。
7. 最近输入时间。

### 11.3 Gamepad View

显示：

1. 按钮状态。
2. 摇杆状态。
3. 扳机值。
4. D-pad 状态。
5. 当前选中控件。
6. 输入高亮。

MVP 可以先使用抽象布局，不必绘制真实手柄外观。

### 11.4 Binding Editor

支持：

1. 点击“等待输入”。
2. 用户按下手柄按钮、扳机或摇杆方向。
3. 自动识别 control id。
4. 选择输出类型。
5. 选择键盘按键或鼠标动作。
6. 保存规则。
7. 显示冲突提示。

MVP 绑定类型：

```text
Button  → Keyboard Key
Button  → Mouse Button
Axis2D  → Mouse Move
Trigger → Keyboard Key / Mouse Button
```

### 11.5 Event Log Panel

显示最近事件：

1. 设备连接 / 断开。
2. 原始输入事件。
3. 标准化 `SInputEvent`。
4. 命中的 `SMappingRule`。
5. 输出的 `SAction`。
6. 输出后端结果。
7. profile 加载错误。

---

## 12. Profile 格式

MVP 使用 JSON。JSON 字段可以采用 `snake_case`，因为这是面向用户和配置文件的外部格式，不强制套用 C++ 命名风格。

### 12.1 示例

```json
{
  "schema_version": 1,
  "profile_name": "Default Profile",
  "device_match": {
    "name": "Xbox Wireless Controller",
    "vendor_id": "045e",
    "product_id": "02fd",
    "sdl_guid": "",
    "backend": "sdl3"
  },
  "settings": {
    "enabled": true
  },
  "mappings": [
    {
      "id": "rule_a_to_space",
      "input": {
        "control_id": "button_south",
        "control_type": "button",
        "event": "pressed"
      },
      "action": {
        "type": "keyboard_key",
        "key": "Space",
        "mode": "press_release"
      }
    },
    {
      "id": "rule_rt_to_left_mouse",
      "input": {
        "control_id": "right_trigger",
        "control_type": "trigger",
        "threshold": 0.5
      },
      "action": {
        "type": "mouse_button",
        "button": "left",
        "mode": "press_release"
      }
    },
    {
      "id": "rule_right_stick_to_mouse",
      "input": {
        "control_id": "right_stick",
        "control_type": "axis2d"
      },
      "action": {
        "type": "mouse_move",
        "sensitivity": 1.0,
        "deadzone": 0.15,
        "curve": "linear"
      }
    }
  ]
}
```

### 12.2 Profile 设计原则

1. 必须带 `schema_version`。
2. 必须支持未来迁移。
3. 不直接保存后端内部枚举值。
4. `control_id` 使用项目内部标准命名。
5. `action` 使用平台无关描述。
6. 平台相关转换在 `IOutputBackend` 内完成。
7. profile 解析失败必须返回 `TResult<SMappingProfile>`。
8. profile 查询为空可使用 `TOptional<SMappingRule>`。

---

## 13. 错误处理

项目不使用异常作为常规错误处理路径。

### 13.1 使用 TResult

可能失败且需要错误信息的函数返回 `TResult<T>`：

```cpp
ZERO_NODISCARD TResult<SMappingProfile> LoadProfile(const StdPath& ProfilePath);
ZERO_NODISCARD TResult<void> SaveProfile(const SMappingProfile& Profile, const StdPath& ProfilePath);
ZERO_NODISCARD TResult<void> Start();
ZERO_NODISCARD TResult<void> SendAction(const SAction& Action);
```

调用方必须检查结果：

```cpp
auto ProfileResult = ProfileManager.LoadProfile(ProfilePath);

if (ProfileResult.IsErr())
{
    LogError(ProfileResult.Failure().Message);
    return;
}

SMappingProfile Profile = std::move(ProfileResult).TakeValue();
```

### 13.2 使用 TOptional

查询结果只有“有或没有”，且没有错误原因时使用 `TOptional<T>`：

```cpp
ZERO_NODISCARD TOptional<SMappingRule> FindRuleByControlId(StdStringView ControlId) const;
ZERO_NODISCARD TOptional<SDeviceInfo> FindDevice(const SDeviceId& DeviceId) const;
```

### 13.3 不忽略错误

禁止无理由忽略 `TResult<T>`。

可以接受：

```cpp
auto SaveResult = ProfileManager.SaveProfile(Profile, ProfilePath);

if (SaveResult.IsErr())
{
    LogError(SaveResult.Failure().Message);
    return SaveResult;
}
```

只有非关键 best-effort 行为可以显式忽略：

```cpp
(void)LogSink.Flush();  // Best-effort shutdown logging.
```

---

## 14. 资源管理与所有权

### 14.1 所有权规则

允许：

1. `TUniquePtr<T>` 表达独占所有权。
2. `TSharedPtr<T>` 表达真实共享所有权。
3. `std::weak_ptr` 或对应 alias 表达弱引用。
4. 引用表达非空非拥有关系。
5. 裸指针表达可空非拥有关系。
6. RAII wrapper 管理平台资源。

禁止：

1. owning raw pointer。
2. 裸 `new` / `delete`。
3. 业务代码直接持有未封装平台句柄。
4. 全局 mutable singleton。
5. 无生命周期说明的跨线程回调捕获。

### 14.2 平台资源 RAII

SDL、Win32、文件句柄、线程、回调注册等资源必须有明确的生命周期对象。

```cpp
class ZWindowsHandle final
{
public:
    ZWindowsHandle() = default;
    explicit ZWindowsHandle(void* NativeHandle);
    ~ZWindowsHandle();

    ZWindowsHandle(const ZWindowsHandle&) = delete;
    ZWindowsHandle& operator=(const ZWindowsHandle&) = delete;

    ZWindowsHandle(ZWindowsHandle&& Other) noexcept;
    ZWindowsHandle& operator=(ZWindowsHandle&& Other) noexcept;

    ZERO_NODISCARD bool IsValid() const noexcept;

private:
    void* Handle = nullptr;
};
```

---

## 15. 线程模型

### 15.1 推荐线程划分

```text
UI Thread
  - Qt event loop
  - QML rendering
  - 用户交互

Input Thread
  - SDL event polling
  - 设备热插拔
  - 输入事件标准化

Runtime Queue
  - MappingEngine
  - Action dispatch

Output
  - MVP 可同步发送
  - 后续可扩展为独立输出队列
```

MVP 阶段可以简化，但必须保证：

1. UI 线程不阻塞输入读取。
2. 输入线程不直接访问 QML 对象。
3. 跨线程通信使用队列或 Qt queued connection。
4. 所有线程必须支持 stop。
5. 退出时必须 clean shutdown。

### 15.2 C++ 线程规则

优先使用：

```cpp
std::jthread
std::stop_token
std::mutex
std::scoped_lock
std::condition_variable
```

禁止：

1. 无停止机制的裸线程。
2. 分离线程后不管理生命周期。
3. 回调捕获悬垂 `this`。
4. 跨线程直接调用 QML 对象。
5. 在析构函数里做不可控阻塞。

---

## 16. 日志与诊断

MVP 必须内置诊断能力。

### 16.1 日志分类

```text
Device
Input
Mapping
Output
Profile
Ui
System
```

### 16.2 日志等级

```text
Trace
Debug
Info
Warning
Error
Critical
```

### 16.3 必须记录的事件

1. 应用启动。
2. SDL 初始化成功或失败。
3. 设备连接。
4. 设备断开。
5. 设备重连。
6. profile 加载成功。
7. profile 加载失败。
8. 绑定规则创建。
9. 映射命中。
10. 输出动作发送成功或失败。
11. 输出后端不可用。
12. 应用退出。

### 16.4 调试面板

MVP UI 应包含基础调试面板，显示：

1. 当前设备数量。
2. 当前 active profile。
3. 最近 100 条输入事件。
4. 最近 100 条输出动作。
5. 后端状态。
6. 错误列表。

---

## 17. 格式化与编译规则

项目采用 ZeroStyle 风格的 clang-format 配置。

建议 `.clang-format`：

```yaml
BasedOnStyle: Chromium
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
PointerAlignment: Left
AccessModifierOffset: -4
NamespaceIndentation: None
SortIncludes: true
```

编译警告建议：

```text
GCC / Clang:
  -Wall
  -Wextra
  -Wconversion
  -Wshadow
  -Wnon-virtual-dtor
  -Wold-style-cast

MSVC:
  /W4
```

核心模块和 CI 可启用 warnings as errors。

---

## 18. C++ 代码规则

### 18.1 类型设计

1. 纯数据结构使用 `struct S...`。
2. 有行为、不变量、生命周期或资源管理的类型使用 `class Z...`。
3. 纯接口使用 `class I...`。
4. 枚举使用 `enum class E...`。
5. 模板参数使用 `TValue`、`TKey`、`TAction` 等语义化名称。
6. 单参数构造函数默认 `explicit`。
7. 拥有资源或复杂状态的类显式声明拷贝 / 移动策略。
8. `final` 只用于明确禁止继承的边界，不作为默认样板。

### 18.2 函数设计

1. 函数名使用 `PascalCase`。
2. 函数名应表达行为。
3. 谨慎使用 `Get`、`Set`、`Process`、`Handle`、`Execute` 等泛化名称。
4. 不修改对象状态的成员函数标记 `const`。
5. 保证不抛异常的轻量函数可标记 `noexcept`。
6. 返回错误、状态、资源、查询结果的函数必须使用 `ZERO_NODISCARD`。
7. 超过约 50 行的函数考虑拆分。
8. 超过约 100 行的函数通常需要重构。

### 18.3 变量设计

1. 局部变量使用 `PascalCase`。
2. 成员变量使用 `PascalCase`，不使用 `m_` 或尾随下划线。
3. 布尔变量使用 `b` 前缀。
4. 常量使用 `PascalCase`。
5. `ALL_CAPS` 仅用于宏。
6. `auto` 可用于类型明显或冗长的场景，不用于隐藏重要类型。

---

## 19. MVP 里程碑

### Milestone 0：工程骨架

目标：项目可以编译和启动。

交付：

1. CMake 工程。
2. ZeroStyle 接入。
3. Qt QML 空窗口。
4. `source/` 目录结构。
5. `.clang-format`。
6. README。
7. 基础测试工程。
8. 基础 CI。

### Milestone 1：SDL3 输入读取

目标：可以识别手柄并显示输入事件。

交付：

1. `ZSdlInputBackend`。
2. SDL 初始化。
3. 设备枚举。
4. 热插拔。
5. 按钮事件。
6. 轴事件。
7. `SDeviceInfo`。
8. `SInputEvent`。
9. QML 输入事件显示。

### Milestone 2：Profile 与 MappingEngine

目标：可以从配置文件加载映射并生成 `SAction`。

交付：

1. `SMappingProfile`。
2. `SMappingRule`。
3. JSON profile loader。
4. `ZMappingEngine`。
5. Button → Keyboard Action。
6. Button → Mouse Button Action。
7. Axis2D → Mouse Move Action。
8. Core 单元测试。

### Milestone 3：Windows 输出后端

目标：可以实际输出键盘和鼠标动作。

交付：

1. `ZWindowsSendInputBackend`。
2. 键盘按下 / 抬起。
3. 鼠标点击。
4. 鼠标移动。
5. 输出失败日志。
6. `ZNullOutputBackend` 测试。

### Milestone 4：绑定 UI

目标：用户可以通过 UI 创建映射规则。

交付：

1. 等待输入模式。
2. 自动捕获下一个手柄输入。
3. 选择输出动作。
4. 保存到 profile。
5. 应用新 profile snapshot。
6. 冲突提示。

### Milestone 5：MVP 收尾

目标：形成可用 v0.1。

交付：

1. profile 保存 / 加载。
2. 应用设置。
3. 错误提示。
4. 事件日志面板。
5. Windows 基础打包。
6. MVP 文档。
7. 手动测试清单。

---

## 20. 测试策略

### 20.1 Core 单元测试

必须测试：

1. 按钮按下映射。
2. 按钮抬起映射。
3. trigger threshold。
4. axis deadzone。
5. profile 解析。
6. profile schema version。
7. 无匹配规则时不输出 action。
8. 多规则匹配。
9. 冲突检测。

### 20.2 Runtime 测试

必须测试：

1. profile snapshot 替换。
2. 设备连接。
3. 设备断开。
4. 映射启停。
5. `ZNullOutputBackend` 输出记录。
6. 日志写入。
7. `TResult` 错误路径。

### 20.3 手动测试清单

每次 v0.1 release 前验证：

1. 启动时无手柄。
2. 启动前已连接手柄。
3. 运行中插入手柄。
4. 运行中拔出手柄。
5. 蓝牙手柄重连。
6. 同一个按钮绑定键盘。
7. 同一个按钮绑定鼠标。
8. 摇杆控制鼠标。
9. 保存 profile。
10. 重启后加载 profile。
11. profile 解析失败时 UI 有提示。
12. 输出后端不可用时 UI 有提示。
13. 退出时无线程残留。
14. 退出时无崩溃。

---

## 21. AI 协作规则

本项目允许大量使用 AI 生成代码，但必须遵守接口先行和高频 review。

### 21.1 AI 可以负责

1. 样板代码。
2. CMake 配置。
3. QML 页面草稿。
4. JSON 序列化。
5. 单元测试。
6. 文档。
7. 小型重构。
8. 平台 API 封装初稿。

### 21.2 AI 不应直接决定

1. 模块边界。
2. 线程模型。
3. 所有权模型。
4. profile schema。
5. 后端接口。
6. release 范围。
7. 安全策略。
8. GPL 相关代码复用。

### 21.3 AI 生成代码前必须明确

每个任务发给 AI 前，应明确：

1. 文件路径。
2. 模块职责。
3. 允许依赖。
4. 禁止依赖。
5. 输入输出契约。
6. 错误处理方式。
7. 是否需要测试。
8. 是否允许修改公共接口。
9. 必须遵守 ZeroStyle。

示例任务：

```text
任务：实现 source/Core/MappingEngine.cpp

约束：
- 遵守 ZeroStyle
- 不允许依赖 Qt
- 不允许依赖 QML
- 不允许依赖 SDL
- 不允许依赖 Win32 API
- 输入为 SInputEvent + SMappingProfile
- 输出为 TVector<SAction>
- 失败路径使用 TResult 或空 action 列表，不使用异常作为普通控制流
- 不允许裸 new/delete
- 不允许 owning raw pointer
- 必须补充 Core 单元测试
- 不允许修改公开接口，除非先说明理由
```

---

## 22. 合规与代码隔离

本项目不复制、不改写、不翻译 AntiMicroX 源码。

允许参考：

1. 公开功能列表。
2. 用户体验问题。
3. 用户反馈。
4. 自己总结出的需求。
5. 官方 API 文档。

禁止：

1. 复制 AntiMicroX 代码。
2. 照着 AntiMicroX 类结构重写。
3. 翻译 AntiMicroX 代码到新架构。
4. 复制配置格式实现。
5. 复制图标、文案、资源。
6. fork 后闭源发布。

项目开发过程中应保留独立设计文档和提交记录。

---

## 23. MVP 成功标准

v0.1 成功标准：

1. 普通 Xbox 风格手柄可被识别。
2. 手柄热插拔不会导致崩溃。
3. 用户可以按下手柄按钮完成绑定。
4. 用户可以将按钮映射到键盘键。
5. 用户可以将按钮映射到鼠标点击。
6. 用户可以将摇杆映射到鼠标移动。
7. profile 可以保存和加载。
8. 映射启停可靠。
9. UI 可以显示实时输入状态。
10. 日志能帮助定位大部分输入和输出问题。
11. Core 层有基础单元测试。
12. Windows 版本可实际运行。
13. C++ 代码整体遵守 ZeroStyle。

---

## 24. 后续版本方向

### v0.2

1. 长按 / 短按。
2. 双击。
3. 连发。
4. 组合键。
5. Layer / Mode Shift。
6. per-app profile 手动选择。
7. 更完整的冲突检测。
8. 摇杆曲线编辑器。

### v0.3

1. Linux uinput 输出。
2. per-app 自动 profile 切换。
3. 更完整的宏系统。
4. 宏取消和恢复。
5. 配置迁移工具。
6. 更完整的设备匹配策略。

### v1.0

1. 虚拟手柄输出。
2. 原始手柄隐藏集成。
3. 脚本系统。
4. 插件系统。
5. 更完整的多手柄支持。
6. 完整打包和自动更新。
7. 用户文档和开发者文档。
