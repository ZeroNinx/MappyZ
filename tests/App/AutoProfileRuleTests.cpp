// NormalizeProcessName 与 ResolveAutomaticProfileId 纯策略单元测试。
// 不触碰磁盘、Qt 或平台 API。

#include <catch2/catch_test_macros.hpp>

#include "App/AutoProfileRule.h"

using namespace MappyZ;

namespace
{

// 便捷构造一条启用规则。
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

// ── NormalizeProcessName ──

TEST_CASE("NormalizeProcessName lowercases and keeps extension", "[App][AutoProfile]")
{
    auto Result = NormalizeProcessName("EldenRing.exe");
    REQUIRE(Result.has_value());
    REQUIRE(*Result == "eldenring.exe");
}

TEST_CASE("NormalizeProcessName trims surrounding whitespace", "[App][AutoProfile]")
{
    auto Result = NormalizeProcessName("   game.exe  ");
    REQUIRE(Result.has_value());
    REQUIRE(*Result == "game.exe");
}

TEST_CASE("NormalizeProcessName strips paired quotes", "[App][AutoProfile]")
{
    REQUIRE(NormalizeProcessName("\"game.exe\"").value() == "game.exe");
    REQUIRE(NormalizeProcessName("'Game.EXE'").value() == "game.exe");
}

TEST_CASE("NormalizeProcessName takes basename from full path", "[App][AutoProfile]")
{
    REQUIRE(NormalizeProcessName("C:\\Games\\Elden Ring\\eldenring.exe").value()
        == "eldenring.exe");
    REQUIRE(NormalizeProcessName("/usr/bin/Game.exe").value() == "game.exe");
    // 带引号的完整路径同样解析。
    REQUIRE(NormalizeProcessName("\"C:\\Program Files\\App\\App.exe\"").value()
        == "app.exe");
}

TEST_CASE("NormalizeProcessName rejects invalid inputs", "[App][AutoProfile]")
{
    REQUIRE_FALSE(NormalizeProcessName("").has_value());
    REQUIRE_FALSE(NormalizeProcessName("   ").has_value());
    REQUIRE_FALSE(NormalizeProcessName(".").has_value());
    REQUIRE_FALSE(NormalizeProcessName("..").has_value());
    REQUIRE_FALSE(NormalizeProcessName("*.exe").has_value());
    REQUIRE_FALSE(NormalizeProcessName("game?.exe").has_value());
    // 仅有路径分隔符、结尾无 basename。
    REQUIRE_FALSE(NormalizeProcessName("C:\\Games\\").has_value());
}

// ── ResolveAutomaticProfileId ──

TEST_CASE("ResolveAutomaticProfileId matches enabled exact name", "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules{MakeRule("id1", "eldenring.exe", "souls")};
    TVector<StdString> Available{"default", "souls"};

    REQUIRE(ResolveAutomaticProfileId("EldenRing.exe", Rules, Available) == "souls");
    // 前台名带完整路径也能命中（内部规范化）。
    REQUIRE(ResolveAutomaticProfileId("C:\\x\\EldenRing.exe", Rules, Available) == "souls");
}

TEST_CASE("ResolveAutomaticProfileId falls back to default when disabled",
    "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules{MakeRule("id1", "game.exe", "gp", /*bEnabled=*/false)};
    TVector<StdString> Available{"default", "gp"};

    REQUIRE(ResolveAutomaticProfileId("game.exe", Rules, Available) == "default");
}

TEST_CASE("ResolveAutomaticProfileId falls back when profile missing",
    "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules{MakeRule("id1", "game.exe", "gone")};
    TVector<StdString> Available{"default"};  // "gone" 不存在

    REQUIRE(ResolveAutomaticProfileId("game.exe", Rules, Available) == "default");
}

TEST_CASE("ResolveAutomaticProfileId does not substring match", "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules{MakeRule("id1", "game.exe", "gp")};
    TVector<StdString> Available{"default", "gp"};

    REQUIRE(ResolveAutomaticProfileId("mygame.exe", Rules, Available) == "default");
    REQUIRE(ResolveAutomaticProfileId("game.exe.bak", Rules, Available) == "default");
}

TEST_CASE("ResolveAutomaticProfileId falls back on empty or invalid foreground",
    "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules{MakeRule("id1", "game.exe", "gp")};
    TVector<StdString> Available{"default", "gp"};

    REQUIRE(ResolveAutomaticProfileId("", Rules, Available) == "default");
    REQUIRE(ResolveAutomaticProfileId("*", Rules, Available) == "default");
}

TEST_CASE("ResolveAutomaticProfileId with no rules returns default", "[App][AutoProfile]")
{
    TVector<SAutoProfileRule> Rules;
    TVector<StdString> Available{"default"};

    REQUIRE(ResolveAutomaticProfileId("anything.exe", Rules, Available) == "default");
}

// ── GenerateRuleId ──

TEST_CASE("GenerateRuleId produces unique 36-char uuids", "[App][AutoProfile]")
{
    const StdString A = GenerateRuleId();
    const StdString B = GenerateRuleId();
    REQUIRE(A.size() == 36);
    REQUIRE(B.size() == 36);
    REQUIRE(A != B);
    // version 4 与 variant 位。
    REQUIRE(A[14] == '4');
    REQUIRE((A[19] == '8' || A[19] == '9' || A[19] == 'a' || A[19] == 'b'));
}
