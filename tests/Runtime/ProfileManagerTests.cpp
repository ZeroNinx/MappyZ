// ZProfileManager 单元测试。
// 验证 JSON 解析、序列化、round-trip 和文件 I/O 行为。

#include <catch2/catch_test_macros.hpp>

#include "Runtime/ProfileManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace MappyZ;

namespace
{

// 每个集合测试使用各自唯一的临时目录，析构时递归删除，避免读写真实 AppData 或彼此干扰。
struct STempProfileDir
{
    StdPath Path;

    explicit STempProfileDir(const StdString& Label)
    {
        static std::atomic<uint32> Counter{0};
        const auto Unique =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "_" + std::to_string(Counter.fetch_add(1));
        Path = std::filesystem::temp_directory_path() / ("mappyz_pm_" + Label + "_" + Unique);
        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
    }

    ~STempProfileDir()
    {
        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
    }
};

// 向指定路径写入 UTF-8 文本（二进制方式，避免行尾转换）。
void WriteTextFile(const StdPath& FilePath, const StdString& Content)
{
    std::filesystem::create_directories(FilePath.parent_path());
    std::ofstream File(FilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    File.write(Content.data(), static_cast<std::streamsize>(Content.size()));
}

// 读取整份文本文件内容（二进制方式）。
StdString ReadTextFile(const StdPath& FilePath)
{
    std::ifstream File(FilePath, std::ios::in | std::ios::binary);
    std::ostringstream Stream;
    Stream << File.rdbuf();
    return Stream.str();
}

}  // namespace

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

// ============================================================================
// 集合 API 测试（配置目录、列表、当前配置生命周期）
// ============================================================================

// ── 初始化：空目录创建 Default ──

TEST_CASE("ProfileManager Initialize on empty directory creates Default",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("empty_init");
    ZProfileManager Manager;

    auto Result = Manager.Initialize(Temp.Path);
    REQUIRE(Result.IsOk());

    auto Loaded = std::move(Result).TakeValue();
    REQUIRE(Loaded.Info.Id == "default");
    REQUIRE(Loaded.Info.Name == "Default");
    REQUIRE(Loaded.Profile.Rules.empty());

    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
    REQUIRE(std::filesystem::exists(Temp.Path / "active_profile.txt"));

    REQUIRE(Manager.GetProfiles().size() == 1);
    auto Active = Manager.GetActiveProfileInfo();
    REQUIRE(Active.has_value());
    REQUIRE(Active->Id == "default");
    REQUIRE_FALSE(Manager.CanDeleteActiveProfile());
}

// ── 初始化：旧 default.json 缺失 ID 时用文件 stem 兜底 ──

TEST_CASE("ProfileManager Initialize discovers legacy default.json without id",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("legacy_default");
    WriteTextFile(Temp.Path / "default.json", R"({
        "schema_version": 1,
        "mappings": [{
            "id": "r1",
            "input": { "control_id": "button_south", "control_type": "button", "event": "pressed" },
            "action": { "type": "keyboard_key", "mode": "press_release", "keyboard_key": { "key": "A" } }
        }]
    })");

    ZProfileManager Manager;
    auto Result = Manager.Initialize(Temp.Path);
    REQUIRE(Result.IsOk());

    auto Loaded = std::move(Result).TakeValue();
    REQUIRE(Loaded.Info.Id == "default");
    REQUIRE(Loaded.Profile.Id == "default");
    REQUIRE(Loaded.Profile.Rules.size() == 1);
}

// ── 重建 manager 后恢复 active_profile.txt 指向的配置 ──

TEST_CASE("ProfileManager reload restores active profile from state file",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("reload_active");
    {
        ZProfileManager Manager;
        REQUIRE(Manager.Initialize(Temp.Path).IsOk());
        REQUIRE(Manager.CreateProfile().IsOk());  // untitled_1 成为当前项
    }
    {
        ZProfileManager Manager;
        auto Result = Manager.Initialize(Temp.Path);
        REQUIRE(Result.IsOk());
        auto Loaded = std::move(Result).TakeValue();
        REQUIRE(Loaded.Info.Id == "untitled_1");
    }
}

// ── 状态文件指向不存在项时按 default 规则恢复 ──

TEST_CASE("ProfileManager falls back to default when state file invalid",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("state_fallback");
    {
        ZProfileManager Manager;
        REQUIRE(Manager.Initialize(Temp.Path).IsOk());
        REQUIRE(Manager.CreateProfile().IsOk());  // untitled_1
    }
    WriteTextFile(Temp.Path / "active_profile.txt", "nonexistent_id\n");
    {
        ZProfileManager Manager;
        auto Result = Manager.Initialize(Temp.Path);
        REQUIRE(Result.IsOk());
        auto Loaded = std::move(Result).TakeValue();
        REQUIRE(Loaded.Info.Id == "default");
    }
}

// ── 列表按名称忽略大小写、名称相同时按 ID 稳定排序 ──

TEST_CASE("ProfileManager sorts profiles by name then id",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("sort");
    WriteTextFile(Temp.Path / "a.json",
        R"({"schema_version":1,"profile_id":"zeta","profile_name":"Zeta"})");
    WriteTextFile(Temp.Path / "b.json",
        R"({"schema_version":1,"profile_id":"alpha","profile_name":"alpha"})");
    WriteTextFile(Temp.Path / "c.json",
        R"({"schema_version":1,"profile_id":"mid","profile_name":"Beta"})");

    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    // Initialize 恒补建稳定 ID `default`（显示名 "Default"），故列表含四项。
    // 按名称忽略大小写排序：alpha < Beta < Default < Zeta。
    const auto& List = Manager.GetProfiles();
    REQUIRE(List.size() == 4);
    REQUIRE(List[0].Name == "alpha");
    REQUIRE(List[1].Name == "Beta");
    REQUIRE(List[2].Name == "Default");
    REQUIRE(List[3].Name == "Zeta");
}

// ── 损坏 JSON、重复 ID、重复名称不进入列表且原文件仍存在 ──

TEST_CASE("ProfileManager skips corrupt duplicate-id and duplicate-name files",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("skip_invalid");
    // 前缀保证按路径排序时 good 先被处理并成为保留项。
    WriteTextFile(Temp.Path / "01_good.json",
        R"({"schema_version":1,"profile_id":"good","profile_name":"Good"})");
    WriteTextFile(Temp.Path / "02_corrupt.json", "{not valid json");
    WriteTextFile(Temp.Path / "03_dupid.json",
        R"({"schema_version":1,"profile_id":"good","profile_name":"Other"})");
    WriteTextFile(Temp.Path / "04_dupname.json",
        R"({"schema_version":1,"profile_id":"other","profile_name":"good"})");

    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    // 损坏 / 重复项被跳过，仅保留 good；Initialize 另补建的 Default 也在列表中。
    // 按名称忽略大小写排序：Default < Good。
    const auto& List = Manager.GetProfiles();
    REQUIRE(List.size() == 2);
    REQUIRE(List[0].Id == "default");
    REQUIRE(List[1].Id == "good");

    REQUIRE(std::filesystem::exists(Temp.Path / "02_corrupt.json"));
    REQUIRE(std::filesystem::exists(Temp.Path / "03_dupid.json"));
    REQUIRE(std::filesystem::exists(Temp.Path / "04_dupname.json"));
}

// ── CreateProfile 连续编号，删除后复用最小空缺编号 ──

TEST_CASE("ProfileManager CreateProfile numbers and reuses freed slots",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("create_number");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    auto First = Manager.CreateProfile();
    REQUIRE(First.IsOk());
    REQUIRE(std::move(First).TakeValue().Info.Id == "untitled_1");

    auto Second = Manager.CreateProfile();
    REQUIRE(Second.IsOk());
    REQUIRE(std::move(Second).TakeValue().Info.Id == "untitled_2");

    // 当前项是 untitled_2；切回 untitled_1 再删除它。
    REQUIRE(Manager.ActivateProfile("untitled_1").IsOk());
    REQUIRE(Manager.DeleteActiveProfile().IsOk());
    REQUIRE_FALSE(std::filesystem::exists(Temp.Path / "untitled_1.json"));

    // 再次创建应复用编号 1。
    auto Third = Manager.CreateProfile();
    REQUIRE(Third.IsOk());
    REQUIRE(std::move(Third).TakeValue().Info.Id == "untitled_1");
    REQUIRE(std::filesystem::exists(Temp.Path / "untitled_1.json"));
}

// ── 新建 profile 为空配置，不复制当前规则 ──

TEST_CASE("ProfileManager CreateProfile yields empty profile",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("create_empty");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    // 给当前 default 添加一条规则并保存。
    SMappingProfile WithRule;
    WithRule.SchemaVersion = 1;
    SMappingRule Rule;
    Rule.Id = "r1";
    Rule.Input.ControlId = "button_south";
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "A", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    WithRule.Rules.push_back(Rule);
    REQUIRE(Manager.SaveActiveProfile(WithRule).IsOk());

    auto Created = Manager.CreateProfile();
    REQUIRE(Created.IsOk());
    REQUIRE(std::move(Created).TakeValue().Profile.Rules.empty());
}

// ── SaveActiveProfile 只改当前文件并强制保留 manager 的 ID/名称 ──

TEST_CASE("ProfileManager SaveActiveProfile forces identity and touches only current file",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("save_forces");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    // 增加第二个配置并记录其字节。
    REQUIRE(Manager.CreateProfile().IsOk());  // untitled_1 成为当前项
    const StdString UntitledBefore = ReadTextFile(Temp.Path / "untitled_1.json");

    // 切回 default 保存一个身份字段被篡改的 snapshot。
    REQUIRE(Manager.ActivateProfile("default").IsOk());
    SMappingProfile Snapshot;
    Snapshot.SchemaVersion = 1;
    Snapshot.Id = "hacked";
    Snapshot.Name = "Hacked";
    REQUIRE(Manager.SaveActiveProfile(Snapshot).IsOk());

    // 另一个文件未被改动。
    REQUIRE(ReadTextFile(Temp.Path / "untitled_1.json") == UntitledBefore);

    // default.json 中身份被强制回权威值。
    ZProfileManager Verify;
    auto Reload = Verify.LoadProfile(Temp.Path / "default.json");
    REQUIRE(Reload.IsOk());
    auto Loaded = std::move(Reload).TakeValue();
    REQUIRE(Loaded.Id == "default");
    REQUIRE(Loaded.Name == "Default");
}

// ── ActivateProfile 更新状态文件；未知/损坏目标保持当前 ID ──

TEST_CASE("ProfileManager ActivateProfile updates state and preserves current on failure",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("activate");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());
    REQUIRE(Manager.CreateProfile().IsOk());  // untitled_1

    REQUIRE(Manager.ActivateProfile("default").IsOk());
    REQUIRE(ReadTextFile(Temp.Path / "active_profile.txt") == "default\n");

    // 未知 ID：失败，当前 ID 不变。
    REQUIRE(Manager.ActivateProfile("does_not_exist").IsErr());
    REQUIRE(Manager.GetActiveProfileInfo()->Id == "default");

    // 目标文件损坏：加载失败，当前 ID 和状态文件不变。
    WriteTextFile(Temp.Path / "untitled_1.json", "{corrupt");
    REQUIRE(Manager.ActivateProfile("untitled_1").IsErr());
    REQUIRE(Manager.GetActiveProfileInfo()->Id == "default");
    REQUIRE(ReadTextFile(Temp.Path / "active_profile.txt") == "default\n");
}

// ── RenameActiveProfile 保存新名称但保持 ID、路径和规则 ──

TEST_CASE("ProfileManager RenameActiveProfile keeps id path and rules",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("rename");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());
    // Default 不可重命名，先新建一个普通配置作为当前项。
    REQUIRE(Manager.CreateProfile().IsOk());  // untitled_1 成为当前项

    SMappingProfile Snapshot;
    Snapshot.SchemaVersion = 1;
    SMappingRule Rule;
    Rule.Id = "r1";
    Rule.Input.ControlId = "button_south";
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = "A", .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    Snapshot.Rules.push_back(Rule);

    auto Renamed = Manager.RenameActiveProfile(Snapshot, "  My Profile  ");
    REQUIRE(Renamed.IsOk());
    auto Loaded = std::move(Renamed).TakeValue();
    REQUIRE(Loaded.Info.Id == "untitled_1");
    REQUIRE(Loaded.Info.Name == "My Profile");
    REQUIRE(Loaded.Info.FilePath == Temp.Path / "untitled_1.json");
    REQUIRE(Loaded.Profile.Rules.size() == 1);

    // 落盘确认：新名称写入，ID 与规则保持。
    ZProfileManager Verify;
    auto Reload = Verify.LoadProfile(Temp.Path / "untitled_1.json");
    REQUIRE(Reload.IsOk());
    auto Persisted = std::move(Reload).TakeValue();
    REQUIRE(Persisted.Id == "untitled_1");
    REQUIRE(Persisted.Name == "My Profile");
    REQUIRE(Persisted.Rules.size() == 1);
}

// ── Default 不可重命名：底层拒绝且 CanRenameActiveProfile 语义正确 ──

TEST_CASE("ProfileManager RenameActiveProfile refuses to rename the default profile",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("rename_default");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());  // 当前项为 Default

    // Default 是当前项：不可重命名。
    REQUIRE_FALSE(Manager.CanRenameActiveProfile());

    SMappingProfile Snapshot;
    Snapshot.SchemaVersion = 1;
    REQUIRE(Manager.RenameActiveProfile(Snapshot, "Renamed Default").IsErr());
    // 显示名保持不变，文件未被改名。
    REQUIRE(Manager.GetActiveProfileInfo()->Name == "Default");

    // 新建普通配置后当前项可重命名。
    REQUIRE(Manager.CreateProfile().IsOk());
    REQUIRE(Manager.CanRenameActiveProfile());

    // 切回 Default：再次不可重命名。
    REQUIRE(Manager.ActivateProfile("default").IsOk());
    REQUIRE_FALSE(Manager.CanRenameActiveProfile());
}

// ── 空白名与忽略大小写重名返回错误，不改变文件 ──

TEST_CASE("ProfileManager RenameActiveProfile rejects blank and duplicate names",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("rename_reject");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());  // default 名为 "Default"
    REQUIRE(Manager.CreateProfile().IsOk());          // untitled_1 成为当前项

    SMappingProfile Snapshot;
    Snapshot.SchemaVersion = 1;

    REQUIRE(Manager.RenameActiveProfile(Snapshot, "   ").IsErr());
    REQUIRE(Manager.RenameActiveProfile(Snapshot, "default").IsErr());  // 与 "Default" 忽略大小写重名

    REQUIRE(Manager.GetActiveProfileInfo()->Name == "Untitled_1");
}

// ── DeleteActiveProfile 删除精确文件并返回第一项回退 snapshot ──

TEST_CASE("ProfileManager DeleteActiveProfile removes file and returns fallback",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("delete");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());  // default
    REQUIRE(Manager.CreateProfile().IsOk());          // untitled_1 成为当前项

    auto Del = Manager.DeleteActiveProfile();
    REQUIRE(Del.IsOk());
    auto Fallback = std::move(Del).TakeValue();
    REQUIRE(Fallback.Info.Id == "default");  // 排序后第一个非当前项

    REQUIRE_FALSE(std::filesystem::exists(Temp.Path / "untitled_1.json"));
    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
    REQUIRE(Manager.GetActiveProfileInfo()->Id == "default");
    REQUIRE(Manager.GetProfiles().size() == 1);
    REQUIRE(ReadTextFile(Temp.Path / "active_profile.txt") == "default\n");
}

// ── 最后一项不可删除 ──

TEST_CASE("ProfileManager DeleteActiveProfile refuses last remaining profile",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("delete_last");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());

    REQUIRE_FALSE(Manager.CanDeleteActiveProfile());
    REQUIRE(Manager.DeleteActiveProfile().IsErr());
    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
}

// ── 已有其他配置但缺少 default 时补建 Default，且不覆盖现有配置 ──

TEST_CASE("ProfileManager Initialize rebuilds default when missing without overwriting",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("rebuild_default");
    // 只有非 default 配置：初始化必须补建 ID default，且不改动现有文件。
    WriteTextFile(Temp.Path / "custom.json",
        R"({"schema_version":1,"profile_id":"custom","profile_name":"Custom"})");
    const StdString CustomBefore = ReadTextFile(Temp.Path / "custom.json");

    ZProfileManager Manager;
    auto Result = Manager.Initialize(Temp.Path);
    REQUIRE(Result.IsOk());

    // 现有配置仍在，字节未变。
    REQUIRE(std::filesystem::exists(Temp.Path / "custom.json"));
    REQUIRE(ReadTextFile(Temp.Path / "custom.json") == CustomBefore);

    // 补建了 ID default。
    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
    auto DefaultIndex = std::find_if(Manager.GetProfiles().begin(), Manager.GetProfiles().end(),
        [](const SProfileInfo& Info) { return Info.Id == "default"; });
    REQUIRE(DefaultIndex != Manager.GetProfiles().end());
    REQUIRE(Manager.GetProfiles().size() == 2);
}

// ── 多配置下 Default 为当前项时底层拒绝删除且文件仍存在 ──

TEST_CASE("ProfileManager DeleteActiveProfile refuses default even with multiple profiles",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("delete_default");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());  // default 为当前项
    REQUIRE(Manager.CreateProfile().IsOk());          // untitled_1 成为当前项

    // 切回 default：即使存在多个配置，Default 也不可删除。
    REQUIRE(Manager.ActivateProfile("default").IsOk());
    REQUIRE(Manager.GetProfiles().size() == 2);
    REQUIRE_FALSE(Manager.CanDeleteActiveProfile());

    // 即便调用方绕过 UI 直接调用，底层仍拒绝并保留文件。
    REQUIRE(Manager.DeleteActiveProfile().IsErr());
    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
    REQUIRE(Manager.GetActiveProfileInfo()->Id == "default");
    REQUIRE(Manager.GetProfiles().size() == 2);
}

// ── 删除文件失败时列表和当前 ID 不变，状态文件恢复 ──

TEST_CASE("ProfileManager DeleteActiveProfile keeps state when file removal fails",
    "[Runtime][ProfileManager]")
{
    STempProfileDir Temp("delete_fail");
    ZProfileManager Manager;
    REQUIRE(Manager.Initialize(Temp.Path).IsOk());  // default
    REQUIRE(Manager.CreateProfile().IsOk());          // untitled_1 成为当前项

    // 把当前项文件替换为非空目录，使单文件 remove 失败（目录非空）。
    std::filesystem::remove(Temp.Path / "untitled_1.json");
    std::filesystem::create_directories(Temp.Path / "untitled_1.json" / "blocker");

    auto Del = Manager.DeleteActiveProfile();
    REQUIRE(Del.IsErr());
    REQUIRE(Manager.GetActiveProfileInfo()->Id == "untitled_1");
    REQUIRE(Manager.GetProfiles().size() == 2);
    REQUIRE(ReadTextFile(Temp.Path / "active_profile.txt") == "untitled_1\n");
}
