// 自动切换规则的 JSON 持久化：读取、逐条校验修复、原子写入。
// 不切换 profile、不持有任何 UI/平台状态；只把磁盘上的规则文件与内存规则列表互转。
//
// App 层，使用 std::filesystem、标准流与项目已有的 nlohmann/json。公开头不含 JSON 库头。

#pragma once

#include "App/AutoProfileRule.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

// 一次加载的结果：规则列表 + 是否检测到需要在下次成功保存时写回的修复。
// bRepaired 为 true 表示加载时修复了缺失/重复 ID 或跳过了非法/重复项，
// 调用方应在下次成功 mutation 时把规范化后的列表写回磁盘。
struct SLoadedRules
{
    TVector<SAutoProfileRule> Rules;
    bool bRepaired = false;
};

class ZAutoProfileRuleStore final
{
public:
    // 以显式规则文件路径构造。生产路径由调用方解析
    // （AppConfigLocation/automatic_profile_rules.json）；测试注入独立临时路径。
    explicit ZAutoProfileRuleStore(StdPath FilePath);

    // 当前 schema 版本；顶层 JSON 写入此值，读取时容忍未知字段。
    static constexpr int SchemaVersion = 1;

    NODISCARD const StdPath& GetFilePath() const noexcept { return FilePath; }

    // 加载规则：
    //   - 文件不存在视为空规则集，不报错、不创建文件；
    //   - JSON 整体损坏不覆盖原文件，返回错误（调用方按空集处理并向 UI 报错）；
    //   - 逐条校验类型；ProcessName 重新规范化；缺失/重复 ID 生成新 UUID；
    //     同一规范化进程名重复只保留第一条；这些修复置 bRepaired=true。
    NODISCARD TResult<SLoadedRules> Load() const;

    // 原子写入规则：同目录临时文件 -> flush/close -> rename replace。
    // 调用方在写入成功后才提交内存状态并发信号；失败时应保持旧快照。
    NODISCARD TResult<void> Save(const TVector<SAutoProfileRule>& Rules) const;

private:
    StdPath FilePath;
};

}  // namespace MappyZ
