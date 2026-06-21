// 映射 Profile 数据契约单元测试。
// 验证默认值、典型规则构造和头文件独立 include。

#include <catch2/catch_test_macros.hpp>

#include "Core/ControlId.h"
#include "Core/MappingProfile.h"

using namespace ZeroMapper;

// ── 默认值 ──

TEST_CASE("SMappingInput default values", "[Core][MappingProfile]")
{
    SMappingInput Input;

    REQUIRE(Input.ControlId.empty());
    REQUIRE(Input.ControlType == EInputControlType::Button);
    REQUIRE(Input.EventType == EInputEventType::Pressed);
    REQUIRE(Input.Threshold == 0.5f);
    REQUIRE(Input.Deadzone == 0.0f);
}

TEST_CASE("SMappingOutput default values", "[Core][MappingProfile]")
{
    SMappingOutput Output;

    REQUIRE(Output.Action.Type == EActionType::None);
    REQUIRE(Output.Mode == EMappingActionMode::PressRelease);
    REQUIRE(Output.Sensitivity == 1.0f);
}

TEST_CASE("SMappingRule default is enabled with default input and output", "[Core][MappingProfile]")
{
    SMappingRule Rule;

    REQUIRE(Rule.Id.empty());
    REQUIRE(Rule.DisplayName.empty());
    REQUIRE(Rule.bEnabled == true);
    REQUIRE(Rule.Input.ControlId.empty());
    REQUIRE(Rule.Output.Action.Type == EActionType::None);
}

TEST_CASE("SDeviceMatch default construction is all empty", "[Core][MappingProfile]")
{
    SDeviceMatch Match;

    REQUIRE(Match.Name.empty());
    REQUIRE(Match.Backend.empty());
    REQUIRE(Match.VendorId.empty());
    REQUIRE(Match.ProductId.empty());
    REQUIRE(Match.Guid.empty());
    REQUIRE(Match.InstanceId.empty());
}

TEST_CASE("SMappingProfile default SchemaVersion is 1 and enabled", "[Core][MappingProfile]")
{
    SMappingProfile Profile;

    REQUIRE(Profile.SchemaVersion == 1);
    REQUIRE(Profile.Id.empty());
    REQUIRE(Profile.Name.empty());
    REQUIRE(Profile.bEnabled == true);
    REQUIRE(Profile.Rules.empty());
}

TEST_CASE("SMappingProfile empty rules is valid", "[Core][MappingProfile]")
{
    SMappingProfile Profile;
    Profile.Id = "empty_profile";
    Profile.Name = "Empty";

    REQUIRE(Profile.Rules.empty());
    REQUIRE(Profile.bEnabled == true);
}

// ── 典型规则构造 ──

TEST_CASE("Button to Keyboard rule construction", "[Core][MappingProfile]")
{
    SMappingRule Rule;
    Rule.Id = "btn_south_to_space";
    Rule.DisplayName = "A -> Space";
    Rule.Input.ControlId = StdString(ControlId::ButtonSouth);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;

    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "Space", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;

    REQUIRE(Rule.bEnabled == true);
    REQUIRE(Rule.Input.ControlId == ControlId::ButtonSouth);
    REQUIRE(Rule.Output.Action.Type == EActionType::KeyboardKey);
    REQUIRE(std::get<SKeyboardAction>(Rule.Output.Action.Payload).Key == "Space");
}

TEST_CASE("Button to MouseButton rule construction", "[Core][MappingProfile]")
{
    SMappingRule Rule;
    Rule.Id = "btn_west_to_lmb";
    Rule.Input.ControlId = StdString(ControlId::ButtonWest);
    Rule.Input.ControlType = EInputControlType::Button;

    Rule.Output.Action.Type = EActionType::MouseButton;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;

    REQUIRE(Rule.Output.Action.Type == EActionType::MouseButton);
    REQUIRE(std::get<SMouseButtonAction>(Rule.Output.Action.Payload).Button == 0);
}

TEST_CASE("Axis2D to MouseMove rule with deadzone and sensitivity", "[Core][MappingProfile]")
{
    SMappingRule Rule;
    Rule.Id = "rstick_to_mouse";
    Rule.Input.ControlId = StdString(ControlId::RightStick);
    Rule.Input.ControlType = EInputControlType::Axis2D;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Deadzone = 0.15f;

    Rule.Output.Action.Type = EActionType::MouseMove;
    Rule.Output.Action.Payload = SMouseMoveAction{};
    Rule.Output.Mode = EMappingActionMode::Analog;
    Rule.Output.Sensitivity = 2.5f;

    REQUIRE(Rule.Input.Deadzone == 0.15f);
    REQUIRE(Rule.Output.Mode == EMappingActionMode::Analog);
    REQUIRE(Rule.Output.Sensitivity == 2.5f);
}

TEST_CASE("Trigger threshold rule construction", "[Core][MappingProfile]")
{
    SMappingRule Rule;
    Rule.Id = "rtrigger_to_lmb";
    Rule.Input.ControlId = StdString(ControlId::RightTrigger);
    Rule.Input.ControlType = EInputControlType::Trigger;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Threshold = 0.3f;

    Rule.Output.Action.Type = EActionType::MouseButton;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};

    REQUIRE(Rule.Input.Threshold == 0.3f);
    REQUIRE(Rule.Input.ControlType == EInputControlType::Trigger);
}

// ── 头文件独立 include ──

TEST_CASE("MappingProfile header is self-contained", "[Core][MappingProfile]")
{
    // 此测试的编译通过即验证 MappingProfile.h 可独立 include
    SMappingProfile Profile;
    Profile.Id = "compile_check";
    REQUIRE_FALSE(Profile.Id.empty());
}
