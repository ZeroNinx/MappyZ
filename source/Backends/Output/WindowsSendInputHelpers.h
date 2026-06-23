// Windows SendInput 输出转换辅助函数。
// 纯数据转换，不依赖 Win32 运行时，方便独立进行单元测试。
// 本头文件是 Windows 输出后端的内部实现细节，不属于公共 API。
//
// VK 镜像值和鼠标标志在 WindowsSendInputBackend.cpp 中通过 static_assert 与 Win32 头文件校验。

#pragma once

#include "Core/Action.h"
#include "Core/ProjectCore.h"

#include <algorithm>
#include <cctype>

namespace MappyZ
{
namespace SendInputHelpers
{

// ── Windows Virtual-Key 镜像常量 ──
// 与 Windows.h 中 VK_* 宏定义保持一致

namespace VirtualKey
{

// 字母键 A-Z（VK 码与 ASCII 大写字母相同）
inline constexpr uint32 VkA = 0x41;
inline constexpr uint32 VkB = 0x42;
inline constexpr uint32 VkC = 0x43;
inline constexpr uint32 VkD = 0x44;
inline constexpr uint32 VkE = 0x45;
inline constexpr uint32 VkF = 0x46;
inline constexpr uint32 VkG = 0x47;
inline constexpr uint32 VkH = 0x48;
inline constexpr uint32 VkI = 0x49;
inline constexpr uint32 VkJ = 0x4A;
inline constexpr uint32 VkK = 0x4B;
inline constexpr uint32 VkL = 0x4C;
inline constexpr uint32 VkM = 0x4D;
inline constexpr uint32 VkN = 0x4E;
inline constexpr uint32 VkO = 0x4F;
inline constexpr uint32 VkP = 0x50;
inline constexpr uint32 VkQ = 0x51;
inline constexpr uint32 VkR = 0x52;
inline constexpr uint32 VkS = 0x53;
inline constexpr uint32 VkT = 0x54;
inline constexpr uint32 VkU = 0x55;
inline constexpr uint32 VkV = 0x56;
inline constexpr uint32 VkW = 0x57;
inline constexpr uint32 VkX = 0x58;
inline constexpr uint32 VkY = 0x59;
inline constexpr uint32 VkZ = 0x5A;

// 数字键 0-9（VK 码与 ASCII 数字字符相同）
inline constexpr uint32 Vk0 = 0x30;
inline constexpr uint32 Vk1 = 0x31;
inline constexpr uint32 Vk2 = 0x32;
inline constexpr uint32 Vk3 = 0x33;
inline constexpr uint32 Vk4 = 0x34;
inline constexpr uint32 Vk5 = 0x35;
inline constexpr uint32 Vk6 = 0x36;
inline constexpr uint32 Vk7 = 0x37;
inline constexpr uint32 Vk8 = 0x38;
inline constexpr uint32 Vk9 = 0x39;

// 常用功能键
inline constexpr uint32 VkSpace     = 0x20;
inline constexpr uint32 VkReturn    = 0x0D;
inline constexpr uint32 VkEscape    = 0x1B;
inline constexpr uint32 VkTab       = 0x09;
inline constexpr uint32 VkBackspace = 0x08;

// 方向键
inline constexpr uint32 VkArrowUp    = 0x26;
inline constexpr uint32 VkArrowDown  = 0x28;
inline constexpr uint32 VkArrowLeft  = 0x25;
inline constexpr uint32 VkArrowRight = 0x27;

// 修饰键（不区分左右）
inline constexpr uint32 VkShift   = 0x10;
inline constexpr uint32 VkControl = 0x11;
inline constexpr uint32 VkAlt     = 0x12;

// 功能键 F1-F12
inline constexpr uint32 VkF1  = 0x70;
inline constexpr uint32 VkF2  = 0x71;
inline constexpr uint32 VkF3  = 0x72;
inline constexpr uint32 VkF4  = 0x73;
inline constexpr uint32 VkF5  = 0x74;
inline constexpr uint32 VkF6  = 0x75;
inline constexpr uint32 VkF7  = 0x76;
inline constexpr uint32 VkF8  = 0x77;
inline constexpr uint32 VkF9  = 0x78;
inline constexpr uint32 VkF10 = 0x79;
inline constexpr uint32 VkF11 = 0x7A;
inline constexpr uint32 VkF12 = 0x7B;

}  // namespace VirtualKey

// ── Windows 鼠标事件标志镜像常量 ──
// 与 Windows.h 中 MOUSEEVENTF_* 宏定义保持一致

namespace MouseFlag
{

inline constexpr uint32 Move       = 0x0001;
inline constexpr uint32 LeftDown   = 0x0002;
inline constexpr uint32 LeftUp     = 0x0004;
inline constexpr uint32 RightDown  = 0x0008;
inline constexpr uint32 RightUp    = 0x0010;
inline constexpr uint32 MiddleDown = 0x0020;
inline constexpr uint32 MiddleUp   = 0x0040;
inline constexpr uint32 Wheel      = 0x0800;

}  // namespace MouseFlag

// ── SendInput 内部命令类型 ──

enum class ESendInputCommandType
{
    Keyboard,
    MouseButton,
    MouseMove,
    MouseWheel,
};

// SendInput 命令描述，不含 Win32 INPUT 类型
struct SSendInputCommand
{
    ESendInputCommandType Type = ESendInputCommandType::Keyboard;

    // 键盘：VK 码
    uint32 VirtualKeyCode = 0;
    // 键盘：是否为 key up（false 表示 key down）
    bool bKeyUp = false;

    // 鼠标按钮/移动/滚轮：标志位
    uint32 MouseFlags = 0;

    // 鼠标移动：相对偏移
    int32 DeltaX = 0;
    int32 DeltaY = 0;

    // 鼠标滚轮：滚动量（已按 WHEEL_DELTA 缩放）
    int32 WheelDelta = 0;
};

// ── 转换函数 ──

// 将平台无关的 key name 映射为 Windows Virtual-Key 码。
// 大小写不敏感，"a" 和 "A" 映射到同一个 VK 码。
// 返回空 optional 表示未知的 key name。
NODISCARD inline TOptional<uint32> MapKeyNameToVirtualKey(StdStringView KeyName)
{
    if (KeyName.empty())
    {
        return std::nullopt;
    }

    // 单字符按键：字母和数字
    if (KeyName.size() == 1)
    {
        char Character = KeyName[0];

        // 字母 a-z / A-Z
        if (std::isalpha(static_cast<unsigned char>(Character)))
        {
            char Upper = static_cast<char>(std::toupper(static_cast<unsigned char>(Character)));
            return static_cast<uint32>(Upper);
        }

        // 数字 0-9
        if (Character >= '0' && Character <= '9')
        {
            return static_cast<uint32>(Character);
        }

        return std::nullopt;
    }

    // 多字符功能键名称匹配
    // 为了大小写不敏感比较，先转为小写
    StdString LowerName(KeyName);
    std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(),
        [](unsigned char C) { return static_cast<char>(std::tolower(C)); });

    if (LowerName == "space")      return VirtualKey::VkSpace;
    if (LowerName == "enter")      return VirtualKey::VkReturn;
    if (LowerName == "escape")     return VirtualKey::VkEscape;
    if (LowerName == "tab")        return VirtualKey::VkTab;
    if (LowerName == "backspace")  return VirtualKey::VkBackspace;

    if (LowerName == "arrowup")    return VirtualKey::VkArrowUp;
    if (LowerName == "arrowdown")  return VirtualKey::VkArrowDown;
    if (LowerName == "arrowleft")  return VirtualKey::VkArrowLeft;
    if (LowerName == "arrowright") return VirtualKey::VkArrowRight;

    if (LowerName == "shift")      return VirtualKey::VkShift;
    if (LowerName == "control")    return VirtualKey::VkControl;
    if (LowerName == "alt")        return VirtualKey::VkAlt;

    if (LowerName == "f1")  return VirtualKey::VkF1;
    if (LowerName == "f2")  return VirtualKey::VkF2;
    if (LowerName == "f3")  return VirtualKey::VkF3;
    if (LowerName == "f4")  return VirtualKey::VkF4;
    if (LowerName == "f5")  return VirtualKey::VkF5;
    if (LowerName == "f6")  return VirtualKey::VkF6;
    if (LowerName == "f7")  return VirtualKey::VkF7;
    if (LowerName == "f8")  return VirtualKey::VkF8;
    if (LowerName == "f9")  return VirtualKey::VkF9;
    if (LowerName == "f10") return VirtualKey::VkF10;
    if (LowerName == "f11") return VirtualKey::VkF11;
    if (LowerName == "f12") return VirtualKey::VkF12;

    return std::nullopt;
}

// 将鼠标按钮编号映射为 Windows 鼠标事件标志。
// Button: 0=左键, 1=右键, 2=中键。
// 返回空 optional 表示未知的按钮编号。
NODISCARD inline TOptional<uint32> MapMouseButtonToFlags(int32 Button, bool bPressed)
{
    switch (Button)
    {
    case 0:
        return bPressed ? MouseFlag::LeftDown : MouseFlag::LeftUp;
    case 1:
        return bPressed ? MouseFlag::RightDown : MouseFlag::RightUp;
    case 2:
        return bPressed ? MouseFlag::MiddleDown : MouseFlag::MiddleUp;
    default:
        return std::nullopt;
    }
}

// WHEEL_DELTA 镜像常量（Windows 标准滚轮步进量为 120）
inline constexpr int32 WheelDeltaUnit = 120;

// 将 SAction 转换为 SendInput 内部命令。
// 执行全部验证：None 类型、type/payload 不匹配、未知 key、未知 button。
// 失败返回 EErrorCode::InvalidArgument。
NODISCARD inline TResult<SSendInputCommand> BuildCommandFromAction(const SAction& Action)
{
    if (Action.Type == EActionType::None)
    {
        return TResult<SSendInputCommand>::Err(
            MakeError(EErrorCode::InvalidArgument, "invalid action type: none"));
    }

    switch (Action.Type)
    {
    case EActionType::KeyboardKey:
    {
        const auto* KeyAction = std::get_if<SKeyboardAction>(&Action.Payload);
        if (!KeyAction)
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument, "action type is KeyboardKey but payload is not SKeyboardAction"));
        }

        auto VirtualKey = MapKeyNameToVirtualKey(KeyAction->Key);
        if (!VirtualKey.has_value())
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument, "unknown key name: " + KeyAction->Key));
        }

        SSendInputCommand Command;
        Command.Type = ESendInputCommandType::Keyboard;
        Command.VirtualKeyCode = VirtualKey.value();
        Command.bKeyUp = !KeyAction->bPressed;
        return TResult<SSendInputCommand>::Ok(Command);
    }

    case EActionType::MouseButton:
    {
        const auto* ButtonAction = std::get_if<SMouseButtonAction>(&Action.Payload);
        if (!ButtonAction)
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument, "action type is MouseButton but payload is not SMouseButtonAction"));
        }

        auto Flags = MapMouseButtonToFlags(ButtonAction->Button, ButtonAction->bPressed);
        if (!Flags.has_value())
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument,
                    "unknown mouse button: " + std::to_string(ButtonAction->Button)));
        }

        SSendInputCommand Command;
        Command.Type = ESendInputCommandType::MouseButton;
        Command.MouseFlags = Flags.value();
        return TResult<SSendInputCommand>::Ok(Command);
    }

    case EActionType::MouseMove:
    {
        const auto* MoveAction = std::get_if<SMouseMoveAction>(&Action.Payload);
        if (!MoveAction)
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument, "action type is MouseMove but payload is not SMouseMoveAction"));
        }

        SSendInputCommand Command;
        Command.Type = ESendInputCommandType::MouseMove;
        Command.MouseFlags = MouseFlag::Move;
        Command.DeltaX = static_cast<int32>(MoveAction->DeltaX);
        Command.DeltaY = static_cast<int32>(MoveAction->DeltaY);
        return TResult<SSendInputCommand>::Ok(Command);
    }

    case EActionType::MouseWheel:
    {
        const auto* WheelAction = std::get_if<SMouseWheelAction>(&Action.Payload);
        if (!WheelAction)
        {
            return TResult<SSendInputCommand>::Err(
                MakeError(EErrorCode::InvalidArgument, "action type is MouseWheel but payload is not SMouseWheelAction"));
        }

        SSendInputCommand Command;
        Command.Type = ESendInputCommandType::MouseWheel;
        Command.MouseFlags = MouseFlag::Wheel;
        Command.WheelDelta = static_cast<int32>(WheelAction->Delta * static_cast<float32>(WheelDeltaUnit));
        return TResult<SSendInputCommand>::Ok(Command);
    }

    default:
        return TResult<SSendInputCommand>::Err(
            MakeError(EErrorCode::InvalidArgument, "unsupported action type"));
    }
}

}  // namespace SendInputHelpers
}  // namespace MappyZ
