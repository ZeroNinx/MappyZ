// 映射引擎。
// 消费单个 SInputEvent 和一个 SMappingProfile 快照，输出命中规则对应的 TVector<SAction>。
// 无状态纯函数式设计，不保存上一次输入、不保存按键状态、不访问 Runtime。
// Core 层，不依赖 Runtime、Backend、UI 或平台 API。

#pragma once

#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZMappingEngine final
{
public:
    // 根据输入事件和 profile 配置，输出命中规则对应的动作列表
    // 输出动作按 profile 中规则顺序追加，保持 deterministic
    // 无匹配或 profile/rule 禁用时返回空列表
    NODISCARD TVector<SAction> MapInput(const SInputEvent& Event, const SMappingProfile& Profile) const;

private:
    // 判断输入事件是否匹配规则的输入条件
    NODISCARD bool DoesRuleMatchInput(const SInputEvent& Event, const SMappingRule& Rule) const;

    // 根据匹配的规则和输入事件构建输出动作
    NODISCARD TOptional<SAction> BuildAction(const SInputEvent& Event, const SMappingRule& Rule) const;

    // PressRelease 模式的动作构建
    NODISCARD TOptional<SAction> BuildPressReleaseAction(const SInputEvent& Event, const SMappingRule& Rule) const;

    // Analog 模式的动作构建
    NODISCARD TOptional<SAction> BuildAnalogAction(const SInputEvent& Event, const SMappingRule& Rule) const;
};

}  // namespace MappyZ
