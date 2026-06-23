// ZProfileManager 实现。
// 所有非预期路径（文件 I/O 失败、JSON 解析失败、schema 不匹配）都输出日志并返回对应错误码。

#include "Runtime/ProfileManager.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace MappyZ
{

using Json = nlohmann::json;

// ── 字符串 ↔ 枚举转换 ──

static TOptional<EInputControlType> ParseControlType(const StdString& Value)
{
    if (Value == "button") return EInputControlType::Button;
    if (Value == "axis1d") return EInputControlType::Axis1D;
    if (Value == "axis2d") return EInputControlType::Axis2D;
    if (Value == "trigger") return EInputControlType::Trigger;
    if (Value == "hat") return EInputControlType::Hat;
    return std::nullopt;
}

static StdString ControlTypeToString(EInputControlType Type)
{
    switch (Type)
    {
    case EInputControlType::Button: return "button";
    case EInputControlType::Axis1D: return "axis1d";
    case EInputControlType::Axis2D: return "axis2d";
    case EInputControlType::Trigger: return "trigger";
    case EInputControlType::Hat: return "hat";
    }
    std::fprintf(stderr, "[ProfileManager] 警告: 未知 EInputControlType 值: %d\n", static_cast<int>(Type));
    return "button";
}

static TOptional<EInputEventType> ParseEventType(const StdString& Value)
{
    if (Value == "pressed") return EInputEventType::Pressed;
    if (Value == "released") return EInputEventType::Released;
    if (Value == "changed") return EInputEventType::Changed;
    return std::nullopt;
}

static StdString EventTypeToString(EInputEventType Type)
{
    switch (Type)
    {
    case EInputEventType::Pressed: return "pressed";
    case EInputEventType::Released: return "released";
    case EInputEventType::Changed: return "changed";
    }
    std::fprintf(stderr, "[ProfileManager] 警告: 未知 EInputEventType 值: %d\n", static_cast<int>(Type));
    return "pressed";
}

static TOptional<EActionType> ParseActionType(const StdString& Value)
{
    if (Value == "keyboard_key") return EActionType::KeyboardKey;
    if (Value == "mouse_button") return EActionType::MouseButton;
    if (Value == "mouse_move") return EActionType::MouseMove;
    if (Value == "mouse_wheel") return EActionType::MouseWheel;
    return std::nullopt;
}

static StdString ActionTypeToString(EActionType Type)
{
    switch (Type)
    {
    case EActionType::None: break;
    case EActionType::KeyboardKey: return "keyboard_key";
    case EActionType::MouseButton: return "mouse_button";
    case EActionType::MouseMove: return "mouse_move";
    case EActionType::MouseWheel: return "mouse_wheel";
    }
    std::fprintf(stderr, "[ProfileManager] 警告: 无法序列化 EActionType 值: %d\n", static_cast<int>(Type));
    return "keyboard_key";
}

static TOptional<EMappingActionMode> ParseActionMode(const StdString& Value)
{
    if (Value == "press_release") return EMappingActionMode::PressRelease;
    if (Value == "hold") return EMappingActionMode::Hold;
    if (Value == "analog") return EMappingActionMode::Analog;
    return std::nullopt;
}

static StdString ActionModeToString(EMappingActionMode Mode)
{
    switch (Mode)
    {
    case EMappingActionMode::PressRelease: return "press_release";
    case EMappingActionMode::Hold: return "hold";
    case EMappingActionMode::Analog: return "analog";
    }
    std::fprintf(stderr, "[ProfileManager] 警告: 未知 EMappingActionMode 值: %d\n", static_cast<int>(Mode));
    return "press_release";
}

static TOptional<int32> ParseMouseButton(const StdString& Value)
{
    if (Value == "left") return 0;
    if (Value == "right") return 1;
    if (Value == "middle") return 2;
    return std::nullopt;
}

static StdString MouseButtonToString(int32 Button)
{
    switch (Button)
    {
    case 0: return "left";
    case 1: return "right";
    case 2: return "middle";
    }
    std::fprintf(stderr, "[ProfileManager] 警告: 未知鼠标按钮值: %d，回退为 \"left\"\n", Button);
    return "left";
}

// ── 反序列化 ──

static TResult<SMappingInput> ParseInput(const Json& InputJson)
{
    SMappingInput Input;

    if (!InputJson.contains("control_id") || !InputJson["control_id"].is_string())
    {
        return TResult<SMappingInput>::Err(
            MakeError(EErrorCode::InvalidManifest, "input missing 'control_id' string"));
    }
    Input.ControlId = InputJson["control_id"].get<StdString>();

    if (InputJson.contains("control_type"))
    {
        if (!InputJson["control_type"].is_string())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest, "input 'control_type' is not a string"));
        }
        auto ControlType = ParseControlType(InputJson["control_type"].get<StdString>());
        if (!ControlType.has_value())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest,
                    "unknown control_type: " + InputJson["control_type"].get<StdString>()));
        }
        Input.ControlType = *ControlType;
    }

    if (InputJson.contains("event"))
    {
        if (!InputJson["event"].is_string())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest, "input 'event' is not a string"));
        }
        auto EventType = ParseEventType(InputJson["event"].get<StdString>());
        if (!EventType.has_value())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest,
                    "unknown event: " + InputJson["event"].get<StdString>()));
        }
        Input.EventType = *EventType;
    }

    if (InputJson.contains("threshold"))
    {
        if (!InputJson["threshold"].is_number())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest, "input 'threshold' is not a number"));
        }
        Input.Threshold = InputJson["threshold"].get<float32>();
    }

    if (InputJson.contains("deadzone"))
    {
        if (!InputJson["deadzone"].is_number())
        {
            return TResult<SMappingInput>::Err(
                MakeError(EErrorCode::InvalidManifest, "input 'deadzone' is not a number"));
        }
        Input.Deadzone = InputJson["deadzone"].get<float32>();
    }

    return TResult<SMappingInput>::Ok(std::move(Input));
}

static TResult<SMappingOutput> ParseAction(const Json& ActionJson)
{
    SMappingOutput Output;

    if (!ActionJson.contains("type") || !ActionJson["type"].is_string())
    {
        return TResult<SMappingOutput>::Err(
            MakeError(EErrorCode::InvalidManifest, "action missing 'type' string"));
    }

    auto ActionType = ParseActionType(ActionJson["type"].get<StdString>());
    if (!ActionType.has_value())
    {
        return TResult<SMappingOutput>::Err(
            MakeError(EErrorCode::InvalidManifest,
                "unknown action type: " + ActionJson["type"].get<StdString>()));
    }
    Output.Action.Type = *ActionType;

    if (ActionJson.contains("mode"))
    {
        if (!ActionJson["mode"].is_string())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "action 'mode' is not a string"));
        }
        auto Mode = ParseActionMode(ActionJson["mode"].get<StdString>());
        if (!Mode.has_value())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest,
                    "unknown action mode: " + ActionJson["mode"].get<StdString>()));
        }
        Output.Mode = *Mode;
    }

    switch (*ActionType)
    {
    case EActionType::KeyboardKey:
    {
        if (!ActionJson.contains("keyboard_key") || !ActionJson["keyboard_key"].is_object())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "keyboard_key action missing 'keyboard_key' object"));
        }
        auto& KeyObj = ActionJson["keyboard_key"];
        if (!KeyObj.contains("key") || !KeyObj["key"].is_string())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "keyboard_key missing 'key' string"));
        }
        Output.Action.Payload = SKeyboardAction{.Key = KeyObj["key"].get<StdString>(), .bPressed = true};
        break;
    }

    case EActionType::MouseButton:
    {
        if (!ActionJson.contains("mouse_button") || !ActionJson["mouse_button"].is_object())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "mouse_button action missing 'mouse_button' object"));
        }
        auto& BtnObj = ActionJson["mouse_button"];
        if (!BtnObj.contains("button") || !BtnObj["button"].is_string())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "mouse_button missing 'button' string"));
        }
        auto Button = ParseMouseButton(BtnObj["button"].get<StdString>());
        if (!Button.has_value())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest,
                    "unknown mouse button: " + BtnObj["button"].get<StdString>()));
        }
        Output.Action.Payload = SMouseButtonAction{.Button = *Button, .bPressed = true};
        break;
    }

    case EActionType::MouseMove:
    {
        Output.Action.Payload = SMouseMoveAction{};
        if (ActionJson.contains("mouse_move") && ActionJson["mouse_move"].is_object())
        {
            auto& MoveObj = ActionJson["mouse_move"];
            if (MoveObj.contains("sensitivity"))
            {
                if (!MoveObj["sensitivity"].is_number())
                {
                    return TResult<SMappingOutput>::Err(
                        MakeError(EErrorCode::InvalidManifest, "mouse_move 'sensitivity' is not a number"));
                }
                Output.Sensitivity = MoveObj["sensitivity"].get<float32>();
            }
            if (MoveObj.contains("curve"))
            {
                if (!MoveObj["curve"].is_string())
                {
                    return TResult<SMappingOutput>::Err(
                        MakeError(EErrorCode::InvalidManifest, "mouse_move 'curve' is not a string"));
                }
                auto Curve = MoveObj["curve"].get<StdString>();
                if (Curve != "linear")
                {
                    return TResult<SMappingOutput>::Err(
                        MakeError(EErrorCode::InvalidManifest,
                            "unsupported mouse_move curve: " + Curve));
                }
            }
        }
        break;
    }

    case EActionType::MouseWheel:
    {
        if (!ActionJson.contains("mouse_wheel") || !ActionJson["mouse_wheel"].is_object())
        {
            return TResult<SMappingOutput>::Err(
                MakeError(EErrorCode::InvalidManifest, "mouse_wheel action missing 'mouse_wheel' object"));
        }
        auto& WheelObj = ActionJson["mouse_wheel"];
        float32 Delta = 0.0f;
        if (WheelObj.contains("delta"))
        {
            if (!WheelObj["delta"].is_number())
            {
                return TResult<SMappingOutput>::Err(
                    MakeError(EErrorCode::InvalidManifest, "mouse_wheel 'delta' is not a number"));
            }
            Delta = WheelObj["delta"].get<float32>();
        }
        Output.Action.Payload = SMouseWheelAction{.Delta = Delta};
        break;
    }

    case EActionType::None:
        break;
    }

    return TResult<SMappingOutput>::Ok(std::move(Output));
}

static TResult<SMappingRule> ParseMapping(const Json& MappingJson)
{
    SMappingRule Rule;

    if (MappingJson.contains("id") && MappingJson["id"].is_string())
    {
        Rule.Id = MappingJson["id"].get<StdString>();
    }

    if (MappingJson.contains("display_name") && MappingJson["display_name"].is_string())
    {
        Rule.DisplayName = MappingJson["display_name"].get<StdString>();
    }

    if (MappingJson.contains("enabled") && MappingJson["enabled"].is_boolean())
    {
        Rule.bEnabled = MappingJson["enabled"].get<bool>();
    }

    if (MappingJson.contains("input") && MappingJson["input"].is_object())
    {
        auto InputResult = ParseInput(MappingJson["input"]);
        if (InputResult.IsErr())
        {
            return TResult<SMappingRule>::Err(std::move(InputResult).TakeFailure());
        }
        Rule.Input = std::move(InputResult).TakeValue();
    }

    if (MappingJson.contains("action") && MappingJson["action"].is_object())
    {
        auto ActionResult = ParseAction(MappingJson["action"]);
        if (ActionResult.IsErr())
        {
            return TResult<SMappingRule>::Err(std::move(ActionResult).TakeFailure());
        }
        Rule.Output = std::move(ActionResult).TakeValue();
    }

    return TResult<SMappingRule>::Ok(std::move(Rule));
}

// ── 序列化 ──

static Json SerializeInput(const SMappingInput& Input)
{
    Json InputJson;
    InputJson["control_id"] = Input.ControlId;
    InputJson["control_type"] = ControlTypeToString(Input.ControlType);
    InputJson["event"] = EventTypeToString(Input.EventType);
    if (Input.Threshold != 0.5f)
    {
        InputJson["threshold"] = Input.Threshold;
    }
    if (Input.Deadzone != 0.0f)
    {
        InputJson["deadzone"] = Input.Deadzone;
    }
    return InputJson;
}

static Json SerializeAction(const SMappingOutput& Output)
{
    Json ActionJson;
    ActionJson["type"] = ActionTypeToString(Output.Action.Type);
    ActionJson["mode"] = ActionModeToString(Output.Mode);

    switch (Output.Action.Type)
    {
    case EActionType::KeyboardKey:
    {
        auto* Payload = std::get_if<SKeyboardAction>(&Output.Action.Payload);
        if (Payload)
        {
            ActionJson["keyboard_key"] = Json{{"key", Payload->Key}};
        }
        break;
    }

    case EActionType::MouseButton:
    {
        auto* Payload = std::get_if<SMouseButtonAction>(&Output.Action.Payload);
        if (Payload)
        {
            ActionJson["mouse_button"] = Json{{"button", MouseButtonToString(Payload->Button)}};
        }
        break;
    }

    case EActionType::MouseMove:
    {
        Json MoveObj;
        MoveObj["sensitivity"] = Output.Sensitivity;
        ActionJson["mouse_move"] = MoveObj;
        break;
    }

    case EActionType::MouseWheel:
    {
        auto* Payload = std::get_if<SMouseWheelAction>(&Output.Action.Payload);
        if (Payload)
        {
            ActionJson["mouse_wheel"] = Json{{"delta", Payload->Delta}};
        }
        break;
    }

    case EActionType::None:
        break;
    }

    return ActionJson;
}

static Json SerializeMapping(const SMappingRule& Rule)
{
    Json MappingJson;
    MappingJson["id"] = Rule.Id;
    MappingJson["display_name"] = Rule.DisplayName;
    MappingJson["enabled"] = Rule.bEnabled;
    MappingJson["input"] = SerializeInput(Rule.Input);
    MappingJson["action"] = SerializeAction(Rule.Output);
    return MappingJson;
}

// ── 公开 API ──

TResult<SMappingProfile> ZProfileManager::ParseProfileJson(StdStringView JsonText) const
{
    Json Root;
    try
    {
        Root = Json::parse(JsonText);
    }
    catch (const Json::parse_error& Error)
    {
        std::fprintf(stderr, "[ProfileManager] 警告: JSON 解析失败: %s\n", Error.what());
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::ParseFailed, StdString("JSON parse error: ") + Error.what()));
    }

    try
    {

    if (!Root.is_object())
    {
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::InvalidManifest, "root is not a JSON object"));
    }

    if (!Root.contains("schema_version") || !Root["schema_version"].is_number_integer())
    {
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::InvalidManifest, "missing or non-integer 'schema_version'"));
    }

    auto SchemaVersion = Root["schema_version"].get<uint32>();
    if (SchemaVersion != 1)
    {
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::InvalidManifest,
                "unsupported schema_version: " + std::to_string(SchemaVersion)));
    }

    SMappingProfile Profile;
    Profile.SchemaVersion = SchemaVersion;

    if (Root.contains("profile_id") && Root["profile_id"].is_string())
    {
        Profile.Id = Root["profile_id"].get<StdString>();
    }

    if (Root.contains("profile_name") && Root["profile_name"].is_string())
    {
        Profile.Name = Root["profile_name"].get<StdString>();
    }

    if (Root.contains("settings") && Root["settings"].is_object())
    {
        auto& Settings = Root["settings"];
        if (Settings.contains("enabled") && Settings["enabled"].is_boolean())
        {
            Profile.bEnabled = Settings["enabled"].get<bool>();
        }
    }

    if (Root.contains("device_match") && Root["device_match"].is_object())
    {
        auto& Dm = Root["device_match"];
        if (Dm.contains("name") && Dm["name"].is_string())
            Profile.DeviceMatch.Name = Dm["name"].get<StdString>();
        if (Dm.contains("backend") && Dm["backend"].is_string())
            Profile.DeviceMatch.Backend = Dm["backend"].get<StdString>();
        if (Dm.contains("vendor_id") && Dm["vendor_id"].is_string())
            Profile.DeviceMatch.VendorId = Dm["vendor_id"].get<StdString>();
        if (Dm.contains("product_id") && Dm["product_id"].is_string())
            Profile.DeviceMatch.ProductId = Dm["product_id"].get<StdString>();
        if (Dm.contains("guid") && Dm["guid"].is_string())
            Profile.DeviceMatch.Guid = Dm["guid"].get<StdString>();
        if (Dm.contains("instance_id") && Dm["instance_id"].is_string())
            Profile.DeviceMatch.InstanceId = Dm["instance_id"].get<StdString>();
    }

    if (Root.contains("mappings") && Root["mappings"].is_array())
    {
        for (const auto& MappingJson : Root["mappings"])
        {
            auto RuleResult = ParseMapping(MappingJson);
            if (RuleResult.IsErr())
            {
                return TResult<SMappingProfile>::Err(std::move(RuleResult).TakeFailure());
            }
            Profile.Rules.push_back(std::move(RuleResult).TakeValue());
        }
    }

    return TResult<SMappingProfile>::Ok(std::move(Profile));

    }
    catch (const Json::exception& Error)
    {
        std::fprintf(stderr, "[ProfileManager] 警告: JSON 类型异常: %s\n", Error.what());
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::InvalidManifest, StdString("JSON type error: ") + Error.what()));
    }
}

TResult<StdString> ZProfileManager::SerializeProfileJson(const SMappingProfile& Profile) const
{
    Json Root;
    Root["schema_version"] = Profile.SchemaVersion;
    Root["profile_id"] = Profile.Id;
    Root["profile_name"] = Profile.Name;
    Root["settings"] = Json{{"enabled", Profile.bEnabled}};

    Json DeviceMatchJson;
    if (!Profile.DeviceMatch.Name.empty())
        DeviceMatchJson["name"] = Profile.DeviceMatch.Name;
    if (!Profile.DeviceMatch.Backend.empty())
        DeviceMatchJson["backend"] = Profile.DeviceMatch.Backend;
    if (!Profile.DeviceMatch.VendorId.empty())
        DeviceMatchJson["vendor_id"] = Profile.DeviceMatch.VendorId;
    if (!Profile.DeviceMatch.ProductId.empty())
        DeviceMatchJson["product_id"] = Profile.DeviceMatch.ProductId;
    if (!Profile.DeviceMatch.Guid.empty())
        DeviceMatchJson["guid"] = Profile.DeviceMatch.Guid;
    if (!Profile.DeviceMatch.InstanceId.empty())
        DeviceMatchJson["instance_id"] = Profile.DeviceMatch.InstanceId;
    if (!DeviceMatchJson.empty())
    {
        Root["device_match"] = DeviceMatchJson;
    }

    Json Mappings = Json::array();
    for (const auto& Rule : Profile.Rules)
    {
        Mappings.push_back(SerializeMapping(Rule));
    }
    Root["mappings"] = Mappings;

    return TResult<StdString>::Ok(Root.dump(2));
}

TResult<SMappingProfile> ZProfileManager::LoadProfile(const StdPath& ProfilePath) const
{
    if (!std::filesystem::exists(ProfilePath))
    {
        std::fprintf(stderr, "[ProfileManager] 警告: Profile 文件不存在: \"%s\"\n",
            ProfilePath.string().c_str());
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::FileNotFound, "file not found", ProfilePath));
    }

    std::ifstream File(ProfilePath, std::ios::in | std::ios::binary);
    if (!File.is_open())
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 无法打开 Profile 文件: \"%s\"\n",
            ProfilePath.string().c_str());
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::FileReadFailed, "cannot open file", ProfilePath));
    }

    std::ostringstream Stream;
    Stream << File.rdbuf();
    if (File.bad())
    {
        return TResult<SMappingProfile>::Err(
            MakeError(EErrorCode::FileReadFailed, "read error", ProfilePath));
    }

    auto Result = ParseProfileJson(Stream.str());
    if (Result.IsErr())
    {
        auto Error = std::move(Result).TakeFailure();
        Error.ContextPath = ProfilePath;
        return TResult<SMappingProfile>::Err(std::move(Error));
    }

    return Result;
}

TResult<void> ZProfileManager::SaveProfile(const SMappingProfile& Profile, const StdPath& ProfilePath) const
{
    auto JsonResult = SerializeProfileJson(Profile);
    if (JsonResult.IsErr())
    {
        return TResult<void>::Err(std::move(JsonResult).TakeFailure());
    }

    auto ParentPath = ProfilePath.parent_path();
    if (!ParentPath.empty() && !std::filesystem::exists(ParentPath))
    {
        std::error_code Ec;
        std::filesystem::create_directories(ParentPath, Ec);
        if (Ec)
        {
            return TResult<void>::Err(
                MakeError(EErrorCode::FileWriteFailed, "cannot create directory", ProfilePath));
        }
    }

    std::ofstream File(ProfilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!File.is_open())
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 无法写入 Profile 文件: \"%s\"\n",
            ProfilePath.string().c_str());
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "cannot open file for writing", ProfilePath));
    }

    auto& JsonText = JsonResult.Value();
    File.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
    if (File.bad())
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "write error", ProfilePath));
    }

    return TResult<void>::Ok();
}

}  // namespace MappyZ
