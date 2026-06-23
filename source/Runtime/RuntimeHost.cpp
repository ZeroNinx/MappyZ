// ZRuntimeHost 实现。
// 组合 Runtime 组件，管理启动/停止生命周期。
// 所有非预期路径（重复 Start、非 Running 下 Pump、backend 启动失败）都输出日志。

#include "Runtime/RuntimeHost.h"

#include <cstdio>
#include <utility>

namespace MappyZ
{

ZRuntimeHost::ZRuntimeHost(IInputBackend& InputBackend, IOutputBackend& OutputBackend)
    : InputBackend(InputBackend)
    , OutputBackend(OutputBackend)
    , EventQueue(InputBackend)
    , Dispatcher(OutputBackend)
    , Session(Dispatcher)
    , Pump(EventQueue, Session)
{
}

ZRuntimeHost::~ZRuntimeHost()
{
    Stop();
}

TResult<void> ZRuntimeHost::Start(SRuntimeHostStartOptions Options)
{
    if (HostState == ERuntimeHostState::Running)
    {
        std::fprintf(stderr, "[RuntimeHost] 调试: 已处于 Running 状态，跳过重复启动\n");
        return TResult<void>::Ok();
    }

    bool bDidAttach = false;

    if (Options.bAttachEventQueue && !EventQueue.IsAttached())
    {
        EventQueue.Attach();
        bDidAttach = true;
    }

    if (Options.bStartInputBackend && !InputBackend.IsRunning())
    {
        auto Result = InputBackend.Start();
        if (!Result)
        {
            // 启动失败，回滚本次 attach
            if (bDidAttach)
            {
                EventQueue.Detach();
            }

            HostState = ERuntimeHostState::Error;
            HostMessage = Result.Failure().Message;
            std::fprintf(stderr, "[RuntimeHost] 错误: 输入后端启动失败: \"%s\"\n",
                HostMessage.c_str());
            return TResult<void>::Err(std::move(Result).TakeFailure());
        }

        bDidStartInputBackend = true;
    }

    Session.SetEnabled(Options.bEnableMapping);

    HostState = ERuntimeHostState::Running;
    HostMessage = "running";
    return TResult<void>::Ok();
}

void ZRuntimeHost::Stop()
{
    // 只停止由 host 自己启动的 input backend
    if (bDidStartInputBackend && InputBackend.IsRunning())
    {
        InputBackend.Stop();
        bDidStartInputBackend = false;
    }

    // 无论 HostState 如何，始终 detach 已 attached 的 event queue，
    // 避免通过 GetEventQueue().Attach() 手动 attach 后析构时留下悬空回调
    if (EventQueue.IsAttached())
    {
        EventQueue.Detach();
    }

    HostState = ERuntimeHostState::Stopped;
    HostMessage = "stopped";
}

bool ZRuntimeHost::IsRunning() const noexcept
{
    return HostState == ERuntimeHostState::Running;
}

SRuntimeHostStatus ZRuntimeHost::GetStatus() const
{
    return SRuntimeHostStatus{
        .State = HostState,
        .Message = HostMessage,
        .OutputStatus = OutputBackend.GetStatus(),
    };
}

SRuntimeEventPumpSummary ZRuntimeHost::PumpOnce()
{
    if (HostState != ERuntimeHostState::Running)
    {
        std::fprintf(stderr, "[RuntimeHost] 调试: 非 Running 状态下调用 PumpOnce，跳过\n");
        return {};
    }

    return Pump.PumpOnce();
}

void ZRuntimeHost::ReplaceProfile(SMappingProfile Profile)
{
    Session.ReplaceProfile(std::move(Profile));
}

SMappingProfile ZRuntimeHost::GetProfileSnapshot() const
{
    return Session.GetProfileSnapshot();
}

void ZRuntimeHost::SetMappingEnabled(bool bEnabled)
{
    Session.SetEnabled(bEnabled);
}

bool ZRuntimeHost::IsMappingEnabled() const noexcept
{
    return Session.IsEnabled();
}

ZBackendEventQueue& ZRuntimeHost::GetEventQueue()
{
    return EventQueue;
}

ZRuntimeEventPump& ZRuntimeHost::GetEventPump()
{
    return Pump;
}

ZMappingSession& ZRuntimeHost::GetMappingSession()
{
    return Session;
}

ZActionDispatcher& ZRuntimeHost::GetActionDispatcher()
{
    return Dispatcher;
}

}  // namespace MappyZ
