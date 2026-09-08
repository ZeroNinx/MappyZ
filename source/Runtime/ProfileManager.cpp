// ZProfileManager 实现。
// 所有非预期路径（文件 I/O 失败、JSON 解析失败、schema 不匹配）都输出日志并返回对应错误码。

#include "Runtime/ProfileManager.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace MappyZ
{

using Json = nlohmann::json;

// ── 文本辅助（仅处理 ASCII，非 ASCII 内容保持原值）──

// 去除首尾空白，不修改中间空白，不做 Unicode 正规化。
static StdString TrimAscii(StdStringView Text)
{
    auto IsSpace = [](unsigned char Ch) {
        return Ch == ' ' || Ch == '\t' || Ch == '\r'
            || Ch == '\n' || Ch == '\f' || Ch == '\v';
    };

    size_t Start = 0;
    size_t End = Text.size();
    while (Start < End && IsSpace(static_cast<unsigned char>(Text[Start]))) ++Start;
    while (End > Start && IsSpace(static_cast<unsigned char>(Text[End - 1]))) --End;
    return StdString(Text.substr(Start, End - Start));
}

// ASCII 小写化，非字母字节原样返回。
static char AsciiToLower(char Ch)
{
    if (Ch >= 'A' && Ch <= 'Z') return static_cast<char>(Ch - 'A' + 'a');
    return Ch;
}

// ASCII 大小写无关相等比较。
static bool AsciiEqualsIgnoreCase(StdStringView A, StdStringView B)
{
    if (A.size() != B.size()) return false;
    for (size_t Index = 0; Index < A.size(); ++Index)
    {
        if (AsciiToLower(A[Index]) != AsciiToLower(B[Index])) return false;
    }
    return true;
}

// ASCII 大小写无关字典序比较：返回负/零/正。
static int AsciiCompareIgnoreCase(StdStringView A, StdStringView B)
{
    const size_t Count = std::min(A.size(), B.size());
    for (size_t Index = 0; Index < Count; ++Index)
    {
        const char Ca = AsciiToLower(A[Index]);
        const char Cb = AsciiToLower(B[Index]);
        if (Ca != Cb)
        {
            return static_cast<unsigned char>(Ca) < static_cast<unsigned char>(Cb) ? -1 : 1;
        }
    }
    if (A.size() == B.size()) return 0;
    return A.size() < B.size() ? -1 : 1;
}

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

// ── 集合 API：查询 ──

const TVector<SProfileInfo>& ZProfileManager::GetProfiles() const noexcept
{
    return Profiles;
}

TOptional<SProfileInfo> ZProfileManager::GetActiveProfileInfo() const
{
    if (!bInitialized) return std::nullopt;

    auto Index = FindProfileIndex(ActiveProfileId);
    if (!Index.has_value()) return std::nullopt;

    return Profiles[*Index];
}

bool ZProfileManager::CanDeleteActiveProfile() const noexcept
{
    return bInitialized && Profiles.size() > 1;
}

// ── 集合 API：私有辅助 ──

TResult<void> ZProfileManager::EnsureProfilesDirectory()
{
    if (ProfilesDirectory.empty())
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, "profiles directory path must not be empty"));
    }

    std::error_code Ec;
    std::filesystem::create_directories(ProfilesDirectory, Ec);
    // create_directories 对已存在目录返回 false 但不设置 error_code；仅 Ec 被设置才是真正失败。
    if (Ec)
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 无法创建配置目录 \"%s\": %s\n",
            ProfilesDirectory.string().c_str(), Ec.message().c_str());
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "cannot create profiles directory", ProfilesDirectory));
    }

    return TResult<void>::Ok();
}

TResult<void> ZProfileManager::DiscoverProfiles()
{
    Profiles.clear();

    // 收集配置目录直属的 .json 文件路径，不递归子目录。
    std::error_code Ec;
    std::filesystem::directory_iterator Iterator(ProfilesDirectory, Ec);
    if (Ec)
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 无法枚举配置目录 \"%s\": %s\n",
            ProfilesDirectory.string().c_str(), Ec.message().c_str());
        return TResult<void>::Err(
            MakeError(EErrorCode::FileReadFailed, "cannot enumerate profiles directory", ProfilesDirectory));
    }

    TVector<StdPath> JsonPaths;
    const std::filesystem::directory_iterator End;
    for (; Iterator != End; Iterator.increment(Ec))
    {
        if (Ec)
        {
            std::fprintf(stderr, "[ProfileManager] 警告: 枚举配置目录时出错 \"%s\": %s\n",
                ProfilesDirectory.string().c_str(), Ec.message().c_str());
            return TResult<void>::Err(
                MakeError(EErrorCode::FileReadFailed, "error while enumerating profiles directory",
                    ProfilesDirectory));
        }

        const StdPath& Path = Iterator->path();

        std::error_code FileEc;
        if (!std::filesystem::is_regular_file(Path, FileEc)) continue;
        if (Path.extension() != ".json") continue;

        JsonPaths.push_back(Path);
    }

    // 枚举前先按完整路径排序，使重复项处理稳定。
    std::sort(JsonPaths.begin(), JsonPaths.end());

    for (const StdPath& Path : JsonPaths)
    {
        auto LoadResult = LoadProfile(Path);
        if (LoadResult.IsErr())
        {
            // 无法读取、无法解析或 schema 无效：写日志，保留原文件，不加入列表。
            std::fprintf(stderr, "[ProfileManager] 警告: 跳过无效配置文件 \"%s\": %s\n",
                Path.string().c_str(), LoadResult.Failure().Message.c_str());
            continue;
        }

        auto Profile = std::move(LoadResult).TakeValue();

        // 为缺失 ID/名称的旧文件填入内存 fallback（下一次保存时写回 JSON）。
        const StdString Stem = Path.stem().string();
        StdString Id = Profile.Id.empty() ? Stem : Profile.Id;
        StdString Name = Profile.Name.empty() ? Stem : Profile.Name;

        // 重复 profile_id：保留先遇到的第一份，记录错误，后续重复项不加入列表。
        if (FindProfileIndex(Id).has_value())
        {
            std::fprintf(stderr, "[ProfileManager] 警告: 跳过重复 profile_id \"%s\"（文件 \"%s\"）\n",
                Id.c_str(), Path.string().c_str());
            continue;
        }

        // 重复显示名（忽略大小写）：同样保留第一份。
        bool bDuplicateName = false;
        for (const SProfileInfo& Existing : Profiles)
        {
            if (AsciiEqualsIgnoreCase(Existing.Name, Name))
            {
                bDuplicateName = true;
                break;
            }
        }
        if (bDuplicateName)
        {
            std::fprintf(stderr, "[ProfileManager] 警告: 跳过重复显示名 \"%s\"（文件 \"%s\"）\n",
                Name.c_str(), Path.string().c_str());
            continue;
        }

        SProfileInfo Info;
        Info.Id = std::move(Id);
        Info.Name = std::move(Name);
        Info.FilePath = Path;
        Profiles.push_back(std::move(Info));
    }

    return TResult<void>::Ok();
}

TResult<StdString> ZProfileManager::ReadActiveProfileId() const
{
    // 文件不存在视为空记录（首次运行），由调用方按 default/首项规则恢复。
    if (!std::filesystem::exists(ActiveProfileStatePath))
    {
        return TResult<StdString>::Ok(StdString());
    }

    std::ifstream File(ActiveProfileStatePath, std::ios::in | std::ios::binary);
    if (!File.is_open())
    {
        return TResult<StdString>::Err(
            MakeError(EErrorCode::FileReadFailed, "cannot open active profile state file",
                ActiveProfileStatePath));
    }

    std::ostringstream Stream;
    Stream << File.rdbuf();
    if (File.bad())
    {
        return TResult<StdString>::Err(
            MakeError(EErrorCode::FileReadFailed, "read error", ActiveProfileStatePath));
    }

    return TResult<StdString>::Ok(TrimAscii(Stream.str()));
}

TResult<void> ZProfileManager::WriteActiveProfileId(StdStringView ProfileId) const
{
    std::ofstream File(ActiveProfileStatePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!File.is_open())
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 无法写入当前配置状态文件 \"%s\"\n",
            ActiveProfileStatePath.string().c_str());
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "cannot open active profile state file for writing",
                ActiveProfileStatePath));
    }

    File.write(ProfileId.data(), static_cast<std::streamsize>(ProfileId.size()));
    File.put('\n');
    if (File.bad())
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "write error", ActiveProfileStatePath));
    }

    return TResult<void>::Ok();
}

TResult<SLoadedProfile> ZProfileManager::LoadManagedProfile(const SProfileInfo& Info) const
{
    auto LoadResult = LoadProfile(Info.FilePath);
    if (LoadResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(LoadResult).TakeFailure());
    }

    auto Profile = std::move(LoadResult).TakeValue();
    // 强制把 snapshot 的身份字段设为 SProfileInfo 的权威值。
    Profile.Id = Info.Id;
    Profile.Name = Info.Name;

    SLoadedProfile Loaded;
    Loaded.Info = Info;
    Loaded.Profile = std::move(Profile);
    return TResult<SLoadedProfile>::Ok(std::move(Loaded));
}

TOptional<uint32> ZProfileManager::FindProfileIndex(StdStringView ProfileId) const
{
    for (uint32 Index = 0; Index < Profiles.size(); ++Index)
    {
        if (StdStringView(Profiles[Index].Id) == ProfileId)
        {
            return Index;
        }
    }
    return std::nullopt;
}

TResult<StdString> ZProfileManager::NormalizeAndValidateName(
    StdStringView Name, StdStringView IgnoredProfileId) const
{
    // 名称规范化只移除首尾空白，不修改中间空白，不执行 Unicode 正规化。
    StdString Normalized = TrimAscii(Name);
    if (Normalized.empty())
    {
        return TResult<StdString>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile name must not be empty after trimming"));
    }

    for (const SProfileInfo& Info : Profiles)
    {
        if (!IgnoredProfileId.empty() && StdStringView(Info.Id) == IgnoredProfileId)
        {
            continue;
        }
        if (AsciiEqualsIgnoreCase(Info.Name, Normalized))
        {
            return TResult<StdString>::Err(
                MakeError(EErrorCode::InvalidArgument, "profile name already exists: " + Normalized));
        }
    }

    return TResult<StdString>::Ok(std::move(Normalized));
}

void ZProfileManager::SortProfiles()
{
    // 按名称忽略大小写排序；名称相同时按 ID 排序。
    std::sort(Profiles.begin(), Profiles.end(),
        [](const SProfileInfo& A, const SProfileInfo& B) {
            const int NameCmp = AsciiCompareIgnoreCase(A.Name, B.Name);
            if (NameCmp != 0) return NameCmp < 0;
            return A.Id < B.Id;
        });
}

// ── 集合 API：初始化 ──

TResult<SLoadedProfile> ZProfileManager::Initialize(const StdPath& InProfilesDirectory)
{
    // 初始化中途失败不得留下半初始化状态。
    bInitialized = false;
    Profiles.clear();
    ActiveProfileId.clear();

    ProfilesDirectory = InProfilesDirectory;

    // 1. 校验目录非空并创建目录。
    auto DirResult = EnsureProfilesDirectory();
    if (DirResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(DirResult).TakeFailure());
    }

    // 2. 设置 active_profile.txt 路径。
    ActiveProfileStatePath = ProfilesDirectory / "active_profile.txt";

    // 3-4. 枚举、加载、修复旧字段、去重，构造列表。
    auto DiscoverResult = DiscoverProfiles();
    if (DiscoverResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(DiscoverResult).TakeFailure());
    }

    // 5. 若列表为空，创建 Default 并加入列表。
    //    优先 default.json/default；路径被损坏文件占用时使用 default_N，不覆盖原文件。
    if (Profiles.empty())
    {
        StdString Id = "default";
        StdPath Path = ProfilesDirectory / "default.json";
        uint32 Suffix = 1;
        while (std::filesystem::exists(Path))
        {
            Id = "default_" + std::to_string(Suffix);
            Path = ProfilesDirectory / (Id + ".json");
            ++Suffix;
        }

        SMappingProfile Default;
        Default.SchemaVersion = 1;
        Default.Id = Id;
        Default.Name = "Default";
        Default.bEnabled = true;

        auto SaveResult = SaveProfile(Default, Path);
        if (SaveResult.IsErr())
        {
            return TResult<SLoadedProfile>::Err(std::move(SaveResult).TakeFailure());
        }

        SProfileInfo Info;
        Info.Id = Id;
        Info.Name = "Default";
        Info.FilePath = Path;
        Profiles.push_back(std::move(Info));
    }

    // 6. 排序列表。
    SortProfiles();

    // 7. 读取 active_profile.txt，选出记录项、default 或首项。
    StdString RecordedId;
    auto ReadResult = ReadActiveProfileId();
    if (ReadResult.IsOk())
    {
        RecordedId = std::move(ReadResult).TakeValue();
    }
    else
    {
        // 读取失败不阻断初始化，退化为按 default/首项恢复。
        std::fprintf(stderr, "[ProfileManager] 警告: 读取当前配置状态失败，退化为默认选择: %s\n",
            ReadResult.Failure().Message.c_str());
    }

    StdString SelectedId;
    if (!RecordedId.empty() && FindProfileIndex(RecordedId).has_value())
    {
        SelectedId = RecordedId;
    }
    else if (FindProfileIndex("default").has_value())
    {
        SelectedId = "default";
    }
    else
    {
        SelectedId = Profiles.front().Id;
    }

    // 8. 完整加载选中项；成功后写入当前 ID 文件。
    auto SelectedIndex = FindProfileIndex(SelectedId);
    auto LoadResult = LoadManagedProfile(Profiles[*SelectedIndex]);
    if (LoadResult.IsErr())
    {
        std::fprintf(stderr, "[ProfileManager] 警告: 加载选中配置失败 \"%s\": %s\n",
            SelectedId.c_str(), LoadResult.Failure().Message.c_str());
        return TResult<SLoadedProfile>::Err(std::move(LoadResult).TakeFailure());
    }

    auto WriteResult = WriteActiveProfileId(SelectedId);
    if (WriteResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(WriteResult).TakeFailure());
    }

    // 9. 提交当前 ID 与初始化标志。
    ActiveProfileId = SelectedId;
    bInitialized = true;
    return LoadResult;
}

// ── 集合 API：切换 ──

TResult<SLoadedProfile> ZProfileManager::ActivateProfile(StdStringView ProfileId)
{
    if (!bInitialized)
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile manager not initialized"));
    }

    if (ProfileId.empty())
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile id must not be empty"));
    }

    auto Index = FindProfileIndex(ProfileId);
    if (!Index.has_value())
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "unknown profile id: " + StdString(ProfileId)));
    }

    // 无论是否当前项都从文件加载并返回。
    auto LoadResult = LoadManagedProfile(Profiles[*Index]);
    if (LoadResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(LoadResult).TakeFailure());
    }

    // ID 等于当前项时不重写状态文件；否则写入成功后再提交 ActiveProfileId。
    if (StdStringView(ActiveProfileId) != ProfileId)
    {
        auto WriteResult = WriteActiveProfileId(ProfileId);
        if (WriteResult.IsErr())
        {
            // 任何失败都保持原 ID 和列表不变。
            return TResult<SLoadedProfile>::Err(std::move(WriteResult).TakeFailure());
        }
        ActiveProfileId = StdString(ProfileId);
    }

    return LoadResult;
}

// ── 集合 API：创建 ──

TResult<SLoadedProfile> ZProfileManager::CreateProfile()
{
    if (!bInitialized)
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile manager not initialized"));
    }

    // 从 N = 1 开始，查找显示名/ID/路径均未占用的第一个编号。
    StdString Id;
    StdString Name;
    StdPath Path;
    for (uint32 Suffix = 1; ; ++Suffix)
    {
        Name = "Untitled_" + std::to_string(Suffix);
        Id = "untitled_" + std::to_string(Suffix);
        Path = ProfilesDirectory / (Id + ".json");

        const bool bIdUsed = FindProfileIndex(Id).has_value();

        bool bNameUsed = false;
        for (const SProfileInfo& Info : Profiles)
        {
            if (AsciiEqualsIgnoreCase(Info.Name, Name))
            {
                bNameUsed = true;
                break;
            }
        }

        const bool bPathUsed = std::filesystem::exists(Path);

        if (!bIdUsed && !bNameUsed && !bPathUsed) break;
    }

    // 构造空配置，不复制当前配置的映射。
    SMappingProfile Profile;
    Profile.SchemaVersion = 1;
    Profile.Id = Id;
    Profile.Name = Name;
    Profile.bEnabled = true;

    auto SaveResult = SaveProfile(Profile, Path);
    if (SaveResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(SaveResult).TakeFailure());
    }

    auto WriteResult = WriteActiveProfileId(Id);
    if (WriteResult.IsErr())
    {
        // 状态文件写入失败：删除本次刚创建的文件并返回错误。
        std::error_code Ec;
        std::filesystem::remove(Path, Ec);
        return TResult<SLoadedProfile>::Err(std::move(WriteResult).TakeFailure());
    }

    SProfileInfo Info;
    Info.Id = Id;
    Info.Name = Name;
    Info.FilePath = Path;
    Profiles.push_back(Info);
    SortProfiles();
    ActiveProfileId = Id;

    SLoadedProfile Loaded;
    Loaded.Info = std::move(Info);
    Loaded.Profile = std::move(Profile);
    return TResult<SLoadedProfile>::Ok(std::move(Loaded));
}

// ── 集合 API：重命名 ──

TResult<SLoadedProfile> ZProfileManager::RenameActiveProfile(
    const SMappingProfile& CurrentSnapshot, StdStringView NewName)
{
    if (!bInitialized)
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile manager not initialized"));
    }

    // 规范化新名称，排除当前 ID 后检查重名。
    auto NameResult = NormalizeAndValidateName(NewName, ActiveProfileId);
    if (NameResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(NameResult).TakeFailure());
    }
    StdString Normalized = std::move(NameResult).TakeValue();

    auto Index = FindProfileIndex(ActiveProfileId);
    if (!Index.has_value())
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "no active profile to rename"));
    }

    const StdString SavedId = Profiles[*Index].Id;
    const StdPath SavedPath = Profiles[*Index].FilePath;

    // 复制 CurrentSnapshot，强制写入当前 ID 和新名称，防止 UI 编辑改变身份字段。
    SMappingProfile Updated = CurrentSnapshot;
    Updated.Id = SavedId;
    Updated.Name = Normalized;

    auto SaveResult = SaveProfile(Updated, SavedPath);
    if (SaveResult.IsErr())
    {
        // 保存失败：列表、Runtime snapshot 和文件中的旧名称保持原状。
        return TResult<SLoadedProfile>::Err(std::move(SaveResult).TakeFailure());
    }

    // 更新列表中当前项名称并重新排序（重排后原下标失效，使用已保存的副本构造返回）。
    Profiles[*Index].Name = Normalized;
    SortProfiles();

    SProfileInfo ResultInfo;
    ResultInfo.Id = SavedId;
    ResultInfo.Name = Normalized;
    ResultInfo.FilePath = SavedPath;

    SLoadedProfile Loaded;
    Loaded.Info = std::move(ResultInfo);
    Loaded.Profile = std::move(Updated);
    return TResult<SLoadedProfile>::Ok(std::move(Loaded));
}

// ── 集合 API：删除 ──

TResult<SLoadedProfile> ZProfileManager::DeleteActiveProfile()
{
    if (!bInitialized)
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile manager not initialized"));
    }

    // 配置数量小于等于 1 时拒绝删除。
    if (Profiles.size() <= 1)
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "cannot delete the only remaining profile"));
    }

    auto CurrentIndex = FindProfileIndex(ActiveProfileId);
    if (!CurrentIndex.has_value())
    {
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::InvalidArgument, "no active profile to delete"));
    }

    // 从排序列表中选择第一个非当前项作为回退项。
    TOptional<uint32> FallbackIndex;
    for (uint32 Index = 0; Index < Profiles.size(); ++Index)
    {
        if (Index != *CurrentIndex)
        {
            FallbackIndex = Index;
            break;
        }
    }

    // 完整加载回退项（独立于列表，后续删除元素安全）。
    auto FallbackLoad = LoadManagedProfile(Profiles[*FallbackIndex]);
    if (FallbackLoad.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(FallbackLoad).TakeFailure());
    }

    const StdString FallbackId = Profiles[*FallbackIndex].Id;
    const StdString OldActiveId = ActiveProfileId;
    const StdPath DeletePath = Profiles[*CurrentIndex].FilePath;

    // 先把状态文件写为回退 ID。
    auto WriteResult = WriteActiveProfileId(FallbackId);
    if (WriteResult.IsErr())
    {
        return TResult<SLoadedProfile>::Err(std::move(WriteResult).TakeFailure());
    }

    // 只删除 manager 枚举并保存的精确 JSON 路径。
    std::error_code Ec;
    std::filesystem::remove(DeletePath, Ec);
    if (Ec)
    {
        // 删除失败：尝试把状态文件恢复为原 ID，然后返回错误；内存列表和当前 ID 不变。
        std::fprintf(stderr, "[ProfileManager] 警告: 删除配置文件失败 \"%s\": %s\n",
            DeletePath.string().c_str(), Ec.message().c_str());
        auto RestoreResult = WriteActiveProfileId(OldActiveId);
        if (RestoreResult.IsErr())
        {
            std::fprintf(stderr, "[ProfileManager] 警告: 恢复当前配置状态文件失败: %s\n",
                RestoreResult.Failure().Message.c_str());
        }
        return TResult<SLoadedProfile>::Err(
            MakeError(EErrorCode::FileWriteFailed, "cannot delete profile file", DeletePath));
    }

    // 删除成功：从列表移除旧项、提交回退 ID，返回已预加载的回退 profile。
    Profiles.erase(Profiles.begin() + static_cast<std::ptrdiff_t>(*CurrentIndex));
    ActiveProfileId = FallbackId;
    return FallbackLoad;
}

// ── 集合 API：保存当前项 ──

TResult<void> ZProfileManager::SaveActiveProfile(const SMappingProfile& CurrentSnapshot)
{
    if (!bInitialized)
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, "profile manager not initialized"));
    }

    auto Index = FindProfileIndex(ActiveProfileId);
    if (!Index.has_value())
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, "no active profile to save"));
    }

    const SProfileInfo& Info = Profiles[*Index];

    // 先复制 snapshot，再覆盖 Id/Name，防止 UI 映射编辑意外改变身份字段。
    SMappingProfile Copy = CurrentSnapshot;
    Copy.Id = Info.Id;
    Copy.Name = Info.Name;

    return SaveProfile(Copy, Info.FilePath);
}

}  // namespace MappyZ
