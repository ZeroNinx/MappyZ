// 平台无关的输出动作定义。
// 映射引擎将输入事件转换为 SAction，再由输出后端翻译为平台原生调用。
// 本文件不包含任何 Win32 virtual-key、scan-code 或 SDL 枚举，
// 平台相关转换完全在 IOutputBackend 实现中完成。

#pragma once

#include "Core/ProjectCore.h"

#include <variant>

namespace ZeroMapper
{

// 输出动作的类型
enum class EActionType
{
    None,         // 无动作，用于默认初始化
    KeyboardKey,  // 键盘按键
    MouseButton,  // 鼠标按钮点击
    MouseMove,    // 鼠标指针移动
    MouseWheel,   // 鼠标滚轮（预留）
};

// 键盘按键动作，Key 使用平台无关的名称（例如 "Space"、"A"、"Escape"）
struct SKeyboardAction
{
    StdString Key;
    bool bPressed = false;
};

// 鼠标按钮动作，Button 使用逻辑编号：0=左键，1=右键，2=中键
struct SMouseButtonAction
{
    int32 Button = 0;
    bool bPressed = false;
};

// 鼠标移动动作，DeltaX/DeltaY 为像素级偏移量
struct SMouseMoveAction
{
    float32 DeltaX = 0.0f;
    float32 DeltaY = 0.0f;
};

// 鼠标滚轮动作，Delta 为滚动量，正值向上/向前，负值向下/向后
struct SMouseWheelAction
{
    float32 Delta = 0.0f;
};

// 动作的具体载荷，使用 variant 实现类型安全的多态
using TActionPayload = std::variant<
    std::monostate,
    SKeyboardAction,
    SMouseButtonAction,
    SMouseMoveAction,
    SMouseWheelAction>;

// 标准化输出动作，映射引擎的输出、输出后端的输入
struct SAction
{
    EActionType Type = EActionType::None;
    TActionPayload Payload;
};

}  // namespace ZeroMapper
