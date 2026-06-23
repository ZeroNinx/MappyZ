// SDL3 游戏手柄输入转换辅助函数。
// 纯数据转换，不依赖 SDL 运行时，方便独立进行单元测试。
// 本头文件是 SDL 后端的内部实现细节，不属于公共 API。
//
// 枚举镜像值在 SdlInputBackend.cpp 中通过 static_assert 与 SDL3 头文件校验。

#pragma once

#include "Core/ControlId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

#include <algorithm>

namespace MappyZ
{
namespace SdlInputHelpers
{

// ── SDL3 Gamepad 事件类型镜像值 ──
// 与 SDL_EventType 中手柄相关定义保持一致

namespace SdlGamepadEvent
{

inline constexpr uint32 AxisMotion = 0x650;
inline constexpr uint32 ButtonDown = 0x651;
inline constexpr uint32 ButtonUp = 0x652;
inline constexpr uint32 Added = 0x653;
inline constexpr uint32 Removed = 0x654;
inline constexpr uint32 Remapped = 0x655;

}  // namespace SdlGamepadEvent

// ── SDL3 Gamepad 按钮枚举镜像值 ──
// 与 SDL_GamepadButton 定义保持一致，避免本头文件直接引入 SDL 头

namespace SdlButton
{

inline constexpr int South = 0;
inline constexpr int East = 1;
inline constexpr int West = 2;
inline constexpr int North = 3;
inline constexpr int Back = 4;
inline constexpr int Guide = 5;
inline constexpr int Start = 6;
inline constexpr int LeftStick = 7;
inline constexpr int RightStick = 8;
inline constexpr int LeftShoulder = 9;
inline constexpr int RightShoulder = 10;
inline constexpr int DpadUp = 11;
inline constexpr int DpadDown = 12;
inline constexpr int DpadLeft = 13;
inline constexpr int DpadRight = 14;

}  // namespace SdlButton

// ── SDL3 Gamepad 轴枚举镜像值 ──
// 与 SDL_GamepadAxis 定义保持一致

namespace SdlAxis
{

inline constexpr int LeftX = 0;
inline constexpr int LeftY = 1;
inline constexpr int RightX = 2;
inline constexpr int RightY = 3;
inline constexpr int LeftTrigger = 4;
inline constexpr int RightTrigger = 5;

}  // namespace SdlAxis

// ── 轴映射结果 ──

// 描述一个 SDL 轴对应的项目内部控件信息
struct SAxisMapping
{
    // 控件标识，例如 "left_stick" 或 "left_trigger"
    StdStringView ControlId;

    // 控件物理类型
    EInputControlType ControlType = EInputControlType::Axis2D;

    // 是否为 X 轴（仅 Axis2D 类型有效，Trigger 忽略此字段）
    bool bIsXAxis = true;
};

// ── 转换函数 ──

// 将 SDL 按钮枚举值映射为项目内部 ControlId 字符串。
// 返回空 optional 表示不认识的按钮类型（如 MISC、PADDLE、TOUCHPAD）。
NODISCARD inline TOptional<StdStringView> MapButtonToControlId(int SdlButtonValue)
{
    switch (SdlButtonValue)
    {
    case SdlButton::South:
        return ControlId::ButtonSouth;
    case SdlButton::East:
        return ControlId::ButtonEast;
    case SdlButton::West:
        return ControlId::ButtonWest;
    case SdlButton::North:
        return ControlId::ButtonNorth;
    case SdlButton::Back:
        return ControlId::ButtonBack;
    case SdlButton::Guide:
        return ControlId::ButtonGuide;
    case SdlButton::Start:
        return ControlId::ButtonStart;
    case SdlButton::LeftStick:
        return ControlId::LeftStickButton;
    case SdlButton::RightStick:
        return ControlId::RightStickButton;
    case SdlButton::LeftShoulder:
        return ControlId::LeftShoulder;
    case SdlButton::RightShoulder:
        return ControlId::RightShoulder;
    case SdlButton::DpadUp:
        return ControlId::DpadUp;
    case SdlButton::DpadDown:
        return ControlId::DpadDown;
    case SdlButton::DpadLeft:
        return ControlId::DpadLeft;
    case SdlButton::DpadRight:
        return ControlId::DpadRight;
    default:
        return std::nullopt;
    }
}

// 将 SDL 轴枚举值映射为项目内部轴信息。
// 返回空 optional 表示不认识的轴类型。
NODISCARD inline TOptional<SAxisMapping> MapAxisToMapping(int SdlAxisValue)
{
    switch (SdlAxisValue)
    {
    case SdlAxis::LeftX:
        return SAxisMapping{
            .ControlId = ControlId::LeftStick,
            .ControlType = EInputControlType::Axis2D,
            .bIsXAxis = true,
        };
    case SdlAxis::LeftY:
        return SAxisMapping{
            .ControlId = ControlId::LeftStick,
            .ControlType = EInputControlType::Axis2D,
            .bIsXAxis = false,
        };
    case SdlAxis::RightX:
        return SAxisMapping{
            .ControlId = ControlId::RightStick,
            .ControlType = EInputControlType::Axis2D,
            .bIsXAxis = true,
        };
    case SdlAxis::RightY:
        return SAxisMapping{
            .ControlId = ControlId::RightStick,
            .ControlType = EInputControlType::Axis2D,
            .bIsXAxis = false,
        };
    case SdlAxis::LeftTrigger:
        return SAxisMapping{
            .ControlId = ControlId::LeftTrigger,
            .ControlType = EInputControlType::Trigger,
            .bIsXAxis = false,
        };
    case SdlAxis::RightTrigger:
        return SAxisMapping{
            .ControlId = ControlId::RightTrigger,
            .ControlType = EInputControlType::Trigger,
            .bIsXAxis = false,
        };
    default:
        return std::nullopt;
    }
}

// 将 SDL 摇杆原始轴值归一化到 [-1.0, 1.0]。
// SDL 摇杆轴范围为 -32768 ~ 32767，正负方向分别除以不同的最大值以确保两端精确到达 ±1.0。
NODISCARD inline float32 NormalizeStickAxis(int16 RawValue)
{
    if (RawValue >= 0)
    {
        return static_cast<float32>(RawValue) / 32767.0f;
    }
    return static_cast<float32>(RawValue) / 32768.0f;
}

// 将 SDL 扳机原始轴值归一化到 [0.0, 1.0]。
// SDL 扳机轴范围为 0 ~ 32767，负值视为静止状态。
NODISCARD inline float32 NormalizeTriggerAxis(int16 RawValue)
{
    if (RawValue <= 0)
    {
        return 0.0f;
    }
    return std::min(static_cast<float32>(RawValue) / 32767.0f, 1.0f);
}

// 更新摇杆轴缓存并返回合并后的双轴值。
// SDL 分别发送 X 和 Y 轴事件，需要合并为完整的双轴值再发出。
NODISCARD inline SAxis2DValue MergeAxis2DCache(
    SAxis2DValue& Cache,
    bool bIsXAxis,
    float32 NormalizedValue)
{
    if (bIsXAxis)
    {
        Cache.X = NormalizedValue;
    }
    else
    {
        Cache.Y = NormalizedValue;
    }
    return Cache;
}

// ── 事件分类 ──

// SDL 手柄事件的逻辑分类
enum class ESdlGamepadAction
{
    DeviceAdded,
    DeviceRemoved,
    DeviceRemapped,
    ButtonInput,
    AxisInput,
    Ignored,
};

// 将 SDL 事件类型映射为后端处理分类。
// 用于事件分发和测试验证：只有 DeviceAdded 才触发 OnDeviceConnected 回调。
NODISCARD inline ESdlGamepadAction ClassifyGamepadEvent(uint32 SdlEventType)
{
    switch (SdlEventType)
    {
    case SdlGamepadEvent::Added:
        return ESdlGamepadAction::DeviceAdded;
    case SdlGamepadEvent::Removed:
        return ESdlGamepadAction::DeviceRemoved;
    case SdlGamepadEvent::Remapped:
        return ESdlGamepadAction::DeviceRemapped;
    case SdlGamepadEvent::ButtonDown:
    case SdlGamepadEvent::ButtonUp:
        return ESdlGamepadAction::ButtonInput;
    case SdlGamepadEvent::AxisMotion:
        return ESdlGamepadAction::AxisInput;
    default:
        return ESdlGamepadAction::Ignored;
    }
}

}  // namespace SdlInputHelpers
}  // namespace MappyZ
