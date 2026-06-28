// 摇杆方向合成器实现。

#include "Runtime/StickDirectionSynthesizer.h"

#include <cmath>

namespace MappyZ
{

TVector<SInputEvent> ZStickDirectionSynthesizer::ProcessAxis2D(const SInputEvent& Event)
{
    if (Event.ControlType != EInputControlType::Axis2D)
    {
        return {};
    }

    // 只处理 left_stick 和 right_stick
    const bool bIsLeftStick = (Event.ControlId == "left_stick");
    const bool bIsRightStick = (Event.ControlId == "right_stick");
    if (!bIsLeftStick && !bIsRightStick)
    {
        return {};
    }

    const StdString CacheKey = Event.DeviceId.Value + ":" + Event.ControlId;
    auto& State = DirectionCache[CacheKey];

    const StdString& Prefix = Event.ControlId;

    // 根据阈值计算当前帧各方向的目标状态
    const bool bWantRight = Event.Axis2D.X > DirectionThreshold;
    const bool bWantLeft = Event.Axis2D.X < -DirectionThreshold;
    const bool bWantDown = Event.Axis2D.Y > DirectionThreshold;
    const bool bWantUp = Event.Axis2D.Y < -DirectionThreshold;

    TVector<SInputEvent> Results;

    // 同轴切换时必须先释放旧方向再按下新方向，
    // 否则下游 mapping 会短暂出现两个方向同时按下。
    // 分两轮：先收集所有 release 事件，再收集所有 press 事件。

    // 第一轮：release（状态从 pressed 变为 released）
    if (bWantUp != State.bUp && !bWantUp)
    {
        State.bUp = false;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_up", false));
    }
    if (bWantDown != State.bDown && !bWantDown)
    {
        State.bDown = false;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_down", false));
    }
    if (bWantLeft != State.bLeft && !bWantLeft)
    {
        State.bLeft = false;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_left", false));
    }
    if (bWantRight != State.bRight && !bWantRight)
    {
        State.bRight = false;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_right", false));
    }

    // 第二轮：press（状态从 released 变为 pressed）
    if (bWantUp != State.bUp && bWantUp)
    {
        State.bUp = true;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_up", true));
    }
    if (bWantDown != State.bDown && bWantDown)
    {
        State.bDown = true;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_down", true));
    }
    if (bWantLeft != State.bLeft && bWantLeft)
    {
        State.bLeft = true;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_left", true));
    }
    if (bWantRight != State.bRight && bWantRight)
    {
        State.bRight = true;
        Results.push_back(MakeDirectionEvent(Event, Prefix + "_right", true));
    }

    return Results;
}

void ZStickDirectionSynthesizer::ClearDevice(const SDeviceId& DeviceId)
{
    const StdString LeftKey = DeviceId.Value + ":left_stick";
    const StdString RightKey = DeviceId.Value + ":right_stick";
    DirectionCache.erase(LeftKey);
    DirectionCache.erase(RightKey);
}

void ZStickDirectionSynthesizer::ClearAll()
{
    DirectionCache.clear();
}

SInputEvent ZStickDirectionSynthesizer::MakeDirectionEvent(
    const SInputEvent& SourceEvent,
    const StdString& DirectionControlId,
    bool bPressed)
{
    SInputEvent DirectionEvent;
    DirectionEvent.DeviceId = SourceEvent.DeviceId;
    DirectionEvent.ControlId = DirectionControlId;
    DirectionEvent.ControlType = EInputControlType::Button;
    DirectionEvent.EventType = bPressed ? EInputEventType::Pressed : EInputEventType::Released;
    DirectionEvent.Value = bPressed ? 1.0f : 0.0f;
    DirectionEvent.Timestamp = SourceEvent.Timestamp;
    return DirectionEvent;
}

}  // namespace MappyZ
