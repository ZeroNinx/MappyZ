// ZNullOutputBackend 实现。
// 所有非预期路径（None 动作、非 Ready 状态）都输出日志以方便定位问题。

#include "Backends/Output/NullOutputBackend.h"

#include <cstdio>
#include <utility>

namespace MappyZ
{

ZNullOutputBackend::ZNullOutputBackend()
    : CurrentStatus{.State = EOutputBackendState::Ready, .Message = "ready"}
{
}

TResult<void> ZNullOutputBackend::SendAction(const SAction& Action)
{
    // 非 Ready 状态拒绝发送
    if (CurrentStatus.State != EOutputBackendState::Ready)
    {
        std::fprintf(stderr, "[NullOutputBackend] 警告: 后端状态非 Ready，拒绝发送动作 (状态: %d, 消息: \"%s\")\n",
            static_cast<int>(CurrentStatus.State), CurrentStatus.Message.c_str());
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, CurrentStatus.Message));
    }

    // None 动作视为无效
    if (Action.Type == EActionType::None)
    {
        std::fprintf(stderr, "[NullOutputBackend] 警告: 收到 EActionType::None 动作，拒绝记录\n");
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument, "invalid action type: none"));
    }

    RecordedActions.push_back(Action);
    return TResult<void>::Ok();
}

SOutputBackendStatus ZNullOutputBackend::GetStatus() const
{
    return CurrentStatus;
}

TVector<SAction> ZNullOutputBackend::ListActions() const
{
    return RecordedActions;
}

uint32 ZNullOutputBackend::GetActionCount() const noexcept
{
    return static_cast<uint32>(RecordedActions.size());
}

void ZNullOutputBackend::ClearActions()
{
    RecordedActions.clear();
}

void ZNullOutputBackend::SetStatus(SOutputBackendStatus Status)
{
    CurrentStatus = std::move(Status);
}

void ZNullOutputBackend::SetReady(StdString Message)
{
    CurrentStatus.State = EOutputBackendState::Ready;
    CurrentStatus.Message = std::move(Message);
}

void ZNullOutputBackend::SetUnavailable(StdString Message)
{
    CurrentStatus.State = EOutputBackendState::Unavailable;
    CurrentStatus.Message = std::move(Message);
}

void ZNullOutputBackend::SetError(StdString Message)
{
    CurrentStatus.State = EOutputBackendState::Error;
    CurrentStatus.Message = std::move(Message);
}

}  // namespace MappyZ
