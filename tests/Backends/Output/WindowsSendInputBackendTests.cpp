// ZWindowsSendInputBackend 单元测试。
// 使用 fake sender 注入，验证动作转发、状态管理和错误处理。
// 不产生真实系统输入。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Output/WindowsSendInputBackend.h"
#include "Backends/Output/WindowsSendInputHelpers.h"

using namespace MappyZ;
using namespace MappyZ::SendInputHelpers;

// ── fake sender 辅助 ──

struct SFakeSenderState
{
    TVector<SSendInputCommand> RecordedCommands;
    bool bShouldSucceed = true;
};

static ZWindowsSendInputBackend::TNativeSender MakeFakeSender(SFakeSenderState& State)
{
    return [&State](const SSendInputCommand& Command) -> bool
    {
        State.RecordedCommands.push_back(Command);
        return State.bShouldSucceed;
    };
}

// ── 构造辅助 ──

static SAction MakeKeyboardAction(const StdString& Key, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SKeyboardAction{.Key = Key, .bPressed = bPressed};
    return Action;
}

static SAction MakeMouseButtonAction(int32 Button, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::MouseButton;
    Action.Payload = SMouseButtonAction{.Button = Button, .bPressed = bPressed};
    return Action;
}

static SAction MakeMouseMoveAction(float32 DeltaX, float32 DeltaY)
{
    SAction Action;
    Action.Type = EActionType::MouseMove;
    Action.Payload = SMouseMoveAction{.DeltaX = DeltaX, .DeltaY = DeltaY};
    return Action;
}

static SAction MakeMouseWheelAction(float32 Delta)
{
    SAction Action;
    Action.Type = EActionType::MouseWheel;
    Action.Payload = SMouseWheelAction{.Delta = Delta};
    return Action;
}

// ── 默认状态 ──

TEST_CASE("WindowsSendInputBackend default state is Ready", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Ready);
    REQUIRE(Status.Message == "ready");
}

// ── 键盘动作 ──

TEST_CASE("WindowsSendInputBackend sends keyboard key down", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeKeyboardAction("A", true));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::Keyboard);
    REQUIRE(Command.VirtualKeyCode == VirtualKey::VkA);
    REQUIRE(Command.bKeyUp == false);
}

TEST_CASE("WindowsSendInputBackend sends keyboard key up", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeKeyboardAction("Space", false));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::Keyboard);
    REQUIRE(Command.VirtualKeyCode == VirtualKey::VkSpace);
    REQUIRE(Command.bKeyUp == true);
}

// ── 鼠标按钮动作 ──

TEST_CASE("WindowsSendInputBackend sends mouse button pressed", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseButtonAction(0, true));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::MouseButton);
    REQUIRE(Command.MouseFlags == MouseFlag::LeftDown);
}

TEST_CASE("WindowsSendInputBackend sends mouse button released", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseButtonAction(1, false));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::MouseButton);
    REQUIRE(Command.MouseFlags == MouseFlag::RightUp);
}

// ── 鼠标移动动作 ──

TEST_CASE("WindowsSendInputBackend sends mouse move", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseMoveAction(10.0f, -5.0f));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::MouseMove);
    REQUIRE(Command.DeltaX == 10);
    REQUIRE(Command.DeltaY == -5);
}

TEST_CASE("WindowsSendInputBackend accumulates mouse move residual", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    // 小于 1 像素的移动不发送
    auto Result1 = Backend.SendAction(MakeMouseMoveAction(0.3f, 0.4f));
    REQUIRE(Result1.IsOk());
    REQUIRE(FakeState.RecordedCommands.empty());

    // 再次累积后超过 1 像素
    auto Result2 = Backend.SendAction(MakeMouseMoveAction(0.3f, 0.4f));
    REQUIRE(Result2.IsOk());
    REQUIRE(FakeState.RecordedCommands.empty());

    // 第三次累积：0.9 仍不足 1
    auto Result3 = Backend.SendAction(MakeMouseMoveAction(0.3f, 0.4f));
    REQUIRE(Result3.IsOk());

    // X: 0.3+0.3+0.3 = 0.9 不够 1，Y: 0.4+0.4+0.4 = 1.2 够了
    // 但 trunc(0.9) = 0, trunc(1.2) = 1
    REQUIRE(FakeState.RecordedCommands.size() == 1);
    REQUIRE(FakeState.RecordedCommands[0].DeltaX == 0);
    REQUIRE(FakeState.RecordedCommands[0].DeltaY == 1);
}

TEST_CASE("WindowsSendInputBackend skips zero integer mouse move", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseMoveAction(0.1f, 0.1f));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.empty());
}

// ── 鼠标滚轮动作 ──

TEST_CASE("WindowsSendInputBackend sends mouse wheel", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseWheelAction(1.0f));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.size() == 1);

    auto& Command = FakeState.RecordedCommands[0];
    REQUIRE(Command.Type == ESendInputCommandType::MouseWheel);
    REQUIRE(Command.WheelDelta == WheelDeltaUnit);
}

TEST_CASE("WindowsSendInputBackend wheel delta zero does not call sender", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseWheelAction(0.0f));
    REQUIRE(Result.IsOk());
    REQUIRE(FakeState.RecordedCommands.empty());
}

// ── 错误路径：无效动作 ──

TEST_CASE("WindowsSendInputBackend rejects None action", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    SAction NoneAction;
    auto Result = Backend.SendAction(NoneAction);
    REQUIRE(Result.IsErr());
    REQUIRE(FakeState.RecordedCommands.empty());
}

TEST_CASE("WindowsSendInputBackend rejects unknown key name", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeKeyboardAction("Unknown", true));
    REQUIRE(Result.IsErr());
    REQUIRE(FakeState.RecordedCommands.empty());
}

TEST_CASE("WindowsSendInputBackend rejects unknown mouse button", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeMouseButtonAction(5, true));
    REQUIRE(Result.IsErr());
    REQUIRE(FakeState.RecordedCommands.empty());
}

TEST_CASE("WindowsSendInputBackend rejects mismatched payload", "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};

    auto Result = Backend.SendAction(Action);
    REQUIRE(Result.IsErr());
    REQUIRE(FakeState.RecordedCommands.empty());
}

// ── 错误路径：sender 失败 ──

TEST_CASE("WindowsSendInputBackend returns error and sets Error state when sender fails",
    "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    FakeState.bShouldSucceed = false;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    auto Result = Backend.SendAction(MakeKeyboardAction("A", true));
    REQUIRE(Result.IsErr());

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Error);
    REQUIRE(Status.Message == "SendInput failed");
}

// ── 状态恢复 ──

TEST_CASE("WindowsSendInputBackend recovers to Ready after successful send",
    "[Backends][WindowsSendInputBackend]")
{
    SFakeSenderState FakeState;
    FakeState.bShouldSucceed = false;
    ZWindowsSendInputBackend Backend(MakeFakeSender(FakeState));

    // 先失败，进入 Error 状态
    auto FailResult = Backend.SendAction(MakeKeyboardAction("A", true));
    REQUIRE(FailResult.IsErr());
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Error);

    // 恢复 sender，再次发送成功
    FakeState.bShouldSucceed = true;
    auto SuccessResult = Backend.SendAction(MakeKeyboardAction("B", true));
    REQUIRE(SuccessResult.IsOk());

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Ready);
    REQUIRE(Status.Message == "ready");
}

// ── 头文件无 Win32 依赖 ──

TEST_CASE("WindowsSendInputBackend header does not include Windows.h",
    "[Backends][WindowsSendInputBackend]")
{
    // 如果 WindowsSendInputBackend.h 包含了 Windows.h，
    // 则 _WINDOWS_ 或 _WINDEF_ 宏会被定义
#if defined(_WINDOWS_) || defined(_WINDEF_)
    FAIL("WindowsSendInputBackend.h or its includes leaked Windows.h into this translation unit");
#else
    SUCCEED();
#endif
}
