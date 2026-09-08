// 自动配置切换的纯数据类型与无副作用策略函数。
// 只描述规则本身与匹配求值，不依赖 QObject、QML、平台 API 或 JSON 库。
//
// 进程名规范化（NormalizeProcessName）与匹配求值（ResolveAutomaticProfileId）是
// 手工输入、平台枚举、持久化加载和前台事件共用的单一权威实现，避免各处散落逻辑。
//
// App 层，仅依赖 Core 数据类型。

#pragma once

#include "Core/ProjectCore.h"

namespace MappyZ
{

// 一条自动切换规则：前台进程匹配某个可执行文件 basename 时切换到指定配置。
struct SAutoProfileRule
{
    StdString Id;           // 独立稳定 ID（UUID 字符串），与进程名解耦，作为行身份
    bool bEnabled = true;   // 仅控制该规则是否参与匹配；false 等同于无此规则
    StdString ProcessName;  // 已规范化的可执行文件 basename，如 "eldenring.exe"
    StdString ProfileId;    // 引用 SProfileInfo::Id，持久化稳定 ID 而非显示名或路径
};

// 一个当前运行的可见顶层应用候选，供进程选择界面回填进程名。
struct SForegroundApplication
{
    StdString ProcessName;  // 规范化后的 basename
    StdString DisplayName;  // 窗口标题或显示名，仅用于展示
    uint32 ProcessId = 0;
};

// 把用户输入或平台返回的可执行标识规范化为稳定 basename：
//   - 去除首尾空白；
//   - 去除成对包裹的引号；
//   - 若含路径分隔符（'/' 或 '\\'）只取最后一段 basename；
//   - ASCII 大小写统一转小写，保留 ".exe" 等后缀；
// 空串、"."、".."、含通配符（'*' '?'）或规范化后不含有效文件名的输入视为非法。
// 非法时返回空 optional，合法时返回规范化结果。
NODISCARD TOptional<StdString> NormalizeProcessName(StdStringView RawName);

// 纯匹配求值：给定前台进程名、规则权威快照和当前存在的 profile ID 集合，
// 返回应当激活的 profile ID。
//   - 仅 bEnabled 且 ProfileId 存在于 AvailableProfileIds、且规范化名精确相等的规则命中；
//   - 命中返回该规则的 ProfileId；未命中、进程名非法或无规则命中时固定返回 "default"。
// 不做子串、通配符或大小写敏感匹配（名称已在规范化阶段统一小写）。
NODISCARD StdString ResolveAutomaticProfileId(
    StdStringView ForegroundProcessName,
    const TVector<SAutoProfileRule>& Rules,
    const TVector<StdString>& AvailableProfileIds);

// 生成一个新的规则稳定 ID（随机 UUID v4 字符串小写形式）。
// 供 UI 添加规则和持久化加载修复缺失/重复 ID 时共用。
NODISCARD StdString GenerateRuleId();

}  // namespace MappyZ
