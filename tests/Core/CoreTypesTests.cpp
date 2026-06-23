// Core 模块基础类型编译和默认值测试。
// 验证所有 Core 头文件可正常包含编译，以及数据结构的默认值符合预期。

#include <catch2/catch_test_macros.hpp>

#include "Core/Action.h"
#include "Core/ControlId.h"
#include "Core/DeviceId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

using namespace MappyZ;

// ── SDeviceId ──

TEST_CASE("SDeviceId equality comparison", "[Core][DeviceId]")
{
    SDeviceId IdA{.Value = "device_001"};
    SDeviceId IdB{.Value = "device_001"};
    SDeviceId IdC{.Value = "device_002"};

    REQUIRE(IdA == IdB);
    REQUIRE(IdA != IdC);
}

TEST_CASE("SDeviceInfo default construction", "[Core][DeviceId]")
{
    SDeviceInfo Info;

    REQUIRE(Info.Id.Value.empty());
    REQUIRE(Info.Name.empty());
    REQUIRE(Info.Backend.empty());
    REQUIRE(Info.VendorId.empty());
    REQUIRE(Info.ProductId.empty());
    REQUIRE(Info.Guid.empty());
    REQUIRE(Info.InstanceId.empty());
}

TEST_CASE("SDeviceInfo field assignment", "[Core][DeviceId]")
{
    SDeviceInfo Info;
    Info.Id = SDeviceId{.Value = "sdl3_0"};
    Info.Name = "Xbox Wireless Controller";
    Info.Backend = "sdl3";
    Info.VendorId = "045e";
    Info.ProductId = "02fd";

    REQUIRE(Info.Id.Value == "sdl3_0");
    REQUIRE(Info.Name == "Xbox Wireless Controller");
    REQUIRE(Info.Backend == "sdl3");
    REQUIRE(Info.VendorId == "045e");
    REQUIRE(Info.ProductId == "02fd");
}

// ── ControlId ──

TEST_CASE("ControlId constants are non-empty snake_case", "[Core][ControlId]")
{
    REQUIRE_FALSE(ControlId::ButtonSouth.empty());
    REQUIRE(ControlId::ButtonSouth == "button_south");
    REQUIRE(ControlId::RightTrigger == "right_trigger");
    REQUIRE(ControlId::LeftStick == "left_stick");
    REQUIRE(ControlId::DpadUp == "dpad_up");
}

// ── SInputEvent ──

TEST_CASE("SInputEvent default values", "[Core][InputEvent]")
{
    SInputEvent Event;

    REQUIRE(Event.DeviceId.Value.empty());
    REQUIRE(Event.ControlId.empty());
    REQUIRE(Event.ControlType == EInputControlType::Button);
    REQUIRE(Event.EventType == EInputEventType::Changed);
    REQUIRE(Event.Value == 0.0f);
    REQUIRE(Event.Axis2D.X == 0.0f);
    REQUIRE(Event.Axis2D.Y == 0.0f);
}

TEST_CASE("SInputEvent button pressed", "[Core][InputEvent]")
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = "sdl3_0"};
    Event.ControlId = StdString(ControlId::ButtonSouth);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EInputEventType::Pressed;
    Event.Value = 1.0f;
    Event.Timestamp = std::chrono::steady_clock::now();

    REQUIRE(Event.ControlId == "button_south");
    REQUIRE(Event.EventType == EInputEventType::Pressed);
    REQUIRE(Event.Value == 1.0f);
}

TEST_CASE("SInputEvent stick Axis2D", "[Core][InputEvent]")
{
    SInputEvent Event;
    Event.ControlId = StdString(ControlId::RightStick);
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = SAxis2DValue{.X = 0.75f, .Y = -0.5f};

    REQUIRE(Event.ControlType == EInputControlType::Axis2D);
    REQUIRE(Event.Axis2D.X == 0.75f);
    REQUIRE(Event.Axis2D.Y == -0.5f);
}

TEST_CASE("SInputEvent trigger value", "[Core][InputEvent]")
{
    SInputEvent Event;
    Event.ControlId = StdString(ControlId::RightTrigger);
    Event.ControlType = EInputControlType::Trigger;
    Event.EventType = EInputEventType::Changed;
    Event.Value = 0.85f;

    REQUIRE(Event.ControlType == EInputControlType::Trigger);
    REQUIRE(Event.Value == 0.85f);
}

// ── SAction ──

TEST_CASE("SAction default values", "[Core][Action]")
{
    SAction Action;

    REQUIRE(Action.Type == EActionType::None);
    REQUIRE(std::holds_alternative<std::monostate>(Action.Payload));
}

TEST_CASE("SAction keyboard action", "[Core][Action]")
{
    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SKeyboardAction{.Key = "Space", .bPressed = true};

    REQUIRE(Action.Type == EActionType::KeyboardKey);
    REQUIRE(std::holds_alternative<SKeyboardAction>(Action.Payload));

    const auto& Keyboard = std::get<SKeyboardAction>(Action.Payload);
    REQUIRE(Keyboard.Key == "Space");
    REQUIRE(Keyboard.bPressed == true);
}

TEST_CASE("SAction mouse button action", "[Core][Action]")
{
    SAction Action;
    Action.Type = EActionType::MouseButton;
    Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};

    REQUIRE(std::holds_alternative<SMouseButtonAction>(Action.Payload));

    const auto& Mouse = std::get<SMouseButtonAction>(Action.Payload);
    REQUIRE(Mouse.Button == 0);
    REQUIRE(Mouse.bPressed == true);
}

TEST_CASE("SAction mouse move action", "[Core][Action]")
{
    SAction Action;
    Action.Type = EActionType::MouseMove;
    Action.Payload = SMouseMoveAction{.DeltaX = 10.5f, .DeltaY = -3.2f};

    REQUIRE(std::holds_alternative<SMouseMoveAction>(Action.Payload));

    const auto& Move = std::get<SMouseMoveAction>(Action.Payload);
    REQUIRE(Move.DeltaX == 10.5f);
    REQUIRE(Move.DeltaY == -3.2f);
}
