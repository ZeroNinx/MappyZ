// 输入事件数据契约。
// 定义从输入后端传入运行时的标准化事件结构，所有后端必须将原始事件转换为 SInputEvent。
// Core 层不依赖任何后端或平台 API，映射引擎只通过此契约接收输入。

#pragma once

#include "Core/DeviceId.h"
#include "Core/ProjectCore.h"

#include <chrono>

namespace MappyZ
{

// 控件的物理类型
enum class EInputControlType
{
    Button,   // 数字按钮，值为 0 或 1
    Axis1D,   // 单轴模拟量，例如单独的水平或垂直轴
    Axis2D,   // 双轴模拟量，例如摇杆
    Trigger,  // 扳机，范围 [0, 1]
    Hat,      // 方向键，离散方向值
};

// 输入事件的逻辑类型
enum class EInputEventType
{
    Pressed,   // 按钮或方向键按下
    Released,  // 按钮或方向键抬起
    Changed,   // 模拟量发生变化（摇杆、扳机）
};

// 摇杆双轴值，用于 Axis2D 类型的控件
struct SAxis2DValue
{
    float32 X = 0.0f;
    float32 Y = 0.0f;
};

// 标准化输入事件，所有后端的原始事件都必须转换为此结构
struct SInputEvent
{
    // 产生此事件的设备标识
    SDeviceId DeviceId;

    // 控件标识，使用 ControlId 命名空间中的标准名称
    StdString ControlId;

    // 控件的物理类型
    EInputControlType ControlType = EInputControlType::Button;

    // 事件的逻辑类型
    EInputEventType EventType = EInputEventType::Changed;

    // 单轴值，用于 Button（0/1）、Trigger（0~1）和 Axis1D
    float32 Value = 0.0f;

    // 双轴值，仅在 ControlType 为 Axis2D 时有效
    SAxis2DValue Axis2D;

    // 事件产生的时间戳，使用 steady_clock 保证单调递增
    std::chrono::steady_clock::time_point Timestamp;
};

}  // namespace MappyZ
