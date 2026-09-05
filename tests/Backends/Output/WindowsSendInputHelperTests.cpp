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
    REQUIRE_FALSE(MapMouseButtonToFlags(5, true).has_value());
    REQUIRE_FALSE(MapMouseButtonToFlags(-1, false).has_value());
    REQUIRE_FALSE(MapMouseButtonToFlags(99, true).has_value());
}

TEST_CASE("MapMouseButtonToFlags maps button 3 to XDown/XUp", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToFlags(3, true).value()  == MouseFlag::XDown);
    REQUIRE(MapMouseButtonToFlags(3, false).value() == MouseFlag::XUp);
}

TEST_CASE("MapMouseButtonToFlags maps button 4 to XDown/XUp", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToFlags(4, true).value()  == MouseFlag::XDown);
    REQUIRE(MapMouseButtonToFlags(4, false).value() == MouseFlag::XUp);
}

TEST_CASE("MapMouseButtonToXButtonData returns correct data", "[Backends][SendInputHelpers]")
{
    REQUIRE(MapMouseButtonToXButtonData(3) == XButton::XButton1);
    REQUIRE(MapMouseButtonToXButtonData(4) == XButton::XButton2);
    REQUIRE(MapMouseButtonToXButtonData(0) == 0);
    REQUIRE(MapMouseButtonToXButtonData(2) == 0);
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
    REQUIRE(Command.MouseData == 0);
}

TEST_CASE("BuildCommandFromAction builds XButton down command for button 3", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseButtonAction(3, true));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::MouseButton);
    REQUIRE(Command.MouseFlags == MouseFlag::XDown);
    REQUIRE(Command.MouseData == XButton::XButton1);
}

TEST_CASE("BuildCommandFromAction builds XButton up command for button 4", "[Backends][SendInputHelpers]")
{
    auto Result = BuildCommandFromAction(MakeMouseButtonAction(4, false));
    REQUIRE(Result.IsOk());

    auto Command = Result.Value();
    REQUIRE(Command.Type == ESendInputCommandType::MouseButton);
    REQUIRE(Command.MouseFlags == MouseFlag::XUp);
    REQUIRE(Command.MouseData == XButton::XButton2);
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
    auto Result = BuildCommandFromAction(MakeMouseButtonAction(6, true));
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

// ── 扩展键映射 ──

TEST_CASE("MapKeyNameToVirtualKey maps editing and navigation keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("Backspace").value() == VirtualKey::VkBackspace);
    REQUIRE(MapKeyNameToVirtualKey("Delete").value()    == VirtualKey::VkDelete);
    REQUIRE(MapKeyNameToVirtualKey("Insert").value()    == VirtualKey::VkInsert);
    REQUIRE(MapKeyNameToVirtualKey("Home").value()      == VirtualKey::VkHome);
    REQUIRE(MapKeyNameToVirtualKey("End").value()       == VirtualKey::VkEnd);
    REQUIRE(MapKeyNameToVirtualKey("PageUp").value()    == VirtualKey::VkPageUp);
    REQUIRE(MapKeyNameToVirtualKey("PageDown").value()  == VirtualKey::VkPageDown);
}

TEST_CASE("MapKeyNameToVirtualKey maps left/right modifier keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("LeftShift").value()  == VirtualKey::VkLeftShift);
    REQUIRE(MapKeyNameToVirtualKey("RightShift").value() == VirtualKey::VkRightShift);
    REQUIRE(MapKeyNameToVirtualKey("LeftCtrl").value()   == VirtualKey::VkLeftControl);
    REQUIRE(MapKeyNameToVirtualKey("RightCtrl").value()  == VirtualKey::VkRightControl);
    REQUIRE(MapKeyNameToVirtualKey("LeftAlt").value()    == VirtualKey::VkLeftAlt);
    REQUIRE(MapKeyNameToVirtualKey("RightAlt").value()   == VirtualKey::VkRightAlt);
    REQUIRE(MapKeyNameToVirtualKey("LeftMeta").value()   == VirtualKey::VkLeftWin);
}

TEST_CASE("MapKeyNameToVirtualKey maps symbol keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("Minus").value()     == VirtualKey::VkOemMinus);
    REQUIRE(MapKeyNameToVirtualKey("Equal").value()     == VirtualKey::VkOemPlus);
    REQUIRE(MapKeyNameToVirtualKey("Semicolon").value() == VirtualKey::VkOem1);
    REQUIRE(MapKeyNameToVirtualKey("Backquote").value() == VirtualKey::VkOem3);
}

TEST_CASE("MapKeyNameToVirtualKey maps numpad keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(MapKeyNameToVirtualKey("Num0").value()        == VirtualKey::VkNumpad0);
    REQUIRE(MapKeyNameToVirtualKey("Num1").value()        == VirtualKey::VkNumpad1);
    REQUIRE(MapKeyNameToVirtualKey("Num9").value()        == VirtualKey::VkNumpad9);
    REQUIRE(MapKeyNameToVirtualKey("NumDivide").value()   == VirtualKey::VkDivide);
    REQUIRE(MapKeyNameToVirtualKey("NumMultiply").value() == VirtualKey::VkMultiply);
    REQUIRE(MapKeyNameToVirtualKey("NumSubtract").value() == VirtualKey::VkSubtract);
    REQUIRE(MapKeyNameToVirtualKey("NumAdd").value()      == VirtualKey::VkAdd);
    REQUIRE(MapKeyNameToVirtualKey("NumDecimal").value()  == VirtualKey::VkDecimal);
}

// ── DirectInputScanCode: DIK 扫描码策略 ──

TEST_CASE("DirectInputScanCode maps arrow keys to DIK codes",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(DirectInputScanCode(VirtualKey::VkArrowUp).value()    == 0xC8);
    REQUIRE(DirectInputScanCode(VirtualKey::VkArrowDown).value()  == 0xD0);
    REQUIRE(DirectInputScanCode(VirtualKey::VkArrowLeft).value()  == 0xCB);
    REQUIRE(DirectInputScanCode(VirtualKey::VkArrowRight).value() == 0xCD);
}

TEST_CASE("DirectInputScanCode maps navigation and modifier keys to DIK codes",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(DirectInputScanCode(VirtualKey::VkHome).value()         == 0xC7);
    REQUIRE(DirectInputScanCode(VirtualKey::VkEnd).value()          == 0xCF);
    REQUIRE(DirectInputScanCode(VirtualKey::VkPageUp).value()       == 0xC9);
    REQUIRE(DirectInputScanCode(VirtualKey::VkPageDown).value()     == 0xD1);
    REQUIRE(DirectInputScanCode(VirtualKey::VkInsert).value()       == 0xD2);
    REQUIRE(DirectInputScanCode(VirtualKey::VkDelete).value()       == 0xD3);
    REQUIRE(DirectInputScanCode(VirtualKey::VkDivide).value()       == 0xB5);
    REQUIRE(DirectInputScanCode(VirtualKey::VkRightControl).value() == 0x9D);
    REQUIRE(DirectInputScanCode(VirtualKey::VkRightAlt).value()     == 0xB8);
    REQUIRE(DirectInputScanCode(VirtualKey::VkLeftWin).value()      == 0xDB);
    REQUIRE(DirectInputScanCode(VirtualKey::VkRightWin).value()     == 0xDC);
    REQUIRE(DirectInputScanCode(VirtualKey::VkApps).value()         == 0xDD);
}

// 回归: NumLock / PrintScreen 不能一律 OR 0x80, 必须走显式特判
TEST_CASE("DirectInputScanCode handles NumLock and PrintScreen exceptions",
    "[Backends][SendInputHelpers]")
{
    // 若错误地对基础扫描码 OR 0x80, 会得到 0xC5 / 0xD4 —— 这里必须是显式 DIK 值
    REQUIRE(DirectInputScanCode(VirtualKey::VkNumLock).value()     == 0x45);  // DIK_NUMLOCK, 非 0xC5
    REQUIRE(DirectInputScanCode(VirtualKey::VkPrintScreen).value() == 0xB7);  // DIK_SYSRQ,   非 0xD4
}

// 非特判键返回 nullopt, 调用方回退到 MapVirtualKey 基础扫描码
TEST_CASE("DirectInputScanCode returns nullopt for non-special keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE_FALSE(DirectInputScanCode(VirtualKey::VkA).has_value());
    REQUIRE_FALSE(DirectInputScanCode(VirtualKey::VkSpace).has_value());
    REQUIRE_FALSE(DirectInputScanCode(VirtualKey::VkNumpad8).has_value());  // 小键盘 8, 非扩展
    REQUIRE_FALSE(DirectInputScanCode(VirtualKey::VkF1).has_value());
}

// ── ComposeKeyboardInput: 键盘 INPUT 字段组合 ──

TEST_CASE("ComposeKeyboardInput passes through vk and scan code",
    "[Backends][SendInputHelpers]")
{
    auto Fields = ComposeKeyboardInput(VirtualKey::VkA, 0x1E, false, false);
    REQUIRE(Fields.VirtualKeyCode == VirtualKey::VkA);
    REQUIRE(Fields.ScanCode == 0x1E);
}

TEST_CASE("ComposeKeyboardInput sets KeyUp flag only on key up",
    "[Backends][SendInputHelpers]")
{
    auto Down = ComposeKeyboardInput(VirtualKey::VkA, 0x1E, false, false);
    REQUIRE((Down.Flags & KeyEventFlag::KeyUp) == 0);

    auto Up = ComposeKeyboardInput(VirtualKey::VkA, 0x1E, false, true);
    REQUIRE((Up.Flags & KeyEventFlag::KeyUp) != 0);
}

// 回归: 扩展键必须带 KEYEVENTF_EXTENDEDKEY —— 防止有人误删扩展标志逻辑
TEST_CASE("ComposeKeyboardInput sets Extended flag only for extended keys",
    "[Backends][SendInputHelpers]")
{
    auto Extended = ComposeKeyboardInput(VirtualKey::VkArrowUp, 0xC8, true, false);
    REQUIRE((Extended.Flags & KeyEventFlag::Extended) != 0);

    auto NonExtended = ComposeKeyboardInput(VirtualKey::VkA, 0x1E, false, false);
    REQUIRE((NonExtended.Flags & KeyEventFlag::Extended) == 0);
}

TEST_CASE("ComposeKeyboardInput combines Extended and KeyUp flags",
    "[Backends][SendInputHelpers]")
{
    auto Fields = ComposeKeyboardInput(VirtualKey::VkArrowUp, 0xC8, true, true);
    REQUIRE((Fields.Flags & KeyEventFlag::Extended) != 0);
    REQUIRE((Fields.Flags & KeyEventFlag::KeyUp) != 0);
    // 只应包含这两个标志
    REQUIRE(Fields.Flags == (KeyEventFlag::Extended | KeyEventFlag::KeyUp));
}

// ── IsExtendedKey ──

TEST_CASE("IsExtendedKey is true for extended keys", "[Backends][SendInputHelpers]")
{
    REQUIRE(IsExtendedKey(VirtualKey::VkArrowUp));
    REQUIRE(IsExtendedKey(VirtualKey::VkArrowDown));
    REQUIRE(IsExtendedKey(VirtualKey::VkHome));
    REQUIRE(IsExtendedKey(VirtualKey::VkDelete));
    REQUIRE(IsExtendedKey(VirtualKey::VkDivide));
    REQUIRE(IsExtendedKey(VirtualKey::VkRightControl));
    REQUIRE(IsExtendedKey(VirtualKey::VkRightAlt));
    REQUIRE(IsExtendedKey(VirtualKey::VkLeftWin));
    REQUIRE(IsExtendedKey(VirtualKey::VkRightWin));
    REQUIRE(IsExtendedKey(VirtualKey::VkApps));
    REQUIRE(IsExtendedKey(VirtualKey::VkNumLock));
    REQUIRE(IsExtendedKey(VirtualKey::VkPrintScreen));
}

TEST_CASE("IsExtendedKey is false for non-extended keys", "[Backends][SendInputHelpers]")
{
    REQUIRE_FALSE(IsExtendedKey(VirtualKey::VkA));
    REQUIRE_FALSE(IsExtendedKey(VirtualKey::VkSpace));
    REQUIRE_FALSE(IsExtendedKey(VirtualKey::VkNumpad8));  // 小键盘 8, 非扩展
    REQUIRE_FALSE(IsExtendedKey(VirtualKey::VkF1));
    REQUIRE_FALSE(IsExtendedKey(VirtualKey::VkLeftControl));
}

// ── ResolveScanCode: DIK 表 + 基础码回退 seam ──

// fake 基础扫描码解析器：返回一个可辨识的常量，用于验证是否走了回退分支
static uint32 FakeBaseScanCode(uint32 /*VirtualKeyCode*/) { return 0xAB; }

TEST_CASE("ResolveScanCode uses DIK table for extended keys and ignores base resolver",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(ResolveScanCode(VirtualKey::VkArrowUp, &FakeBaseScanCode)   == 0xC8);
    REQUIRE(ResolveScanCode(VirtualKey::VkNumLock, &FakeBaseScanCode)   == 0x45);
    REQUIRE(ResolveScanCode(VirtualKey::VkPrintScreen, &FakeBaseScanCode) == 0xB7);
}

TEST_CASE("ResolveScanCode falls back to base resolver for non-special keys",
    "[Backends][SendInputHelpers]")
{
    REQUIRE(ResolveScanCode(VirtualKey::VkA, &FakeBaseScanCode)      == 0xAB);
    REQUIRE(ResolveScanCode(VirtualKey::VkNumpad8, &FakeBaseScanCode) == 0xAB);
}

// ── BuildResolvedInput: 命令 -> 平台无关字段 ──

static SSendInputCommand MakeKeyboardCommand(uint32 Vk, bool bKeyUp)
{
    SSendInputCommand Command;
    Command.Type = ESendInputCommandType::Keyboard;
    Command.VirtualKeyCode = Vk;
    Command.bKeyUp = bKeyUp;
    return Command;
}

TEST_CASE("BuildResolvedInput resolves extended key down with DIK scan and Extended flag",
    "[Backends][SendInputHelpers]")
{
    auto ResolvedOpt = BuildResolvedInput(MakeKeyboardCommand(VirtualKey::VkArrowUp, false), &FakeBaseScanCode);
    REQUIRE(ResolvedOpt.has_value());
    const auto& Resolved = ResolvedOpt.value();
    REQUIRE(Resolved.Kind == EResolvedInputKind::Keyboard);
    REQUIRE(Resolved.VirtualKeyCode == VirtualKey::VkArrowUp);
    REQUIRE(Resolved.ScanCode == 0xC8);  // DIK_UP
    REQUIRE((Resolved.KeyFlags & KeyEventFlag::Extended) != 0);
    REQUIRE((Resolved.KeyFlags & KeyEventFlag::KeyUp) == 0);
}

TEST_CASE("BuildResolvedInput resolves non-special key up with base scan and KeyUp flag",
    "[Backends][SendInputHelpers]")
{
    auto ResolvedOpt = BuildResolvedInput(MakeKeyboardCommand(VirtualKey::VkA, true), &FakeBaseScanCode);
    REQUIRE(ResolvedOpt.has_value());
    const auto& Resolved = ResolvedOpt.value();
    REQUIRE(Resolved.Kind == EResolvedInputKind::Keyboard);
    REQUIRE(Resolved.VirtualKeyCode == VirtualKey::VkA);
    REQUIRE(Resolved.ScanCode == 0xAB);  // 回退到 base resolver
    REQUIRE((Resolved.KeyFlags & KeyEventFlag::Extended) == 0);
    REQUIRE((Resolved.KeyFlags & KeyEventFlag::KeyUp) != 0);
}

TEST_CASE("BuildResolvedInput passes through mouse button fields",
    "[Backends][SendInputHelpers]")
{
    SSendInputCommand Command;
    Command.Type = ESendInputCommandType::MouseButton;
    Command.MouseFlags = MouseFlag::XDown;
    Command.MouseData = XButton::XButton1;

    auto ResolvedOpt = BuildResolvedInput(Command, &FakeBaseScanCode);
    REQUIRE(ResolvedOpt.has_value());
    const auto& Resolved = ResolvedOpt.value();
    REQUIRE(Resolved.Kind == EResolvedInputKind::MouseButton);
    REQUIRE(Resolved.MouseFlags == MouseFlag::XDown);
    REQUIRE(Resolved.MouseData == XButton::XButton1);
}

TEST_CASE("BuildResolvedInput passes through mouse move fields",
    "[Backends][SendInputHelpers]")
{
    SSendInputCommand Command;
    Command.Type = ESendInputCommandType::MouseMove;
    Command.MouseFlags = MouseFlag::Move;
    Command.DeltaX = 12;
    Command.DeltaY = -7;

    auto ResolvedOpt = BuildResolvedInput(Command, &FakeBaseScanCode);
    REQUIRE(ResolvedOpt.has_value());
    const auto& Resolved = ResolvedOpt.value();
    REQUIRE(Resolved.Kind == EResolvedInputKind::MouseMove);
    REQUIRE(Resolved.MouseFlags == MouseFlag::Move);
    REQUIRE(Resolved.DeltaX == 12);
    REQUIRE(Resolved.DeltaY == -7);
}

TEST_CASE("BuildResolvedInput passes through mouse wheel fields",
    "[Backends][SendInputHelpers]")
{
    SSendInputCommand Command;
    Command.Type = ESendInputCommandType::MouseWheel;
    Command.MouseFlags = MouseFlag::Wheel;
    Command.WheelDelta = WheelDeltaUnit;

    auto ResolvedOpt = BuildResolvedInput(Command, &FakeBaseScanCode);
    REQUIRE(ResolvedOpt.has_value());
    const auto& Resolved = ResolvedOpt.value();
    REQUIRE(Resolved.Kind == EResolvedInputKind::MouseWheel);
    REQUIRE(Resolved.MouseFlags == MouseFlag::Wheel);
    REQUIRE(Resolved.WheelDelta == WheelDeltaUnit);
}

// 防御性: 非法命令类型返回 nullopt（正常路径经 BuildCommandFromAction 不会触发）
TEST_CASE("BuildResolvedInput returns nullopt for invalid command type",
    "[Backends][SendInputHelpers]")
{
    SSendInputCommand Command;
    Command.Type = static_cast<ESendInputCommandType>(999);

    auto ResolvedOpt = BuildResolvedInput(Command, &FakeBaseScanCode);
    REQUIRE_FALSE(ResolvedOpt.has_value());
}
