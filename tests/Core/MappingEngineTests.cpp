// ZMappingEngine 单元测试。
// 验证最小映射行为：Button/Trigger/Axis2D 到 Keyboard/Mouse/MouseMove，
// 以及 disabled profile/rule、payload 不匹配、Hold/MouseWheel 暂不实现等边界情况。

#include <catch2/catch_test_macros.hpp>

#include "Core/ControlId.h"
#include "Core/MappingEngine.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SInputEvent MakeButtonEvent(StdStringView ControlId, EInputEventType EventType)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = "dev_1"};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EventType;
    Event.Value = (EventType == EInputEventType::Pressed) ? 1.0f : 0.0f;
    return Event;
}

static SInputEvent MakeTriggerEvent(StdStringView ControlId, float32 Value)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = "dev_1"};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Trigger;
    Event.EventType = EInputEventType::Changed;
    Event.Value = Value;
    return Event;
}

static SInputEvent MakeAxis2DEvent(StdStringView ControlId, float32 X, float32 Y)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = "dev_1"};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = SAxis2DValue{.X = X, .Y = Y};
    return Event;
}

static SMappingRule MakeButtonToKeyRule(
    const StdString& RuleId,
    StdStringView ControlId,
    const StdString& Key)
{
    SMappingRule Rule;
    Rule.Id = RuleId;
    Rule.Input.ControlId = StdString(ControlId);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = Key, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

static SMappingRule MakeButtonToMouseRule(
    const StdString& RuleId,
    StdStringView ControlId,
    int32 Button)
{
    SMappingRule Rule;
    Rule.Id = RuleId;
    Rule.Input.ControlId = StdString(ControlId);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::MouseButton;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = Button, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

static SMappingRule MakeTriggerToKeyRule(
    const StdString& RuleId,
    StdStringView ControlId,
    const StdString& Key,
    float32 Threshold)
{
    SMappingRule Rule;
    Rule.Id = RuleId;
    Rule.Input.ControlId = StdString(ControlId);
    Rule.Input.ControlType = EInputControlType::Trigger;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Threshold = Threshold;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = Key, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

static SMappingRule MakeAxis2DToMouseMoveRule(
    const StdString& RuleId,
    StdStringView ControlId,
    float32 Deadzone,
    float32 Sensitivity)
{
    SMappingRule Rule;
    Rule.Id = RuleId;
    Rule.Input.ControlId = StdString(ControlId);
    Rule.Input.ControlType = EInputControlType::Axis2D;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Deadzone = Deadzone;
    Rule.Output.Action.Type = EActionType::MouseMove;
    Rule.Output.Action.Payload = SMouseMoveAction{};
    Rule.Output.Mode = EMappingActionMode::Analog;
    Rule.Output.Sensitivity = Sensitivity;
    return Rule;
}

static SMappingProfile MakeProfile(TVector<SMappingRule> Rules)
{
    SMappingProfile Profile;
    Profile.Id = "test_profile";
    Profile.Name = "Test";
    Profile.Rules = std::move(Rules);
    return Profile;
}

// ── 空/禁用 ──

TEST_CASE("MappingEngine empty profile returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);
    auto Profile = MakeProfile({});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine disabled profile returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);
    auto Profile = MakeProfile({MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space")});
    Profile.bEnabled = false;

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine disabled rule is skipped", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);
    auto Rule = MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space");
    Rule.bEnabled = false;
    auto Profile = MakeProfile({Rule});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

// ── 不匹配 ──

TEST_CASE("MappingEngine control id mismatch returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonNorth, EInputEventType::Pressed);
    auto Profile = MakeProfile({MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space")});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine control type mismatch returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    // 事件是 Trigger 类型但规则期望 Button
    auto Event = MakeTriggerEvent(ControlId::ButtonSouth, 1.0f);
    auto Profile = MakeProfile({MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space")});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

// ── Button -> Keyboard ──

TEST_CASE("MappingEngine Button Pressed outputs pressed keyboard action", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);
    auto Profile = MakeProfile({MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space")});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 1);
    REQUIRE(Actions[0].Type == EActionType::KeyboardKey);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.Key == "Space");
    REQUIRE(Payload.bPressed == true);
}

TEST_CASE("MappingEngine Button Released outputs released keyboard action", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Released);
    auto Profile = MakeProfile({MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space")});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 1);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.Key == "Space");
    REQUIRE(Payload.bPressed == false);
}

// ── Button -> MouseButton ──

TEST_CASE("MappingEngine Button to MouseButton outputs correct state", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Profile = MakeProfile({MakeButtonToMouseRule("r1", ControlId::ButtonWest, 0)});

    auto PressedActions = Engine.MapInput(
        MakeButtonEvent(ControlId::ButtonWest, EInputEventType::Pressed), Profile);
    auto ReleasedActions = Engine.MapInput(
        MakeButtonEvent(ControlId::ButtonWest, EInputEventType::Released), Profile);

    REQUIRE(PressedActions.size() == 1);
    REQUIRE(std::get<SMouseButtonAction>(PressedActions[0].Payload).bPressed == true);

    REQUIRE(ReleasedActions.size() == 1);
    REQUIRE(std::get<SMouseButtonAction>(ReleasedActions[0].Payload).bPressed == false);
}

// ── 多规则匹配 ──

TEST_CASE("MappingEngine multiple matching rules output in profile order", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);
    auto Profile = MakeProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
        MakeButtonToKeyRule("r2", ControlId::ButtonSouth, "Enter"),
    });

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 2);
    REQUIRE(std::get<SKeyboardAction>(Actions[0].Payload).Key == "Space");
    REQUIRE(std::get<SKeyboardAction>(Actions[1].Payload).Key == "Enter");
}

// ── Trigger 阈值 ──

TEST_CASE("MappingEngine Trigger below threshold outputs released action", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeTriggerEvent(ControlId::RightTrigger, 0.3f);
    auto Profile = MakeProfile({MakeTriggerToKeyRule("r1", ControlId::RightTrigger, "W", 0.5f)});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 1);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.bPressed == false);
}

TEST_CASE("MappingEngine Trigger above threshold outputs pressed action", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeTriggerEvent(ControlId::RightTrigger, 0.8f);
    auto Profile = MakeProfile({MakeTriggerToKeyRule("r1", ControlId::RightTrigger, "W", 0.5f)});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 1);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.bPressed == true);
}

// ── Axis2D -> MouseMove ──

TEST_CASE("MappingEngine Axis2D inside deadzone outputs no action", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeAxis2DEvent(ControlId::RightStick, 0.05f, 0.05f);
    auto Profile = MakeProfile(
        {MakeAxis2DToMouseMoveRule("r1", ControlId::RightStick, 0.15f, 2.0f)});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine Axis2D outside deadzone outputs scaled MouseMove", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeAxis2DEvent(ControlId::RightStick, 0.5f, -0.3f);
    auto Profile = MakeProfile(
        {MakeAxis2DToMouseMoveRule("r1", ControlId::RightStick, 0.1f, 2.0f)});

    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.size() == 1);
    REQUIRE(Actions[0].Type == EActionType::MouseMove);
    auto& Payload = std::get<SMouseMoveAction>(Actions[0].Payload);
    REQUIRE(Payload.DeltaX == 0.5f * 2.0f);
    REQUIRE(Payload.DeltaY == -0.3f * 2.0f);
}

// ── Payload 不匹配 ──

TEST_CASE("MappingEngine action type payload mismatch skips rule", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);

    // ActionType 是 KeyboardKey 但 Payload 是 SMouseButtonAction
    SMappingRule Rule;
    Rule.Id = "bad_rule";
    Rule.Input.ControlId = StdString(ControlId::ButtonSouth);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;

    auto Profile = MakeProfile({Rule});
    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine Axis2D MouseMove payload mismatch skips rule", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeAxis2DEvent(ControlId::RightStick, 0.5f, -0.3f);

    // ActionType 是 MouseMove 但 Payload 是 SKeyboardAction
    SMappingRule Rule;
    Rule.Id = "bad_analog";
    Rule.Input.ControlId = StdString(ControlId::RightStick);
    Rule.Input.ControlType = EInputControlType::Axis2D;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Deadzone = 0.0f;
    Rule.Output.Action.Type = EActionType::MouseMove;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "W", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::Analog;
    Rule.Output.Sensitivity = 1.0f;

    auto Profile = MakeProfile({Rule});
    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

// ── 暂未实现的 mode/type ──

TEST_CASE("MappingEngine Hold mode returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);

    SMappingRule Rule = MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space");
    Rule.Output.Mode = EMappingActionMode::Hold;

    auto Profile = MakeProfile({Rule});
    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

TEST_CASE("MappingEngine MouseWheel returns no actions", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    auto Event = MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed);

    SMappingRule Rule;
    Rule.Id = "wheel_rule";
    Rule.Input.ControlId = StdString(ControlId::ButtonSouth);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Output.Action.Type = EActionType::MouseWheel;
    Rule.Output.Action.Payload = SMouseWheelAction{.Delta = 1.0f};
    Rule.Output.Mode = EMappingActionMode::PressRelease;

    auto Profile = MakeProfile({Rule});
    auto Actions = Engine.MapInput(Event, Profile);

    REQUIRE(Actions.empty());
}

// ── 头文件独立 include ──

TEST_CASE("MappingEngine header is self-contained", "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    SMappingProfile Profile;
    auto Actions = Engine.MapInput(SInputEvent{}, Profile);
    REQUIRE(Actions.empty());
}

// ── 方向虚拟输入映射 ──

TEST_CASE("MappingEngine stick direction button maps to keyboard press/release",
    "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    SMappingProfile Profile;
    Profile.bEnabled = true;
    Profile.Rules.push_back(
        MakeButtonToKeyRule("r1", ControlId::LeftStickUp, "W"));

    // pressed
    auto PressActions = Engine.MapInput(
        MakeButtonEvent(ControlId::LeftStickUp, EInputEventType::Pressed), Profile);
    REQUIRE(PressActions.size() == 1);
    REQUIRE(PressActions[0].Type == EActionType::KeyboardKey);
    auto* PressPayload = std::get_if<SKeyboardAction>(&PressActions[0].Payload);
    REQUIRE(PressPayload != nullptr);
    REQUIRE(PressPayload->Key == "W");
    REQUIRE(PressPayload->bPressed == true);

    // released
    auto ReleaseActions = Engine.MapInput(
        MakeButtonEvent(ControlId::LeftStickUp, EInputEventType::Released), Profile);
    REQUIRE(ReleaseActions.size() == 1);
    auto* ReleasePayload = std::get_if<SKeyboardAction>(&ReleaseActions[0].Payload);
    REQUIRE(ReleasePayload != nullptr);
    REQUIRE(ReleasePayload->bPressed == false);
}

TEST_CASE("MappingEngine stick direction button maps to mouse button",
    "[Core][MappingEngine]")
{
    ZMappingEngine Engine;
    SMappingProfile Profile;
    Profile.bEnabled = true;
    Profile.Rules.push_back(
        MakeButtonToMouseRule("r1", ControlId::RightStickRight, 1));

    auto PressActions = Engine.MapInput(
        MakeButtonEvent(ControlId::RightStickRight, EInputEventType::Pressed), Profile);
    REQUIRE(PressActions.size() == 1);
    REQUIRE(PressActions[0].Type == EActionType::MouseButton);
    auto* Payload = std::get_if<SMouseButtonAction>(&PressActions[0].Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Button == 1);
    REQUIRE(Payload->bPressed == true);
}
