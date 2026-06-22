// ZBackendEventQueue 实现。
// 回调入队使用 mutex 保护，DrainEvents 使用 swap 取走 pending vector。
// 所有非预期路径（重复 attach、回调被覆盖）都输出日志以方便定位问题。

#include "Runtime/BackendEventQueue.h"

#include <cstdio>
#include <utility>

namespace ZeroMapper
{

ZBackendEventQueue::ZBackendEventQueue(IInputBackend& Backend)
    : Backend(Backend)
{
}

ZBackendEventQueue::~ZBackendEventQueue()
{
    Detach();
}

void ZBackendEventQueue::Attach()
{
    if (bAttached)
    {
        std::fprintf(stderr, "[BackendEventQueue] 调试: 已处于 attached 状态，跳过重复订阅\n");
        return;
    }

    // 检查后端回调是否已被其他模块占用
    if (Backend.OnDeviceConnected)
    {
        std::fprintf(stderr, "[BackendEventQueue] 警告: 后端 OnDeviceConnected 已有回调，将被覆盖\n");
    }
    if (Backend.OnDeviceDisconnected)
    {
        std::fprintf(stderr, "[BackendEventQueue] 警告: 后端 OnDeviceDisconnected 已有回调，将被覆盖\n");
    }
    if (Backend.OnInputEvent)
    {
        std::fprintf(stderr, "[BackendEventQueue] 警告: 后端 OnInputEvent 已有回调，将被覆盖\n");
    }

    Backend.OnDeviceConnected = [this](const SDeviceInfo& DeviceInfo)
    {
        EnqueueDeviceConnected(DeviceInfo);
    };

    Backend.OnDeviceDisconnected = [this](const SDeviceId& DeviceId)
    {
        EnqueueDeviceDisconnected(DeviceId);
    };

    Backend.OnInputEvent = [this](const SInputEvent& Event)
    {
        EnqueueInputEvent(Event);
    };

    bAttached = true;
}

void ZBackendEventQueue::Detach()
{
    if (!bAttached)
    {
        return;
    }

    Backend.OnDeviceConnected = nullptr;
    Backend.OnDeviceDisconnected = nullptr;
    Backend.OnInputEvent = nullptr;

    bAttached = false;
}

bool ZBackendEventQueue::IsAttached() const noexcept
{
    return bAttached;
}

TVector<SBackendEvent> ZBackendEventQueue::DrainEvents()
{
    TVector<SBackendEvent> Result;
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        Result.swap(PendingEvents);
    }
    return Result;
}

uint32 ZBackendEventQueue::GetPendingEventCount() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return static_cast<uint32>(PendingEvents.size());
}

void ZBackendEventQueue::Clear()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    PendingEvents.clear();
}

void ZBackendEventQueue::EnqueueDeviceConnected(const SDeviceInfo& DeviceInfo)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    PendingEvents.push_back(SBackendEvent{
        .Type = EBackendEventType::DeviceConnected,
        .Payload = DeviceInfo,
    });
}

void ZBackendEventQueue::EnqueueDeviceDisconnected(const SDeviceId& DeviceId)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    PendingEvents.push_back(SBackendEvent{
        .Type = EBackendEventType::DeviceDisconnected,
        .Payload = DeviceId,
    });
}

void ZBackendEventQueue::EnqueueInputEvent(const SInputEvent& Event)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    PendingEvents.push_back(SBackendEvent{
        .Type = EBackendEventType::Input,
        .Payload = Event,
    });
}

}  // namespace ZeroMapper
