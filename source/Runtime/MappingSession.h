// Runtime 映射会话。
// 持有当前 active SMappingProfile 快照，接收 SInputEvent，调用 ZMappingEngine 生成动作，
// 再交给 ZActionDispatcher 派发。为 Runtime 提供"一次输入事件的完整映射入口"。
//
// Runtime 层，依赖 Core 和 ZActionDispatcher，不依赖 UI、SDL 或平台 API。

#pragma once

#include "Core/InputEvent.h"
#include "Core/MappingEngine.h"
#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"
#include "Runtime/ActionDispatcher.h"

namespace MappyZ
{

// 单次映射处理结果
struct SMappingSessionResult
{
    uint32 ActionCount = 0;
    SActionDispatchSummary DispatchSummary;
    bool bMapped = false;
    bool bDispatched = false;
    StdString Message;
};

// 单次映射处理记录
struct SMappingSessionRecord
{
    SInputEvent Event;
    SMappingSessionResult Result;
};

class ZMappingSession final
{
public:
    static constexpr uint32 MaxRecentRecords = 128;

    explicit ZMappingSession(ZActionDispatcher& Dispatcher);

    // 禁止拷贝和移动
    ZMappingSession(const ZMappingSession&) = delete;
    ZMappingSession& operator=(const ZMappingSession&) = delete;
    ZMappingSession(ZMappingSession&&) = delete;
    ZMappingSession& operator=(ZMappingSession&&) = delete;

    // 处理单个输入事件，执行映射和派发
    NODISCARD SMappingSessionResult HandleInputEvent(const SInputEvent& Event);

    // 替换当前 active profile 快照
    void ReplaceProfile(SMappingProfile Profile);

    // 返回当前 profile 快照拷贝
    NODISCARD SMappingProfile GetProfileSnapshot() const;

    // 查询是否启用映射
    NODISCARD bool IsEnabled() const noexcept;

    // 设置是否启用映射
    void SetEnabled(bool bEnabled) noexcept;

    // 返回最近处理记录的快照拷贝
    NODISCARD TVector<SMappingSessionRecord> ListRecentRecords() const;

    // 返回当前记录数量
    NODISCARD uint32 GetRecentRecordCount() const noexcept;

    // 清空处理记录，不改变 enabled 状态或 profile
    void ClearRecentRecords();

private:
    void AppendRecord(SMappingSessionRecord Record);

    ZActionDispatcher& ActionDispatcher;
    ZMappingEngine Engine;
    SMappingProfile ActiveProfile;
    bool bSessionEnabled = true;
    TVector<SMappingSessionRecord> RecentRecords;
};

}  // namespace MappyZ
