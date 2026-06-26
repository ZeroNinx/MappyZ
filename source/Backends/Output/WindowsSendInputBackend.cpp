// ZWindowsSendInputBackend 实现。
// 将 SAction 通过 helper 转为 SSendInputCommand，再经 NativeSender 调用 Win32 SendInput。
// 所有非预期路径都输出日志以方便定位问题。

#include "Backends/Output/WindowsSendInputBackend.h"

#include <cmath>
#include <cstdio>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// ── static_assert 校验镜像常量与 Win32 头文件一致 ──

namespace MappyZ
{
namespace SendInputHelpers
{

// 字母键
static_assert(VirtualKey::VkA == 'A', "VkA mismatch");
static_assert(VirtualKey::VkZ == 'Z', "VkZ mismatch");

// 数字键
static_assert(VirtualKey::Vk0 == '0', "Vk0 mismatch");
static_assert(VirtualKey::Vk9 == '9', "Vk9 mismatch");

// 功能键
static_assert(VirtualKey::VkSpace     == VK_SPACE,   "VkSpace mismatch");
static_assert(VirtualKey::VkReturn    == VK_RETURN,  "VkReturn mismatch");
static_assert(VirtualKey::VkEscape    == VK_ESCAPE,  "VkEscape mismatch");
static_assert(VirtualKey::VkTab       == VK_TAB,     "VkTab mismatch");
static_assert(VirtualKey::VkBackspace == VK_BACK,    "VkBackspace mismatch");

// 方向键
static_assert(VirtualKey::VkArrowUp    == VK_UP,    "VkArrowUp mismatch");
static_assert(VirtualKey::VkArrowDown  == VK_DOWN,  "VkArrowDown mismatch");
static_assert(VirtualKey::VkArrowLeft  == VK_LEFT,  "VkArrowLeft mismatch");
static_assert(VirtualKey::VkArrowRight == VK_RIGHT, "VkArrowRight mismatch");

// 修饰键
static_assert(VirtualKey::VkShift   == VK_SHIFT,   "VkShift mismatch");
static_assert(VirtualKey::VkControl == VK_CONTROL, "VkControl mismatch");
static_assert(VirtualKey::VkAlt     == VK_MENU,    "VkAlt mismatch");

// F1-F12
static_assert(VirtualKey::VkF1  == VK_F1,  "VkF1 mismatch");
static_assert(VirtualKey::VkF2  == VK_F2,  "VkF2 mismatch");
static_assert(VirtualKey::VkF3  == VK_F3,  "VkF3 mismatch");
static_assert(VirtualKey::VkF4  == VK_F4,  "VkF4 mismatch");
static_assert(VirtualKey::VkF5  == VK_F5,  "VkF5 mismatch");
static_assert(VirtualKey::VkF6  == VK_F6,  "VkF6 mismatch");
static_assert(VirtualKey::VkF7  == VK_F7,  "VkF7 mismatch");
static_assert(VirtualKey::VkF8  == VK_F8,  "VkF8 mismatch");
static_assert(VirtualKey::VkF9  == VK_F9,  "VkF9 mismatch");
static_assert(VirtualKey::VkF10 == VK_F10, "VkF10 mismatch");
static_assert(VirtualKey::VkF11 == VK_F11, "VkF11 mismatch");
static_assert(VirtualKey::VkF12 == VK_F12, "VkF12 mismatch");

// 鼠标标志
static_assert(MouseFlag::Move       == MOUSEEVENTF_MOVE,       "MouseFlag::Move mismatch");
static_assert(MouseFlag::LeftDown   == MOUSEEVENTF_LEFTDOWN,   "MouseFlag::LeftDown mismatch");
static_assert(MouseFlag::LeftUp     == MOUSEEVENTF_LEFTUP,     "MouseFlag::LeftUp mismatch");
static_assert(MouseFlag::RightDown  == MOUSEEVENTF_RIGHTDOWN,  "MouseFlag::RightDown mismatch");
static_assert(MouseFlag::RightUp    == MOUSEEVENTF_RIGHTUP,    "MouseFlag::RightUp mismatch");
static_assert(MouseFlag::MiddleDown == MOUSEEVENTF_MIDDLEDOWN, "MouseFlag::MiddleDown mismatch");
static_assert(MouseFlag::MiddleUp   == MOUSEEVENTF_MIDDLEUP,   "MouseFlag::MiddleUp mismatch");
static_assert(MouseFlag::Wheel      == MOUSEEVENTF_WHEEL,      "MouseFlag::Wheel mismatch");

// WHEEL_DELTA
static_assert(WheelDeltaUnit == WHEEL_DELTA, "WheelDeltaUnit mismatch");

}  // namespace SendInputHelpers

// ── 扩展键判定 ──
// 扩展键需要在 SendInput 中设置 KEYEVENTF_EXTENDEDKEY 标志，
// 否则 Windows 会将其误解为小键盘等价键（如方向键变成 Numpad 4/6/8/2）
static bool IsExtendedKey(WORD VirtualKeyCode)
{
    switch (VirtualKeyCode)
    {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_NUMLOCK:
    case VK_SNAPSHOT:
    case VK_DIVIDE:
    case VK_RCONTROL:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

// ── 默认 NativeSender 实现：调用真实 Win32 SendInput ──

static bool DefaultNativeSender(const SendInputHelpers::SSendInputCommand& Command)
{
    INPUT Input = {};

    switch (Command.Type)
    {
    case SendInputHelpers::ESendInputCommandType::Keyboard:
    {
        Input.type = INPUT_KEYBOARD;
        Input.ki.wVk = static_cast<WORD>(Command.VirtualKeyCode);
        Input.ki.wScan = static_cast<WORD>(
            ::MapVirtualKey(Input.ki.wVk, MAPVK_VK_TO_VSC));

        DWORD Flags = Command.bKeyUp ? KEYEVENTF_KEYUP : 0;
        if (IsExtendedKey(Input.ki.wVk))
        {
            Flags |= KEYEVENTF_EXTENDEDKEY;
        }
        Input.ki.dwFlags = Flags;
        break;
    }
    case SendInputHelpers::ESendInputCommandType::MouseButton:
    {
        Input.type = INPUT_MOUSE;
        Input.mi.dwFlags = static_cast<DWORD>(Command.MouseFlags);
        break;
    }
    case SendInputHelpers::ESendInputCommandType::MouseMove:
    {
        Input.type = INPUT_MOUSE;
        Input.mi.dwFlags = static_cast<DWORD>(Command.MouseFlags);
        Input.mi.dx = static_cast<LONG>(Command.DeltaX);
        Input.mi.dy = static_cast<LONG>(Command.DeltaY);
        break;
    }
    case SendInputHelpers::ESendInputCommandType::MouseWheel:
    {
        Input.type = INPUT_MOUSE;
        Input.mi.dwFlags = static_cast<DWORD>(Command.MouseFlags);
        Input.mi.mouseData = static_cast<DWORD>(Command.WheelDelta);
        break;
    }
    default:
    {
        std::fprintf(stderr, "[WindowsSendInputBackend] 错误: 未知的命令类型 %d\n",
            static_cast<int>(Command.Type));
        return false;
    }
    }

    UINT Sent = ::SendInput(1, &Input, sizeof(INPUT));
    if (Sent != 1)
    {
        DWORD ErrorCode = ::GetLastError();
        std::fprintf(stderr, "[WindowsSendInputBackend] 错误: SendInput 返回 %u (预期 1), GetLastError=%lu\n",
            Sent, ErrorCode);
        return false;
    }

    return true;
}

// ── 构造函数 ──

ZWindowsSendInputBackend::ZWindowsSendInputBackend()
    : NativeSender(DefaultNativeSender)
    , CurrentStatus{.State = EOutputBackendState::Ready, .Message = "ready"}
{
}

ZWindowsSendInputBackend::ZWindowsSendInputBackend(TNativeSender CustomSender)
    : NativeSender(std::move(CustomSender))
    , CurrentStatus{.State = EOutputBackendState::Ready, .Message = "ready"}
{
}

// ── SendAction ──

TResult<void> ZWindowsSendInputBackend::SendAction(const SAction& Action)
{
    // 转换并验证动作
    auto CommandResult = SendInputHelpers::BuildCommandFromAction(Action);
    if (CommandResult.IsErr())
    {
        auto& Error = CommandResult.Failure();
        std::fprintf(stderr, "[WindowsSendInputBackend] 警告: 动作验证失败 (%s)\n",
            Error.Message.c_str());
        return TResult<void>::Err(MakeError(Error.Code, Error.Message, Error.ContextPath));
    }

    auto Command = CommandResult.Value();

    // 鼠标移动：累加残差后取整，保留小数部分
    if (Command.Type == SendInputHelpers::ESendInputCommandType::MouseMove)
    {
        const auto* MoveAction = std::get_if<SMouseMoveAction>(&Action.Payload);
        if (MoveAction)
        {
            float32 AccumulatedX = MouseResidualX + MoveAction->DeltaX;
            float32 AccumulatedY = MouseResidualY + MoveAction->DeltaY;

            int32 IntegerX = static_cast<int32>(std::trunc(AccumulatedX));
            int32 IntegerY = static_cast<int32>(std::trunc(AccumulatedY));

            MouseResidualX = AccumulatedX - static_cast<float32>(IntegerX);
            MouseResidualY = AccumulatedY - static_cast<float32>(IntegerY);

            // 整数偏移为零时不需要发送
            if (IntegerX == 0 && IntegerY == 0)
            {
                return TResult<void>::Ok();
            }

            Command.DeltaX = IntegerX;
            Command.DeltaY = IntegerY;
        }
    }

    // 滚轮 delta 为 0 时不发送，直接返回成功
    if (Command.Type == SendInputHelpers::ESendInputCommandType::MouseWheel && Command.WheelDelta == 0)
    {
        return TResult<void>::Ok();
    }

    // 调用 native sender
    bool bSuccess = NativeSender(Command);
    if (!bSuccess)
    {
        CurrentStatus.State = EOutputBackendState::Error;
        CurrentStatus.Message = "SendInput failed";
        std::fprintf(stderr, "[WindowsSendInputBackend] 错误: NativeSender 返回失败\n");
        return TResult<void>::Err(
            MakeError(EErrorCode::Unknown, "SendInput failed"));
    }

    // 成功后自动恢复 Ready 状态
    if (CurrentStatus.State != EOutputBackendState::Ready)
    {
        CurrentStatus.State = EOutputBackendState::Ready;
        CurrentStatus.Message = "ready";
    }

    return TResult<void>::Ok();
}

// ── GetStatus ──

SOutputBackendStatus ZWindowsSendInputBackend::GetStatus() const
{
    return CurrentStatus;
}

}  // namespace MappyZ
