// SAutoProfileRule 纯策略实现：进程名规范化与匹配求值。
// 只使用标准库，不依赖 Qt、平台 API 或 JSON。

#include "App/AutoProfileRule.h"

#include <algorithm>
#include <random>

namespace MappyZ
{

namespace
{

// 去除首尾 ASCII 空白，不改动中间内容。
StdString TrimAscii(StdStringView Text)
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
char AsciiToLower(char Ch)
{
    if (Ch >= 'A' && Ch <= 'Z') return static_cast<char>(Ch - 'A' + 'a');
    return Ch;
}

}  // namespace

TOptional<StdString> NormalizeProcessName(StdStringView RawName)
{
    StdString Text = TrimAscii(RawName);

    // 去除成对包裹的引号（单/双），只处理最外层一对。
    if (Text.size() >= 2)
    {
        const char Front = Text.front();
        const char Back = Text.back();
        if ((Front == '"' && Back == '"') || (Front == '\'' && Back == '\''))
        {
            Text = TrimAscii(StdStringView(Text).substr(1, Text.size() - 2));
        }
    }

    if (Text.empty())
    {
        return std::nullopt;
    }

    // 含路径分隔符时只取最后一段 basename。
    const size_t LastSep = Text.find_last_of("/\\");
    if (LastSep != StdString::npos)
    {
        Text = Text.substr(LastSep + 1);
    }

    if (Text.empty() || Text == "." || Text == "..")
    {
        return std::nullopt;
    }

    // 通配符不受支持：出现即视为非法，避免误命中。
    if (Text.find('*') != StdString::npos || Text.find('?') != StdString::npos)
    {
        return std::nullopt;
    }

    // ASCII 大小写统一转小写。
    StdString Normalized;
    Normalized.reserve(Text.size());
    for (char Ch : Text)
    {
        Normalized.push_back(AsciiToLower(Ch));
    }

    return Normalized;
}

StdString ResolveAutomaticProfileId(
    StdStringView ForegroundProcessName,
    const TVector<SAutoProfileRule>& Rules,
    const TVector<StdString>& AvailableProfileIds)
{
    // 固定回退目标：稳定 ID "default"（与 ZProfileManager::DefaultProfileId 一致，
    // 但本纯函数不依赖 Runtime 头，直接使用字面量以保持零依赖）。
    const StdString Fallback = "default";

    auto Normalized = NormalizeProcessName(ForegroundProcessName);
    if (!Normalized.has_value())
    {
        return Fallback;
    }

    auto ProfileExists = [&AvailableProfileIds](const StdString& Id) {
        return std::find(AvailableProfileIds.begin(), AvailableProfileIds.end(), Id)
            != AvailableProfileIds.end();
    };

    for (const auto& Rule : Rules)
    {
        if (!Rule.bEnabled) continue;
        if (Rule.ProcessName != *Normalized) continue;
        if (!ProfileExists(Rule.ProfileId)) continue;
        return Rule.ProfileId;
    }

    return Fallback;
}

StdString GenerateRuleId()
{
    // 随机 UUID v4：使用 thread_local 引擎，避免每次构造 seed。不追求密码学强度，
    // 仅用于给规则行一个稳定唯一身份。
    static thread_local std::mt19937_64 Engine{std::random_device{}()};
    std::uniform_int_distribution<uint32> Nibble(0, 15);

    static constexpr char Hex[] = "0123456789abcdef";
    // 格式 xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx，version=4，variant=8..b。
    StdString Id;
    Id.reserve(36);
    for (int Pos = 0; Pos < 36; ++Pos)
    {
        if (Pos == 8 || Pos == 13 || Pos == 18 || Pos == 23)
        {
            Id.push_back('-');
        }
        else if (Pos == 14)
        {
            Id.push_back('4');
        }
        else if (Pos == 19)
        {
            Id.push_back(Hex[(Nibble(Engine) & 0x3) | 0x8]);
        }
        else
        {
            Id.push_back(Hex[Nibble(Engine)]);
        }
    }
    return Id;
}

}  // namespace MappyZ
