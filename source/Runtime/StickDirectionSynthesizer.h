// 摇杆方向合成器。
// 将 Axis2D 事件拆分为 4 个方向的虚拟 Button press/release 事件。
// 每个方向独立判定：轴值超过阈值时 press，回落时 release。
// 同一轴正负方向互斥，不同轴（X/Y）的方向可同时 press。
// Runtime 层，不依赖 Qt、QML、SDL、Win32。

#pragma once

#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZStickDirectionSynthesizer final
{
public:
    // 方向激活阈值，超过此值视为 pressed
    static constexpr float32 DirectionThreshold = 0.5f;

    // 处理一个 Axis2D 事件，返回 0~4 个方向变化的虚拟 Button 事件。
    // 仅在状态发生变化时生成事件，避免每帧重复。
    // 非 Axis2D 事件或 ControlId 不是 left_stick/right_stick 时返回空。
    NODISCARD TVector<SInputEvent> ProcessAxis2D(const SInputEvent& Event);

    // 清除指定设备的方向缓存（设备断开时调用）
    void ClearDevice(const SDeviceId& DeviceId);

    // 清除所有设备的方向缓存
    void ClearAll();

private:
    // 单个摇杆的 4 方向状态
    struct SStickDirectionState
    {
        bool bUp = false;
        bool bDown = false;
        bool bLeft = false;
        bool bRight = false;
    };

    // 缓存 key = "deviceId:controlId"
    THashMap<StdString, SStickDirectionState> DirectionCache;

    // 生成单个方向的 Button 事件
    static SInputEvent MakeDirectionEvent(
        const SInputEvent& SourceEvent,
        const StdString& DirectionControlId,
        bool bPressed);
};

}  // namespace MappyZ
