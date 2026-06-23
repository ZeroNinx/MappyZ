// ZInputRuntime 实现。
// 所有非预期路径（重复订阅、状态不一致）都输出日志以方便定位问题。

#include "Runtime/InputRuntime.h"

#include <cstdio>

namespace MappyZ
{

ZInputRuntime::ZInputRuntime(IInputBackend& Backend)
    : Backend(Backend)
{
}

ZInputRuntime::~ZInputRuntime()
{
    // best-effort 清理回调，避免 runtime 销毁后回调悬垂
    Detach();
}

void ZInputRuntime::Attach()
{
    if (bAttached)
    {
        std::fprintf(stderr, "[InputRuntime] 调试: 已处于 attached 状态，跳过重复订阅\n");
        return;
    }

    // 订阅后端输入事件回调
    // 本轮约定 runtime attach 期间不允许外部改写后端的 OnInputEvent
    Backend.OnInputEvent = [this](const SInputEvent& Event)
    {
        HandleInputEvent(Event);
    };

    bAttached = true;
}

void ZInputRuntime::Detach()
{
    if (!bAttached)
    {
        return;
    }

    // 清空后端回调，避免 runtime 销毁后回调指向已释放内存
    Backend.OnInputEvent = nullptr;

    bAttached = false;
}

bool ZInputRuntime::IsAttached() const noexcept
{
    return bAttached;
}

TVector<SInputEvent> ZInputRuntime::ListRecentEvents() const
{
    return RecentEvents;
}

uint32 ZInputRuntime::GetRecentEventCount() const noexcept
{
    return static_cast<uint32>(RecentEvents.size());
}

TOptional<SInputEvent> ZInputRuntime::FindControlState(const SDeviceId& DeviceId, StdStringView ControlId) const
{
    auto Key = MakeControlStateKey(DeviceId, ControlId);
    auto Iterator = ControlStates.find(Key);

    if (Iterator == ControlStates.end())
    {
        return std::nullopt;
    }

    return Iterator->second;
}

uint32 ZInputRuntime::GetTrackedControlCount() const noexcept
{
    return static_cast<uint32>(ControlStates.size());
}

void ZInputRuntime::Clear()
{
    RecentEvents.clear();
    ControlStates.clear();
}

void ZInputRuntime::HandleInputEvent(const SInputEvent& Event)
{
    // 更新当前控件状态快照，同一设备同一控件只保留最后一次事件
    auto Key = MakeControlStateKey(Event.DeviceId, Event.ControlId);
    ControlStates[Key] = Event;

    // 追加到最近事件列表
    RecentEvents.push_back(Event);

    // 超过容量上限时丢弃最旧事件
    if (RecentEvents.size() > MaxRecentEvents)
    {
        RecentEvents.erase(RecentEvents.begin());
    }
}

StdString ZInputRuntime::MakeControlStateKey(const SDeviceId& DeviceId, StdStringView ControlId)
{
    // 使用 Unit Separator (0x1F) 作为分隔符，避免 DeviceId 和 ControlId 拼接产生歧义
    StdString Key;
    Key.reserve(DeviceId.Value.size() + 1 + ControlId.size());
    Key += DeviceId.Value;
    Key += '\x1F';
    Key.append(ControlId.data(), ControlId.size());
    return Key;
}

}  // namespace MappyZ
