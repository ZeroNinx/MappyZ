// ZRuntimeEventPump 实现。
// 从 BackendEventQueue drain 事件，按 FIFO 逐个分发到 handler 和 MappingSession。
// 所有非预期路径（payload 类型不匹配）输出警告日志。

#include "Runtime/RuntimeEventPump.h"

#include <cstdio>
#include <utility>

namespace MappyZ
{

ZRuntimeEventPump::ZRuntimeEventPump(
    ZBackendEventQueue& EventQueue,
    ZMappingSession& MappingSession)
    : EventQueue(EventQueue)
    , MappingSession(MappingSession)
{
}

SRuntimeEventPumpSummary ZRuntimeEventPump::PumpOnce()
{
    return PumpEvents(EventQueue.DrainEvents());
}

SRuntimeEventPumpSummary ZRuntimeEventPump::PumpEvents(TVector<SBackendEvent> Events)
{
    SRuntimeEventPumpSummary Summary;
    Summary.DrainedEventCount = static_cast<uint32>(Events.size());

    if (Events.empty())
    {
        return Summary;
    }

    for (const auto& Event : Events)
    {
        ProcessEvent(Event, Summary);
    }

    return Summary;
}

void ZRuntimeEventPump::SetDeviceConnectedHandler(
    std::function<void(const SDeviceInfo&)> Handler)
{
    DeviceConnectedHandler = std::move(Handler);
}

void ZRuntimeEventPump::SetDeviceDisconnectedHandler(
    std::function<void(const SDeviceId&)> Handler)
{
    DeviceDisconnectedHandler = std::move(Handler);
}

void ZRuntimeEventPump::SetInputEventHandler(
    std::function<void(const SInputEvent&)> Handler)
{
    InputEventHandler = std::move(Handler);
}

TVector<SRuntimeEventPumpRecord> ZRuntimeEventPump::ListRecentRecords() const
{
    return RecentRecords;
}

uint32 ZRuntimeEventPump::GetRecentRecordCount() const noexcept
{
    return static_cast<uint32>(RecentRecords.size());
}

void ZRuntimeEventPump::ClearRecentRecords()
{
    RecentRecords.clear();
}

void ZRuntimeEventPump::ProcessEvent(
    const SBackendEvent& Event,
    SRuntimeEventPumpSummary& Summary)
{
    SRuntimeEventPumpRecord Record;
    Record.Event = Event;

    switch (Event.Type)
    {
    case EBackendEventType::DeviceConnected:
    {
        auto* DeviceInfo = std::get_if<SDeviceInfo>(&Event.Payload);
        if (!DeviceInfo)
        {
            std::fprintf(stderr,
                "[RuntimeEventPump] 警告: DeviceConnected 事件的 payload 类型不匹配，跳过\n");
            ++Summary.InvalidEventCount;
            Record.Message = "invalid payload";
            AppendRecord(std::move(Record));
            return;
        }

        ++Summary.DeviceConnectedCount;
        Record.bHandled = true;

        if (DeviceConnectedHandler)
        {
            DeviceConnectedHandler(*DeviceInfo);
        }

        break;
    }

    case EBackendEventType::DeviceDisconnected:
    {
        auto* DeviceId = std::get_if<SDeviceId>(&Event.Payload);
        if (!DeviceId)
        {
            std::fprintf(stderr,
                "[RuntimeEventPump] 警告: DeviceDisconnected 事件的 payload 类型不匹配，跳过\n");
            ++Summary.InvalidEventCount;
            Record.Message = "invalid payload";
            AppendRecord(std::move(Record));
            return;
        }

        ++Summary.DeviceDisconnectedCount;
        Record.bHandled = true;

        if (DeviceDisconnectedHandler)
        {
            DeviceDisconnectedHandler(*DeviceId);
        }

        break;
    }

    case EBackendEventType::Input:
    {
        auto* InputEvent = std::get_if<SInputEvent>(&Event.Payload);
        if (!InputEvent)
        {
            std::fprintf(stderr,
                "[RuntimeEventPump] 警告: Input 事件的 payload 类型不匹配，跳过\n");
            ++Summary.InvalidEventCount;
            Record.Message = "invalid payload";
            AppendRecord(std::move(Record));
            return;
        }

        ++Summary.InputEventCount;
        Record.bHandled = true;

        // handler 先于 mapping session 执行，便于 UI/状态层看到原始输入
        if (InputEventHandler)
        {
            InputEventHandler(*InputEvent);
        }

        Record.MappingResult = MappingSession.HandleInputEvent(*InputEvent);

        if (Record.MappingResult.bMapped)
        {
            ++Summary.MappedInputCount;

            if (Record.MappingResult.bDispatched)
            {
                ++Summary.DispatchedInputCount;
            }
            else
            {
                ++Summary.FailedDispatchInputCount;
            }
        }

        break;
    }

    default:
    {
        std::fprintf(stderr,
            "[RuntimeEventPump] 警告: 未知的事件类型 %d，跳过\n",
            static_cast<int>(Event.Type));
        ++Summary.InvalidEventCount;
        Record.Message = "unknown event type";
        AppendRecord(std::move(Record));
        return;
    }
    }

    AppendRecord(std::move(Record));
}

void ZRuntimeEventPump::AppendRecord(SRuntimeEventPumpRecord Record)
{
    if (RecentRecords.size() >= MaxRecentRecords)
    {
        RecentRecords.erase(RecentRecords.begin());
    }
    RecentRecords.push_back(std::move(Record));
}

}  // namespace MappyZ
