// Profile JSON 序列化和反序列化管理器。
// 负责把磁盘上的 JSON profile 转换为 SMappingProfile，以及反向序列化。
// 公开头文件不包含 JSON 库头，JSON 细节封装在 .cpp 内。
//
// Runtime 层，依赖 Core 数据类型，不依赖 UI、SDL 或平台 API。

#pragma once

#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZProfileManager final
{
public:
    // 从磁盘路径加载 profile
    NODISCARD TResult<SMappingProfile> LoadProfile(const StdPath& ProfilePath) const;

    // 把 profile 序列化并写入磁盘路径
    NODISCARD TResult<void> SaveProfile(const SMappingProfile& Profile, const StdPath& ProfilePath) const;

    // 从 JSON 文本解析 profile
    NODISCARD TResult<SMappingProfile> ParseProfileJson(StdStringView JsonText) const;

    // 把 profile 序列化为 JSON 文本
    NODISCARD TResult<StdString> SerializeProfileJson(const SMappingProfile& Profile) const;
};

}  // namespace MappyZ
