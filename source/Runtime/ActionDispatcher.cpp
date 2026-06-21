// ZActionDispatcher 实现。
// 所有非预期路径（disabled、后端失败）都输出日志以方便定位问题。

#include "Runtime/ActionDispatcher.h"

#include <cstdio>
#include <utility>

namespace ZeroMapper
{

ZActionDispatcher::ZActionDispatcher(IOutputBackend& OutputBackend)
    : Backend(OutputBackend)
{
}

TResult<void> ZActionDispatcher::DispatchAction(const SAction& Action)
{
    if (!bDispatchEnabled)
    {
        std::fprintf(stderr, "[ActionDispatcher] 调试: 派发器已禁用，跳过动作\n");
        AppendRecord(SActionDispatchRecord{
            .Action = Action,
            .bSucceeded = false,
            .Message = "dispatcher disabled",
        });
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, "dispatcher disabled"));
    }

    auto Result = Backend.SendAction(Action);
    if (Result.IsOk())
    {
        AppendRecord(SActionDispatchRecord{
            .Action = Action,
            .bSucceeded = true,
            .Message = "ok",
        });
        return TResult<void>::Ok();
    }

    auto& Error = Result.Failure();
    std::fprintf(stderr, "[ActionDispatcher] 警告: 后端派发失败: \"%s\"\n",
        Error.Message.c_str());
    AppendRecord(SActionDispatchRecord{
        .Action = Action,
        .bSucceeded = false,
        .Message = Error.Message,
    });
    return TResult<void>::Err(MakeError(Error.Code, Error.Message, Error.ContextPath));
}

SActionDispatchSummary ZActionDispatcher::DispatchActions(const TVector<SAction>& Actions)
{
    SActionDispatchSummary Summary;
    Summary.RequestedCount = static_cast<uint32>(Actions.size());

    for (const auto& Action : Actions)
    {
        auto Result = DispatchAction(Action);
        if (Result.IsOk())
        {
            ++Summary.SucceededCount;
        }
        else
        {
            ++Summary.FailedCount;
            Summary.bSucceeded = false;
        }
    }

    if (Summary.FailedCount > 0)
    {
        Summary.Message = std::to_string(Summary.FailedCount) + " of "
            + std::to_string(Summary.RequestedCount) + " failed";
    }
    else
    {
        Summary.Message = "ok";
    }

    return Summary;
}

bool ZActionDispatcher::IsEnabled() const noexcept
{
    return bDispatchEnabled;
}

void ZActionDispatcher::SetEnabled(bool bEnabled) noexcept
{
    bDispatchEnabled = bEnabled;
}

SOutputBackendStatus ZActionDispatcher::GetOutputStatus() const
{
    return Backend.GetStatus();
}

TVector<SActionDispatchRecord> ZActionDispatcher::ListRecentRecords() const
{
    return RecentRecords;
}

uint32 ZActionDispatcher::GetRecentRecordCount() const noexcept
{
    return static_cast<uint32>(RecentRecords.size());
}

void ZActionDispatcher::ClearRecentRecords()
{
    RecentRecords.clear();
}

void ZActionDispatcher::AppendRecord(SActionDispatchRecord Record)
{
    if (RecentRecords.size() >= MaxRecentRecords)
    {
        RecentRecords.erase(RecentRecords.begin());
    }
    RecentRecords.push_back(std::move(Record));
}

}  // namespace ZeroMapper
