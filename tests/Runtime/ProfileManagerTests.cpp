// ZProfileManager 单元测试。
// 验证 JSON 解析、序列化、round-trip 和文件 I/O 行为。

#include <catch2/catch_test_macros.hpp>

#include "Runtime/ProfileManager.h"

#include <filesystem>
#include <fstream>

using namespace MappyZ;

// ── 解析最小 profile ──

TEST_CASE("ProfileManager parses minimal profile", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({"schema_version": 1})");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    REQUIRE(Profile.SchemaVersion == 1);
    REQUIRE(Profile.Id.empty());
    REQUIRE(Profile.Name.empty());
    REQUIRE(Profile.bEnabled == true);
    REQUIRE(Profile.Rules.empty());
}

// ── Button -> Keyboard ──

TEST_CASE("ProfileManager parses Button to Keyboard profile", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "profile_id": "test_1",
        "profile_name": "Test Profile",
        "mappings": [{
            "id": "r1",
            "display_name": "Jump",
            "enabled": true,
            "input": {
                "control_id": "button_south",
                "control_type": "button",
                "event": "pressed"
            },
            "action": {
                "type": "keyboard_key",
                "mode": "press_release",
                "keyboard_key": { "key": "Space" }
            }
        }]
    })");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    REQUIRE(Profile.Id == "test_1");
    REQUIRE(Profile.Name == "Test Profile");
    REQUIRE(Profile.Rules.size() == 1);

    auto& Rule = Profile.Rules[0];
    REQUIRE(Rule.Id == "r1");
    REQUIRE(Rule.DisplayName == "Jump");
    REQUIRE(Rule.Input.ControlId == "button_south");
    REQUIRE(Rule.Input.ControlType == EInputControlType::Button);
    REQUIRE(Rule.Input.EventType == EInputEventType::Pressed);
    REQUIRE(Rule.Output.Action.Type == EActionType::KeyboardKey);
    REQUIRE(Rule.Output.Mode == EMappingActionMode::PressRelease);

    auto* Payload = std::get_if<SKeyboardAction>(&Rule.Output.Action.Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Key == "Space");
}

// ── Button -> MouseButton ──

TEST_CASE("ProfileManager parses Button to MouseButton profile", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": {
                "control_id": "button_west",
                "control_type": "button",
                "event": "pressed"
            },
            "action": {
                "type": "mouse_button",
                "mode": "press_release",
                "mouse_button": { "button": "right" }
            }
        }]
    })");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    auto* Payload = std::get_if<SMouseButtonAction>(&Profile.Rules[0].Output.Action.Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Button == 1);
}

// ── Trigger threshold -> MouseButton ──

TEST_CASE("ProfileManager parses Trigger threshold profile", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": {
                "control_id": "right_trigger",
                "control_type": "trigger",
                "event": "changed",
                "threshold": 0.7
            },
            "action": {
                "type": "mouse_button",
                "mode": "press_release",
                "mouse_button": { "button": "left" }
            }
        }]
    })");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    REQUIRE(Profile.Rules[0].Input.ControlType == EInputControlType::Trigger);
    REQUIRE(Profile.Rules[0].Input.Threshold == 0.7f);
}

// ── Axis2D -> MouseMove ──

TEST_CASE("ProfileManager parses Axis2D MouseMove profile", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": {
                "control_id": "right_stick",
                "control_type": "axis2d",
                "event": "changed",
                "deadzone": 0.15
            },
            "action": {
                "type": "mouse_move",
                "mode": "analog",
                "mouse_move": { "sensitivity": 2.5 }
            }
        }]
    })");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    REQUIRE(Profile.Rules[0].Input.ControlType == EInputControlType::Axis2D);
    REQUIRE(Profile.Rules[0].Input.Deadzone == 0.15f);
    REQUIRE(Profile.Rules[0].Output.Mode == EMappingActionMode::Analog);
    REQUIRE(Profile.Rules[0].Output.Sensitivity == 2.5f);
}

// ── disabled profile 和 disabled rule ──

TEST_CASE("ProfileManager parses disabled profile and rule", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "settings": { "enabled": false },
        "mappings": [{
            "id": "r1",
            "enabled": false,
            "input": { "control_id": "button_south", "control_type": "button" },
            "action": { "type": "keyboard_key", "mode": "press_release", "keyboard_key": { "key": "A" } }
        }]
    })");

    REQUIRE(Result.IsOk());
    auto Profile = std::move(Result).TakeValue();
    REQUIRE(Profile.bEnabled == false);
    REQUIRE(Profile.Rules[0].bEnabled == false);
}

// ── 错误路径 ──

TEST_CASE("ProfileManager rejects invalid JSON syntax", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson("{not valid json");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::ParseFailed);
}

TEST_CASE("ProfileManager rejects missing schema_version", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({"profile_id": "x"})");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::InvalidManifest);
}

TEST_CASE("ProfileManager rejects unsupported schema_version", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({"schema_version": 99})");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::InvalidManifest);
    REQUIRE(Result.Failure().Message.find("99") != StdString::npos);
}

TEST_CASE("ProfileManager rejects non-integer schema_version", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({"schema_version": "one"})");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::InvalidManifest);
}

TEST_CASE("ProfileManager rejects non-string control_type", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": { "control_id": "x", "control_type": 42 },
            "action": { "type": "keyboard_key", "mode": "press_release", "keyboard_key": { "key": "A" } }
        }]
    })");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::InvalidManifest);
}

TEST_CASE("ProfileManager rejects unknown control type", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.ParseProfileJson(R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": { "control_id": "x", "control_type": "unknown_type" },
            "action": { "type": "keyboard_key", "mode": "press_release", "keyboard_key": { "key": "A" } }
        }]
    })");

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::InvalidManifest);
    REQUIRE(Result.Failure().Message.find("unknown_type") != StdString::npos);
}

// ── Round-trip: Serialize 然后 Parse ──

TEST_CASE("ProfileManager serialize then parse round-trips", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;

    SMappingProfile Original;
    Original.Id = "roundtrip_test";
    Original.Name = "Round Trip";
    Original.bEnabled = true;
    Original.DeviceMatch.Name = "Xbox Controller";
    Original.DeviceMatch.VendorId = "045e";

    SMappingRule Rule;
    Rule.Id = "r1";
    Rule.DisplayName = "Fire";
    Rule.Input.ControlId = "right_trigger";
    Rule.Input.ControlType = EInputControlType::Trigger;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Threshold = 0.6f;
    Rule.Output.Action.Type = EActionType::MouseButton;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    Original.Rules.push_back(Rule);

    auto SerResult = Manager.SerializeProfileJson(Original);
    REQUIRE(SerResult.IsOk());

    auto ParseResult = Manager.ParseProfileJson(SerResult.Value());
    REQUIRE(ParseResult.IsOk());

    auto Parsed = std::move(ParseResult).TakeValue();
    REQUIRE(Parsed.Id == Original.Id);
    REQUIRE(Parsed.Name == Original.Name);
    REQUIRE(Parsed.bEnabled == Original.bEnabled);
    REQUIRE(Parsed.DeviceMatch.Name == "Xbox Controller");
    REQUIRE(Parsed.DeviceMatch.VendorId == "045e");
    REQUIRE(Parsed.Rules.size() == 1);
    REQUIRE(Parsed.Rules[0].Id == "r1");
    REQUIRE(Parsed.Rules[0].Input.ControlType == EInputControlType::Trigger);
    REQUIRE(Parsed.Rules[0].Input.Threshold == 0.6f);

    auto* Payload = std::get_if<SMouseButtonAction>(&Parsed.Rules[0].Output.Action.Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Button == 0);
}

// ── 文件 I/O ──

TEST_CASE("ProfileManager LoadProfile file not found returns FileNotFound", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;
    auto Result = Manager.LoadProfile(StdPath("nonexistent_profile_xyz.json"));

    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::FileNotFound);
}

TEST_CASE("ProfileManager SaveProfile then LoadProfile round-trips", "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;

    SMappingProfile Original;
    Original.Id = "file_test";
    Original.Name = "File Test";

    SMappingRule Rule;
    Rule.Id = "r1";
    Rule.Input.ControlId = "button_south";
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "Enter", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    Original.Rules.push_back(Rule);

    auto TempPath = std::filesystem::temp_directory_path() / "mappyz_test_profile.json";

    auto SaveResult = Manager.SaveProfile(Original, TempPath);
    REQUIRE(SaveResult.IsOk());

    auto LoadResult = Manager.LoadProfile(TempPath);
    REQUIRE(LoadResult.IsOk());

    auto Loaded = std::move(LoadResult).TakeValue();
    REQUIRE(Loaded.Id == "file_test");
    REQUIRE(Loaded.Rules.size() == 1);
    REQUIRE(Loaded.Rules[0].Input.ControlId == "button_south");

    auto* Payload = std::get_if<SKeyboardAction>(&Loaded.Rules[0].Output.Action.Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Key == "Enter");

    // 清理临时文件
    std::filesystem::remove(TempPath);
}

// ── 方向规则 round trip ──

TEST_CASE("ProfileManager direction rule serializes and parses correctly",
    "[Runtime][ProfileManager]")
{
    ZProfileManager Manager;

    SMappingProfile Original;
    Original.Id = "direction_test";
    Original.Name = "Direction Test";
    Original.bEnabled = true;

    SMappingRule Rule;
    Rule.Id = "left_stick_up";
    Rule.DisplayName = "left_stick_up";
    Rule.bEnabled = true;
    Rule.Input.ControlId = "left_stick_up";
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "W", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    Original.Rules.push_back(Rule);

    auto SerResult = Manager.SerializeProfileJson(Original);
    REQUIRE(SerResult.IsOk());

    auto ParseResult = Manager.ParseProfileJson(SerResult.Value());
    REQUIRE(ParseResult.IsOk());

    auto Parsed = std::move(ParseResult).TakeValue();
    REQUIRE(Parsed.Rules.size() == 1);
    REQUIRE(Parsed.Rules[0].Input.ControlId == "left_stick_up");
    REQUIRE(Parsed.Rules[0].Input.ControlType == EInputControlType::Button);

    auto* Payload = std::get_if<SKeyboardAction>(&Parsed.Rules[0].Output.Action.Payload);
    REQUIRE(Payload != nullptr);
    REQUIRE(Payload->Key == "W");
}

