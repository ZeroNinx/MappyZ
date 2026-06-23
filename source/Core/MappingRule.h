// 映射规则数据契约。
// 定义输入匹配条件、输出动作配置和单条映射规则的结构。
// Core 层纯数据类型，不依赖 Runtime、Backend、UI 或平台 API。
// 映射引擎通过此契约接收规则配置，不直接读取 JSON 或 UI 状态。

#pragma once

#include "Core/Action.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

// 映射动作的触发模式
enum class EMappingActionMode
{
    // 按钮按下生成 pressed 动作，抬起生成 released 动作
    PressRelease,

    // 输入保持激活时保持输出状态，后续引擎处理释放
    Hold,

    // 模拟量连续映射，例如摇杆到鼠标移动
    Analog,
};

// 输入匹配条件，描述规则需要匹配的控件和激活阈值
struct SMappingInput
{
    // 控件标识，使用 ControlId 命名空间中的标准名称
    StdString ControlId;

    // 控件的物理类型
    EInputControlType ControlType = EInputControlType::Button;

    // 匹配的事件类型，按钮类规则通常匹配 Pressed，模拟量规则匹配 Changed
    EInputEventType EventType = EInputEventType::Pressed;

    // Trigger 或 Axis1D 激活阈值，超过此值视为激活
    float32 Threshold = 0.5f;

    // Axis1D 或 Axis2D 死区，低于此值视为零输入，合法范围 [0.0f, 1.0f]
    float32 Deadzone = 0.0f;
};

// 输出动作配置，描述规则匹配后生成的动作和参数
struct SMappingOutput
{
    // 输出动作，复用 SAction 定义
    SAction Action;

    // 触发模式
    EMappingActionMode Mode = EMappingActionMode::PressRelease;

    // 模拟量输出灵敏度，用于 Axis2D 到 MouseMove 等场景
    float32 Sensitivity = 1.0f;
};

// 单条映射规则，将一个输入条件绑定到一个输出动作
struct SMappingRule
{
    // 规则标识，在单个 profile 内应稳定且唯一，用于 UI 编辑、保存和日志定位
    StdString Id;

    // 规则显示名称，用于 UI 展示，可为空
    StdString DisplayName;

    // 是否启用此规则，禁用的规则不参与映射
    bool bEnabled = true;

    // 输入匹配条件
    SMappingInput Input;

    // 输出动作配置
    SMappingOutput Output;
};

}  // namespace MappyZ
