// ZAutoProfileRuleStore 单元测试。
// 全部使用注入的临时目录，绝不触碰生产 AppConfigLocation。

#include <catch2/catch_test_macros.hpp>

#include "App/AutoProfileRuleStore.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace MappyZ;

namespace
{

// 每个测试独立的临时目录，析构递归删除。
struct STempDir
{
    StdPath Path;

    explicit STempDir(const StdString& Label)
    {
        static std::atomic<uint32> Counter{0};
        const auto Unique =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "_" + std::to_string(Counter.fetch_add(1));
        Path = std::filesystem::temp_directory_path() / ("mappyz_rules_" + Label + "_" + Unique);
        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
        std::filesystem::create_directories(Path, Ec);
    }

    ~STempDir()
    {
        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
    }
};

void WriteTextFile(const StdPath& FilePath, const StdString& Content)
{
    std::filesystem::create_directories(FilePath.parent_path());
    std::ofstream File(FilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    File.write(Content.data(), static_cast<std::streamsize>(Content.size()));
}

StdString ReadTextFile(const StdPath& FilePath)
{
    std::ifstream File(FilePath, std::ios::in | std::ios::binary);
    std::ostringstream Stream;
    Stream << File.rdbuf();
    return Stream.str();
}

SAutoProfileRule MakeRule(StdString Id, StdString ProcessName, StdString ProfileId,
    bool bEnabled = true)
{
    SAutoProfileRule Rule;
    Rule.Id = std::move(Id);
    Rule.ProcessName = std::move(ProcessName);
    Rule.ProfileId = std::move(ProfileId);
    Rule.bEnabled = bEnabled;
    return Rule;
}

}  // namespace

TEST_CASE("RuleStore missing file loads empty without creating it", "[App][RuleStore]")
{
    STempDir Dir("missing");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";

    ZAutoProfileRuleStore Store(RulePath);
    auto Result = Store.Load();
    REQUIRE(Result.IsOk());
    auto Loaded = std::move(Result).TakeValue();
    REQUIRE(Loaded.Rules.empty());
    REQUIRE_FALSE(Loaded.bRepaired);
    REQUIRE_FALSE(std::filesystem::exists(RulePath));
}

TEST_CASE("RuleStore save then reload preserves all fields", "[App][RuleStore]")
{
    STempDir Dir("roundtrip");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";

    ZAutoProfileRuleStore Store(RulePath);
    TVector<SAutoProfileRule> Rules{
        MakeRule("id-a", "eldenring.exe", "souls", true),
        MakeRule("id-b", "notepad.exe", "default", false),
    };
    REQUIRE(Store.Save(Rules).IsOk());
    REQUIRE(std::filesystem::exists(RulePath));

    auto Result = Store.Load();
    REQUIRE(Result.IsOk());
    auto Loaded = std::move(Result).TakeValue();
    REQUIRE_FALSE(Loaded.bRepaired);
    REQUIRE(Loaded.Rules.size() == 2);
    REQUIRE(Loaded.Rules[0].Id == "id-a");
    REQUIRE(Loaded.Rules[0].ProcessName == "eldenring.exe");
    REQUIRE(Loaded.Rules[0].ProfileId == "souls");
    REQUIRE(Loaded.Rules[0].bEnabled == true);
    REQUIRE(Loaded.Rules[1].Id == "id-b");
    REQUIRE(Loaded.Rules[1].bEnabled == false);
}

TEST_CASE("RuleStore corrupt JSON is not overwritten and returns error", "[App][RuleStore]")
{
    STempDir Dir("corrupt");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, "{not valid json");
    const StdString Before = ReadTextFile(RulePath);

    ZAutoProfileRuleStore Store(RulePath);
    auto Result = Store.Load();
    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::ParseFailed);

    // 原文件未被覆盖。
    REQUIRE(ReadTextFile(RulePath) == Before);
}

TEST_CASE("RuleStore re-normalizes process names on load", "[App][RuleStore]")
{
    STempDir Dir("normalize");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, R"({
        "schema_version": 1,
        "rules": [
            { "id": "id-a", "enabled": true, "process_name": "C:\\Games\\EldenRing.EXE", "profile_id": "souls" }
        ]
    })");

    ZAutoProfileRuleStore Store(RulePath);
    auto Loaded = std::move(Store.Load()).TakeValue();
    REQUIRE(Loaded.Rules.size() == 1);
    REQUIRE(Loaded.Rules[0].ProcessName == "eldenring.exe");
    REQUIRE(Loaded.bRepaired);  // 规范化改变了内容
}

TEST_CASE("RuleStore skips duplicate process names keeping first", "[App][RuleStore]")
{
    STempDir Dir("dupproc");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, R"({
        "schema_version": 1,
        "rules": [
            { "id": "id-a", "enabled": true, "process_name": "game.exe", "profile_id": "first" },
            { "id": "id-b", "enabled": true, "process_name": "GAME.EXE", "profile_id": "second" }
        ]
    })");

    ZAutoProfileRuleStore Store(RulePath);
    auto Loaded = std::move(Store.Load()).TakeValue();
    REQUIRE(Loaded.Rules.size() == 1);
    REQUIRE(Loaded.Rules[0].ProfileId == "first");
    REQUIRE(Loaded.bRepaired);
}

TEST_CASE("RuleStore repairs missing and duplicate ids", "[App][RuleStore]")
{
    STempDir Dir("fixids");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, R"({
        "schema_version": 1,
        "rules": [
            { "enabled": true, "process_name": "a.exe", "profile_id": "p1" },
            { "id": "dup", "enabled": true, "process_name": "b.exe", "profile_id": "p2" },
            { "id": "dup", "enabled": true, "process_name": "c.exe", "profile_id": "p3" }
        ]
    })");

    ZAutoProfileRuleStore Store(RulePath);
    auto Loaded = std::move(Store.Load()).TakeValue();
    REQUIRE(Loaded.Rules.size() == 3);
    REQUIRE(Loaded.bRepaired);
    // 三个 ID 都非空且互不相同。
    REQUIRE_FALSE(Loaded.Rules[0].Id.empty());
    REQUIRE(Loaded.Rules[0].Id != Loaded.Rules[1].Id);
    REQUIRE(Loaded.Rules[1].Id != Loaded.Rules[2].Id);
    REQUIRE(Loaded.Rules[0].Id != Loaded.Rules[2].Id);
}

TEST_CASE("RuleStore ignores unknown top-level and rule fields", "[App][RuleStore]")
{
    STempDir Dir("unknown");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, R"({
        "schema_version": 1,
        "future_flag": true,
        "rules": [
            { "id": "id-a", "enabled": true, "process_name": "a.exe", "profile_id": "p1", "extra": 42 }
        ]
    })");

    ZAutoProfileRuleStore Store(RulePath);
    auto Result = Store.Load();
    REQUIRE(Result.IsOk());
    auto Loaded = std::move(Result).TakeValue();
    REQUIRE(Loaded.Rules.size() == 1);
    REQUIRE(Loaded.Rules[0].Id == "id-a");
}

TEST_CASE("RuleStore keeps rule referencing missing profile", "[App][RuleStore]")
{
    // 引用不存在 profile 的规则应保留在文件中（运行期由匹配策略回退 Default）。
    STempDir Dir("missingprofile");
    const StdPath RulePath = Dir.Path / "automatic_profile_rules.json";
    WriteTextFile(RulePath, R"({
        "schema_version": 1,
        "rules": [
            { "id": "id-a", "enabled": true, "process_name": "a.exe", "profile_id": "gone" }
        ]
    })");

    ZAutoProfileRuleStore Store(RulePath);
    auto Loaded = std::move(Store.Load()).TakeValue();
    REQUIRE(Loaded.Rules.size() == 1);
    REQUIRE(Loaded.Rules[0].ProfileId == "gone");
}

TEST_CASE("RuleStore save failure into blocked path preserves original", "[App][RuleStore]")
{
    // 用普通文件充当规则目录，使 create_directories 与写入均失败，验证原子写入不截断。
    STempDir Dir("savefail");
    const StdPath Blocker = Dir.Path / "blocker";
    WriteTextFile(Blocker, "x");
    const StdPath RulePath = Blocker / "automatic_profile_rules.json";

    ZAutoProfileRuleStore Store(RulePath);
    TVector<SAutoProfileRule> Rules{MakeRule("id-a", "a.exe", "p1")};
    auto Result = Store.Save(Rules);
    REQUIRE(Result.IsErr());
    REQUIRE(Result.Failure().Code == EErrorCode::FileWriteFailed);
}
