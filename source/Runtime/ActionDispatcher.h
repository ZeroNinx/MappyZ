// Runtime 动作派发器。
// 接收 SAction 或 TVector<SAction>，调用当前 IOutputBackend，记录派发结果。
// 为 ZMappingSession 提供统一的输出入口，隔离 Runtime 和具体输出后端。
//
// Runtime 层，依赖 Core 和 Backends/Output，不依赖 UI、SDL 或平台 API。

#pragma once

#include "Backends/Output/OutputBackend.h"
#include "Core/Action.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

// 单次派发记录
struct SActionDispatchRecord
{
    SAction Action;
    bool bSucceeded = false;
    StdString Message;
};

// 批量派发汇总
struct SActionDispatchSummary
{
    uint32 RequestedCount = 0;
    uint32 SucceededCount = 0;
    uint32 FailedCount = 0;
    bool bSucceeded = true;
    StdString Message;
};

class ZActionDispatcher final
{
public:
    static constexpr uint32 MaxRecentRecords = 128;

    explicit ZActionDispatcher(IOutputBackend& OutputBackend);

    // 禁止拷贝和移动
    ZActionDispatcher(const ZActionDispatcher&) = delete;
    ZActionDispatcher& operator=(const ZActionDispatcher&) = delete;
    ZActionDispatcher(ZActionDispatcher&&) = delete;
    ZActionDispatcher& operator=(ZActionDispatcher&&) = delete;

    // 派发单个动作，成功返回 Ok，失败返回错误信息
    NODISCARD TResult<void> DispatchAction(const SAction& Action);

    // 批量派发动作，按顺序调用后端，中途失败继续派发剩余
    NODISCARD SActionDispatchSummary DispatchActions(const TVector<SAction>& Actions);

    // 查询是否启用派发
    NODISCARD bool IsEnabled() const noexcept;

    // 设置是否启用派发
    void SetEnabled(bool bEnabled) noexcept;

    // 透传输出后端当前状态
    NODISCARD SOutputBackendStatus GetOutputStatus() const;

    // 返回最近派发记录的快照拷贝
    NODISCARD TVector<SActionDispatchRecord> ListRecentRecords() const;

    // 返回当前记录数量
    NODISCARD uint32 GetRecentRecordCount() const noexcept;

    // 清空派发记录，不改变 enabled 状态
    void ClearRecentRecords();

private:
    void AppendRecord(SActionDispatchRecord Record);

    IOutputBackend& Backend;
    bool bDispatchEnabled = true;
    TVector<SActionDispatchRecord> RecentRecords;
};

}  // namespace MappyZ
