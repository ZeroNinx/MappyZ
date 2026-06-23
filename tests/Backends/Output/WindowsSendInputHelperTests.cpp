// WindowsSendInputHelpers 纯函数单元测试。
// 不需要 Windows API 运行时，验证 VK 映射、鼠标标志映射和 BuildCommandFromAction 转换逻辑。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Output/WindowsSendInputHelpers.h"

using namespace MappyZ;
using namespace MappyZ::SendInputHelpers;

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

// ── MapKeyNameToVirtualKey: 字母键 ──

TEST_CASE("MapKeyNameToVirtualKey maps A-Z uppercase", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("A").value() == VirtualKey::VkA);
    REQUIRE(MapKeyNameToVirtualKey("M").value() == VirtualKey::VkM);
    REQUIRE(MapKeyNameToVirtualKey("Z").value() == VirtualKey::VkZ);
}

TEST_CASE("MapKeyNameToVirtualKey maps a-z lowercase to same VK", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("a").value() == VirtualKey::VkA);
    REQUIRE(MapKeyNameToVirtualKey("m").value() == VirtualKey::VkM);
    REQUIRE(MapKeyNameToVirtualKey("z").value() == VirtualKey::VkZ);
}

// ── MapKeyNameToVirtualKey: 数字键 ──

TEST_CASE("MapKeyNameToVirtualKey maps 0-9", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("0").value() == VirtualKey::Vk0);
    REQUIRE(MapKeyNameToVirtualKey("5").value() == VirtualKey::Vk5);
    REQUIRE(MapKeyNameToVirtualKey("9").value() == VirtualKey::Vk9);
}

// ── MapKeyNameToVirtualKey: 功能键 ──

TEST_CASE("MapKeyNameToVirtualKey maps common function keys", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("Space").value()     == VirtualKey::VkSpace);
    REQUIRE(MapKeyNameToVirtualKey("Enter").value()     == VirtualKey::VkReturn);
    REQUIRE(MapKeyNameToVirtualKey("Escape").value()    == VirtualKey::VkEscape);
    REQUIRE(MapKeyNameToVirtualKey("Tab").value()       == VirtualKey::VkTab);
    REQUIRE(MapKeyNameToVirtualKey("Backspace").value() == VirtualKey::VkBackspace);
}

TEST_CASE("MapKeyNameToVirtualKey is case insensitive for function keys", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("space").value()     == VirtualKey::VkSpace);
    REQUIRE(MapKeyNameToVirtualKey("ENTER").value()     == VirtualKey::VkReturn);
    REQUIRE(MapKeyNameToVirtualKey("escape").value()    == VirtualKey::VkEscape);
    REQUIRE(MapKeyNameToVirtualKey("BACKSPACE").value() == VirtualKey::VkBackspace);
}

// ── MapKeyNameToVirtualKey: 方向键 ──

TEST_CASE("MapKeyNameToVirtualKey maps arrow keys", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("ArrowUp").value()    == VirtualKey::VkArrowUp);
    REQUIRE(MapKeyNameToVirtualKey("ArrowDown").value()  == VirtualKey::VkArrowDown);
    REQUIRE(MapKeyNameToVirtualKey("ArrowLeft").value()  == VirtualKey::VkArrowLeft);
    REQUIRE(MapKeyNameToVirtualKey("ArrowRight").value() == VirtualKey::VkArrowRight);
}

// ── MapKeyNameToVirtualKey: 修饰键 ──

TEST_CASE("MapKeyNameToVirtualKey maps modifier keys", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("Shift").value()   == VirtualKey::VkShift);
    REQUIRE(MapKeyNameToVirtualKey("Control").value() == VirtualKey::VkControl);
    REQUIRE(MapKeyNameToVirtualKey("Alt").value()     == VirtualKey::VkAlt);
}

// ── MapKeyNameToVirtualKey: F1-F12 ──

TEST_CASE("MapKeyNameToVirtualKey maps F1 through F12", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("F1").value()  == VirtualKey::VkF1);
    REQUIRE(MapKeyNameToVirtualKey("F6").value()  == VirtualKey::VkF6);
    REQUIRE(MapKeyNameToVirtualKey("F12").value() == VirtualKey::VkF12);
}

TEST_CASE("MapKeyNameToVirtualKey F keys are case insensitive", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("f1").value()  == VirtualKey::VkF1);
    REQUIRE(MapKeyNameToVirtualKey("f12").value() == VirtualKey::VkF12);
}

// ── MapKeyNameToVirtualKey: 未知键 ──

TEST_CASE("MapKeyNameToVirtualKey returns nullopt for unknown key", "[Backends][SendInputHelpers]")
{
    REQUIRE_FALSE(MapKeyNameToVirtualKey("").has_value());
    REQUIRE_FALSE(MapKeyNameToVirtualKey("Unknown").has_value());
    REQUIRE_FALSE(MapKeyNameToVirtualKey("@").has_value());
    REQUIRE_FALSE(MapKeyNameToVirtualKey("NumpadPlus").has_value());
}

// ── MapMouseButtonToFlags ──

TEST_CASE("MapMouseButtonToFlags maps button 0 to left", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToFlags(0, true).value()  == MouseFlag::LeftDown);
    REQUIRE(MapMouseButtonToFlags(0, false).value() == MouseFlag::LeftUp);
}

TEST_CASE("MapMouseButtonToFlags maps button 1 to right", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToFlags(1, true).value()  == MouseFlag::RightDown);
    REQUIRE(MapMouseButtonToFlags(1, false).value() == MouseFlag::RightUp);
}

TEST_CASE("MapMouseButtonToFlags maps button 2 to middle", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToFlags(2, true).value()  == MouseFlag::MiddleDown);
    REQUIRE(MapMouseButtonToFlags(2, false).value() == MouseFlag::MiddleUp);
}

TEST_CASE("MapMouseButtonToFlags returns nullopt for unknown button", "[Backends][SendInputHelpers]")
{
    REQUIRE_FALSE(MapMouseButtonToFlags(3, true).has_value());
    REQUIRE_FALSE(MapMouseButtonToFlags(-1, false).has_value());
    REQUIRE_FALSE(MapMouseButtonToFlags(99, true).has_value());
}

// ── BuildCommandFromAction: 正常路径 ──

TEST_CASE("BuildCommandFromAction builds keyboard key down command", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeKeyboardAction("A", true));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::Keyboard);
    REQUIRE(Command.VirtualKeyCode == VirtualKey::VkA);
    REQUIRE(Command.bKeyUp == false);
}

TEST_CASE("BuildCommandFromAction builds keyboard key up command", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeKeyboardAction("Space", false));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::Keyboard);
    REQUIRE(Command.VirtualKeyCode == VirtualKey::VkSpace);
    REQUIRE(Command.bKeyUp == true);
}

TEST_CASE("BuildCommandFromAction builds mouse button command", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseButtonAction(0, true));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::MouseButton);
    REQUIRE(Command.MouseFlags == MouseFlag::LeftDown);
}

TEST_CASE("BuildCommandFromAction builds mouse move command", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseMoveAction(10.0f, -5.0f));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::MouseMove);
    REQUIRE(Command.MouseFlags == MouseFlag::Move);
    REQUIRE(Command.DeltaX == 10);
    REQUIRE(Command.DeltaY == -5);
}

TEST_CASE("BuildCommandFromAction builds mouse wheel command", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseWheelAction(1.0f));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::MouseWheel);
    REQUIRE(Command.MouseFlags == MouseFlag::Wheel);
    REQUIRE(Command.WheelDelta == WheelDeltaUnit);
}

TEST_CASE("BuildCommandFromAction scales negative wheel delta", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseWheelAction(-2.0f));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.WheelDelta == -2 * WheelDeltaUnit);
}

// ── BuildCommandFromAction: 错误路径 ──

TEST_CASE("BuildCommandFromAction rejects None action", "[Backends][SendInputHelpers]")
{
    SAction NoneAction;
    auto Result = BuildCommandFromAction(NoneAction);
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects unknown key name", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeKeyboardAction("Unknown", true));
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects unknown mouse button", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseButtonAction(5, true));
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects mismatched keyboard payload", "[Backends][SendInputHelpers]")
{
    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};

    auto Result = BuildCommandFromAction(Action);
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects mismatched mouse button payload", "[Backends][SendInputHelpers]")
{
    SAction Action;
    Action.Type = EActionType::MouseButton;
    Action.Payload = SKeyboardAction{.Key = "A", .bPressed = true};

    auto Result = BuildCommandFromAction(Action);
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects mismatched mouse move payload", "[Backends][SendInputHelpers]")
{
    SAction Action;
    Action.Type = EActionType::MouseMove;
    Action.Payload = SKeyboardAction{.Key = "A", .bPressed = true};

    auto Result = BuildCommandFromAction(Action);
    REQUIRE(Result.IsErr());
}

TEST_CASE("BuildCommandFromAction rejects mismatched mouse wheel payload", "[Backends][SendInputHelpers]")
{
    SAction Action;
    Action.Type = EActionType::MouseWheel;
    Action.Payload = SMouseMoveAction{.DeltaX = 1.0f, .DeltaY = 2.0f};

    auto Result = BuildCommandFromAction(Action);
    REQUIRE(Result.IsErr());
}
