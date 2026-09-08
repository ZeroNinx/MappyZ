// ZAutoProfileRuleStore 实现。
// 读取时逐条校验并规范化，尽量修复而非拒绝整份文件；写入使用同目录临时文件 + rename
// 的原子替换。JSON 细节封装在此 .cpp，公开头不暴露 nlohmann/json。

#include "App/AutoProfileRuleStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace MappyZ
{

using Json = nlohmann::json;

namespace
{

// 顶层与规则字段键名，读写共用。
constexpr auto kSchemaVersionKey = "schema_version";
constexpr auto kRulesKey = "rules";
constexpr auto kRuleIdKey = "id";
constexpr auto kRuleEnabledKey = "enabled";
constexpr auto kRuleProcessNameKey = "process_name";
constexpr auto kRuleProfileIdKey = "profile_id";

}  // namespace

ZAutoProfileRuleStore::ZAutoProfileRuleStore(StdPath InFilePath)
    : FilePath(std::move(InFilePath))
{
}

TResult<SLoadedRules> ZAutoProfileRuleStore::Load() const
{
    std::error_code Ec;
    if (!std::filesystem::exists(FilePath, Ec))
    {
        // 文件不存在是正常首次运行：空规则集，不报错、不创建文件。
        return TResult<SLoadedRules>::Ok(SLoadedRules{});
    }

    std::ifstream File(FilePath, std::ios::in | std::ios::binary);
    if (!File.is_open())
    {
        return TResult<SLoadedRules>::Err(
            MakeError(EErrorCode::FileReadFailed, "cannot open rule file", FilePath));
    }

    std::ostringstream Buffer;
    Buffer << File.rdbuf();
    const StdString Text = Buffer.str();

    Json Root = Json::parse(Text, nullptr, /*allow_exceptions=*/false);
    if (Root.is_discarded() || !Root.is_object())
    {
        // JSON 整体损坏：不覆盖原文件，返回可诊断错误。调用方按空集处理并向 UI 报错，
        // 待用户首次成功 mutation 时才以有效内容替换。
        return TResult<SLoadedRules>::Err(
            MakeError(EErrorCode::ParseFailed, "rule file is not valid JSON", FilePath));
    }

    SLoadedRules Result;

    // 未知顶层字段忽略；rules 缺失或非数组时视为空集。
    auto RulesIt = Root.find(kRulesKey);
    if (RulesIt == Root.end() || !RulesIt->is_array())
    {
        return TResult<SLoadedRules>::Ok(std::move(Result));
    }

    std::unordered_set<StdString> SeenRuleIds;
    std::unordered_set<StdString> SeenProcessNames;

    for (const auto& Entry : *RulesIt)
    {
        if (!Entry.is_object())
        {
            Result.bRepaired = true;
            continue;
        }

        // ProcessName 必须存在且为字符串，且能通过统一规范化。
        auto ProcessIt = Entry.find(kRuleProcessNameKey);
        if (ProcessIt == Entry.end() || !ProcessIt->is_string())
        {
            std::fprintf(stderr, "[AutoProfileRuleStore] 警告: 规则缺少有效 process_name，已跳过\n");
            Result.bRepaired = true;
            continue;
        }
        auto Normalized = NormalizeProcessName(ProcessIt->get<StdString>());
        if (!Normalized.has_value())
        {
            std::fprintf(stderr, "[AutoProfileRuleStore] 警告: 规则 process_name 规范化失败，已跳过\n");
            Result.bRepaired = true;
            continue;
        }
        // 磁盘上的原始名与规范化结果不一致时视为一次修复，触发下次写回持久化规范形式。
        if (ProcessIt->get<StdString>() != *Normalized)
        {
            Result.bRepaired = true;
        }

        // ProfileId 必须存在且为字符串（可引用已不存在的 profile，运行期再标记 invalid）。
        auto ProfileIt = Entry.find(kRuleProfileIdKey);
        if (ProfileIt == Entry.end() || !ProfileIt->is_string())
        {
            std::fprintf(stderr, "[AutoProfileRuleStore] 警告: 规则缺少有效 profile_id，已跳过\n");
            Result.bRepaired = true;
            continue;
        }

        // 同一规范化进程名只保留文件中第一条。
        if (SeenProcessNames.count(*Normalized) > 0)
        {
            std::fprintf(stderr, "[AutoProfileRuleStore] 警告: 规则 process_name 重复，已跳过后续项\n");
            Result.bRepaired = true;
            continue;
        }

        SAutoProfileRule Rule;

        // Enabled 缺失或非 bool 时默认 true。
        auto EnabledIt = Entry.find(kRuleEnabledKey);
        if (EnabledIt != Entry.end() && EnabledIt->is_boolean())
        {
            Rule.bEnabled = EnabledIt->get<bool>();
        }
        else if (EnabledIt != Entry.end())
        {
            Result.bRepaired = true;
        }

        // ID 缺失、非字符串或重复时生成新 UUID 并标记修复。
        auto IdIt = Entry.find(kRuleIdKey);
        StdString Id;
        if (IdIt != Entry.end() && IdIt->is_string())
        {
            Id = IdIt->get<StdString>();
        }
        if (Id.empty() || SeenRuleIds.count(Id) > 0)
        {
            Id = GenerateRuleId();
            Result.bRepaired = true;
        }
        SeenRuleIds.insert(Id);
        SeenProcessNames.insert(*Normalized);

        Rule.Id = std::move(Id);
        Rule.ProcessName = std::move(*Normalized);
        Rule.ProfileId = ProfileIt->get<StdString>();

        Result.Rules.push_back(std::move(Rule));
    }

    return TResult<SLoadedRules>::Ok(std::move(Result));
}

TResult<void> ZAutoProfileRuleStore::Save(const TVector<SAutoProfileRule>& Rules) const
{
    Json Root = Json::object();
    Root[kSchemaVersionKey] = SchemaVersion;

    Json RuleArray = Json::array();
    for (const auto& Rule : Rules)
    {
        Json Entry = Json::object();
        Entry[kRuleIdKey] = Rule.Id;
        Entry[kRuleEnabledKey] = Rule.bEnabled;
        Entry[kRuleProcessNameKey] = Rule.ProcessName;
        Entry[kRuleProfileIdKey] = Rule.ProfileId;
        RuleArray.push_back(std::move(Entry));
    }
    Root[kRulesKey] = std::move(RuleArray);

    const StdString Text = Root.dump(2);

    // 确保父目录存在。
    const StdPath Parent = FilePath.parent_path();
    if (!Parent.empty())
    {
        std::error_code Ec;
        std::filesystem::create_directories(Parent, Ec);
        if (Ec && !std::filesystem::exists(Parent))
        {
            return TResult<void>::Err(
                MakeError(EErrorCode::FileWriteFailed, "cannot create rule directory", FilePath));
        }
    }

    // 同目录临时文件：写入 -> flush/close -> rename replace，避免中断留下半个 JSON。
    StdPath TempPath = FilePath;
    TempPath += ".tmp";

    {
        std::ofstream Out(TempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!Out.is_open())
        {
            return TResult<void>::Err(
                MakeError(EErrorCode::FileWriteFailed, "cannot open temp rule file", TempPath));
        }
        Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
        Out.flush();
        if (!Out.good())
        {
            Out.close();
            std::error_code Ec;
            std::filesystem::remove(TempPath, Ec);
            return TResult<void>::Err(
                MakeError(EErrorCode::FileWriteFailed, "write error on temp rule file", TempPath));
        }
    }  // Out 析构关闭文件

    std::error_code Ec;
    std::filesystem::rename(TempPath, FilePath, Ec);
    if (Ec)
    {
        // 原子替换失败：清理临时文件，保留原文件不变。
        std::error_code Cleanup;
        std::filesystem::remove(TempPath, Cleanup);
        return TResult<void>::Err(
            MakeError(EErrorCode::FileWriteFailed, "cannot replace rule file", FilePath));
    }

    return TResult<void>::Ok();
}

}  // namespace MappyZ
