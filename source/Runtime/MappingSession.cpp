// ZMappingSession 实现。
// 所有非预期路径（无动作、派发失败）都输出日志以方便定位问题。

#include "Runtime/MappingSession.h"

#include <cstdio>
#include <utility>

namespace MappyZ
{

ZMappingSession::ZMappingSession(ZActionDispatcher& Dispatcher)
    : ActionDispatcher(Dispatcher)
{
}

SMappingSessionResult ZMappingSession::HandleInputEvent(const SInputEvent& Event)
{
    SMappingSessionResult Result;

    auto Actions = Engine.MapInput(Event, ActiveProfile);
    Result.ActionCount = static_cast<uint32>(Actions.size());

    if (Actions.empty())
    {
        Result.Message = "no actions";
        AppendRecord(SMappingSessionRecord{.Event = Event, .Result = Result});
        return Result;
    }

    Result.bMapped = true;
    Result.DispatchSummary = ActionDispatcher.DispatchActions(Actions);

    if (Result.DispatchSummary.bSucceeded)
    {
        Result.bDispatched = true;
        Result.Message = "ok";
    }
    else
    {
        std::fprintf(stderr, "[MappingSession] 警告: 派发部分失败: \"%s\"\n",
            Result.DispatchSummary.Message.c_str());
        Result.Message = Result.DispatchSummary.Message;
    }

    AppendRecord(SMappingSessionRecord{.Event = Event, .Result = Result});
    return Result;
}

void ZMappingSession::ReplaceProfile(SMappingProfile Profile)
{
    ActiveProfile = std::move(Profile);
}

SMappingProfile ZMappingSession::GetProfileSnapshot() const
{
    return ActiveProfile;
}

TVector<SMappingSessionRecord> ZMappingSession::ListRecentRecords() const
{
    return RecentRecords;
}

uint32 ZMappingSession::GetRecentRecordCount() const noexcept
{
    return static_cast<uint32>(RecentRecords.size());
}

void ZMappingSession::ClearRecentRecords()
{
    RecentRecords.clear();
}

void ZMappingSession::AppendRecord(SMappingSessionRecord Record)
{
    if (RecentRecords.size() >= MaxRecentRecords)
    {
        RecentRecords.erase(RecentRecords.begin());
    }
    RecentRecords.push_back(std::move(Record));
}

}  // namespace MappyZ
