// Profile JSON 序列化和反序列化管理器。
// 负责把磁盘上的 JSON profile 转换为 SMappingProfile，以及反向序列化。
// 公开头文件不包含 JSON 库头，JSON 细节封装在 .cpp 内。
//
// 同时作为配置目录、配置列表、当前配置 ID 和配置文件生命周期的唯一管理者：
// 发现目录内的 .json 文件、持久化 active_profile.txt、创建、激活、重命名、删除。
//
// Runtime 层，依赖 Core 数据类型，不依赖 UI、SDL 或平台 API。

#pragma once

#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

// 配置的权威描述：稳定 ID、显示名和磁盘路径。
// 后续保存和删除只使用其中记录的 FilePath，不再依据显示名重新推导。
struct SProfileInfo
{
    StdString Id;
    StdString Name;
    StdPath FilePath;
};

// 一次成功操作同时返回的权威描述与可直接应用到 Runtime 的 snapshot，
// 让调用方无需在操作完成后再次读盘。
struct SLoadedProfile
{
    SProfileInfo Info;
    SMappingProfile Profile;
};

class ZProfileManager final
{
public:
    // 系统回退配置的稳定 ID。该 ID 的配置永不可删除，并且是 active 状态所有回退路径
    // 的最终目标。身份只由此 ID 决定，与显示名或文件名无关（显示名可被用户重命名）。
    static constexpr StdStringView DefaultProfileId = "default";

    // 判断给定 ID 是否为系统回退配置。各层统一复用此语义，禁止散落字符串字面量比较。
    NODISCARD static bool IsDefaultProfile(StdStringView ProfileId) noexcept;

    // ── 单文件 JSON 编解码 API（保持不变，供显式路径读写与测试直接使用）──

    // 从磁盘路径加载 profile
    NODISCARD TResult<SMappingProfile> LoadProfile(const StdPath& ProfilePath) const;

    // 把 profile 序列化并写入磁盘路径
    NODISCARD TResult<void> SaveProfile(const SMappingProfile& Profile, const StdPath& ProfilePath) const;

    // 从 JSON 文本解析 profile
    NODISCARD TResult<SMappingProfile> ParseProfileJson(StdStringView JsonText) const;

    // 把 profile 序列化为 JSON 文本
    NODISCARD TResult<StdString> SerializeProfileJson(const SMappingProfile& Profile) const;

    // ── 集合 API（配置目录、配置列表、当前配置生命周期）──

    // 初始化配置目录：发现文件、修复旧字段、恢复当前项，返回选中项的完整加载结果。
    // 只有目录无法创建/枚举，或最终无任何可用配置时才失败。
    NODISCARD TResult<SLoadedProfile> Initialize(const StdPath& InProfilesDirectory);

    // 返回已排序的稳定配置列表。
    NODISCARD const TVector<SProfileInfo>& GetProfiles() const noexcept;

    // 按当前 ID 查找并返回描述副本；未初始化或未找到时返回空。
    NODISCARD TOptional<SProfileInfo> GetActiveProfileInfo() const;

    // 当且仅当已初始化、配置数量大于 1、且当前项不是 Default 时可删除当前项。
    // Default 是系统回退配置，永不可删除（即使存在多个配置）。
    NODISCARD bool CanDeleteActiveProfile() const noexcept;

    // 当且仅当已初始化、且当前项不是 Default 时可重命名当前项。
    // Default 是系统回退配置，显示名固定不可重命名。
    NODISCARD bool CanRenameActiveProfile() const noexcept;

    // 切换到指定 ID 的配置，成功后持久化 active_profile.txt。
    NODISCARD TResult<SLoadedProfile> ActivateProfile(StdStringView ProfileId);

    // 创建空的 Untitled_N 配置并立即切换过去，不复制当前映射。
    NODISCARD TResult<SLoadedProfile> CreateProfile();

    // 仅修改当前配置的显示名；不改变 ID，也不重命名文件。
    NODISCARD TResult<SLoadedProfile> RenameActiveProfile(
        const SMappingProfile& CurrentSnapshot,
        StdStringView NewName);

    // 删除当前配置及其 JSON 文件，然后固定切换到 Default 回退项。
    // 底层再次拒绝删除 Default，即使调用方绕过 UI 也不能删除该文件。
    NODISCARD TResult<SLoadedProfile> DeleteActiveProfile();

    // 把当前 Runtime snapshot 写回当前配置文件，强制保留 manager 的 ID/名称。
    NODISCARD TResult<void> SaveActiveProfile(const SMappingProfile& CurrentSnapshot);

private:
    // ── 私有辅助函数 ──

    // 校验目录路径非空并创建目录。
    NODISCARD TResult<void> EnsureProfilesDirectory();

    // 枚举直属 .json 文件，逐个加载并构造 SProfileInfo 列表。
    NODISCARD TResult<void> DiscoverProfiles();

    // 读取 active_profile.txt 中记录的 ID（去除首尾空白）。
    NODISCARD TResult<StdString> ReadActiveProfileId() const;

    // 把当前 ID 写入 active_profile.txt。
    NODISCARD TResult<void> WriteActiveProfileId(StdStringView ProfileId) const;

    // 加载指定描述对应的文件，并强制把 snapshot 的 Id/Name 设为权威值。
    NODISCARD TResult<SLoadedProfile> LoadManagedProfile(const SProfileInfo& Info) const;

    // 按 ID 精确匹配查找列表下标。
    NODISCARD TOptional<uint32> FindProfileIndex(StdStringView ProfileId) const;

    // 规范化名称（仅去除首尾空白），校验非空并忽略大小写检查重名。
    // IgnoredProfileId 用于重命名时排除当前项自身。
    NODISCARD TResult<StdString> NormalizeAndValidateName(
        StdStringView Name,
        StdStringView IgnoredProfileId = {}) const;

    // 按名称忽略大小写排序，名称相同时按 ID 排序。
    void SortProfiles();

    // ── 私有状态 ──

    StdPath ProfilesDirectory;
    StdPath ActiveProfileStatePath;
    TVector<SProfileInfo> Profiles;
    StdString ActiveProfileId;
    bool bInitialized = false;
};

}  // namespace MappyZ
