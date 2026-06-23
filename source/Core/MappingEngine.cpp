// ZMappingEngine 实现。
// 所有非预期路径（不支持的 mode、payload 不匹配、未实现的 action type）都输出日志以方便定位问题。

#include "Core/MappingEngine.h"

#include <cmath>
#include <cstdio>

namespace MappyZ
{

TVector<SAction> ZMappingEngine::MapInput(const SInputEvent& Event, const SMappingProfile& Profile) const
{
    TVector<SAction> Actions;

    if (!Profile.bEnabled)
    {
        return Actions;
    }

    for (const auto& Rule : Profile.Rules)
    {
        if (!Rule.bEnabled)
        {
            continue;
        }

        if (!DoesRuleMatchInput(Event, Rule))
        {
            continue;
        }

        auto Action = BuildAction(Event, Rule);
        if (Action.has_value())
        {
            Actions.push_back(std::move(*Action));
        }
    }

    return Actions;
}

bool ZMappingEngine::DoesRuleMatchInput(const SInputEvent& Event, const SMappingRule& Rule) const
{
    // ControlId 精确匹配
    if (Event.ControlId != Rule.Input.ControlId)
    {
        return false;
    }

    // ControlType 精确匹配
    if (Event.ControlType != Rule.Input.ControlType)
    {
        return false;
    }

    // 根据控件类型和动作模式判断事件类型是否被接受
    switch (Rule.Input.ControlType)
    {
    case EInputControlType::Button:
    case EInputControlType::Hat:
    {
        // Button/Hat 的 PressRelease 规则接受 Pressed 和 Released，忽略 Changed
        if (Event.EventType == EInputEventType::Changed)
        {
            return false;
        }
        return true;
    }

    case EInputControlType::Trigger:
    case EInputControlType::Axis1D:
    {
        // Trigger/Axis1D 只接受 Changed 事件
        if (Event.EventType != EInputEventType::Changed)
        {
            return false;
        }
        return true;
    }

    case EInputControlType::Axis2D:
    {
        // Axis2D 只接受 Changed 事件
        if (Event.EventType != EInputEventType::Changed)
        {
            return false;
        }
        return true;
    }

    default:
        std::fprintf(stderr, "[MappingEngine] 警告: 未知的 ControlType，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return false;
    }
}

TOptional<SAction> ZMappingEngine::BuildAction(const SInputEvent& Event, const SMappingRule& Rule) const
{
    switch (Rule.Output.Mode)
    {
    case EMappingActionMode::PressRelease:
        return BuildPressReleaseAction(Event, Rule);

    case EMappingActionMode::Analog:
        return BuildAnalogAction(Event, Rule);

    case EMappingActionMode::Hold:
        // Hold 模式需要 Runtime 状态配合，本轮不实现
        std::fprintf(stderr, "[MappingEngine] 调试: Hold 模式暂未实现，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return std::nullopt;

    default:
        std::fprintf(stderr, "[MappingEngine] 警告: 未知的 EMappingActionMode，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return std::nullopt;
    }
}

TOptional<SAction> ZMappingEngine::BuildPressReleaseAction(const SInputEvent& Event, const SMappingRule& Rule) const
{
    // 判断激活状态：按钮直接使用事件类型，模拟量使用阈值判断
    bool bActivated = false;

    switch (Rule.Input.ControlType)
    {
    case EInputControlType::Button:
    case EInputControlType::Hat:
        bActivated = (Event.EventType == EInputEventType::Pressed);
        break;

    case EInputControlType::Trigger:
    case EInputControlType::Axis1D:
        bActivated = (Event.Value >= Rule.Input.Threshold);
        break;

    default:
        // PressRelease 模式不支持 Axis2D 等类型
        std::fprintf(stderr,
            "[MappingEngine] 调试: PressRelease 不支持 ControlType %d，跳过规则 \"%s\"\n",
            static_cast<int>(Rule.Input.ControlType), Rule.Id.c_str());
        return std::nullopt;
    }

    // 根据 action type 构建对应 payload
    switch (Rule.Output.Action.Type)
    {
    case EActionType::KeyboardKey:
    {
        auto* SourcePayload = std::get_if<SKeyboardAction>(&Rule.Output.Action.Payload);
        if (!SourcePayload)
        {
            std::fprintf(stderr,
                "[MappingEngine] 警告: 规则 \"%s\" 的 ActionType 为 KeyboardKey 但 Payload 类型不匹配\n",
                Rule.Id.c_str());
            return std::nullopt;
        }

        SAction Action;
        Action.Type = EActionType::KeyboardKey;
        Action.Payload = SKeyboardAction{.Key = SourcePayload->Key, .bPressed = bActivated};
        return Action;
    }

    case EActionType::MouseButton:
    {
        auto* SourcePayload = std::get_if<SMouseButtonAction>(&Rule.Output.Action.Payload);
        if (!SourcePayload)
        {
            std::fprintf(stderr,
                "[MappingEngine] 警告: 规则 \"%s\" 的 ActionType 为 MouseButton 但 Payload 类型不匹配\n",
                Rule.Id.c_str());
            return std::nullopt;
        }

        SAction Action;
        Action.Type = EActionType::MouseButton;
        Action.Payload = SMouseButtonAction{.Button = SourcePayload->Button, .bPressed = bActivated};
        return Action;
    }

    case EActionType::MouseWheel:
        // MouseWheel 本轮暂不实现
        std::fprintf(stderr,
            "[MappingEngine] 调试: MouseWheel 暂未实现，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return std::nullopt;

    default:
        std::fprintf(stderr,
            "[MappingEngine] 警告: PressRelease 不支持 ActionType %d，跳过规则 \"%s\"\n",
            static_cast<int>(Rule.Output.Action.Type), Rule.Id.c_str());
        return std::nullopt;
    }
}

TOptional<SAction> ZMappingEngine::BuildAnalogAction(const SInputEvent& Event, const SMappingRule& Rule) const
{
    // Analog 模式目前只支持 Axis2D -> MouseMove
    if (Rule.Input.ControlType != EInputControlType::Axis2D)
    {
        std::fprintf(stderr,
            "[MappingEngine] 调试: Analog 模式暂只支持 Axis2D，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return std::nullopt;
    }

    if (Rule.Output.Action.Type != EActionType::MouseMove)
    {
        std::fprintf(stderr,
            "[MappingEngine] 调试: Analog Axis2D 暂只支持 MouseMove 输出，跳过规则 \"%s\"\n",
            Rule.Id.c_str());
        return std::nullopt;
    }

    if (!std::get_if<SMouseMoveAction>(&Rule.Output.Action.Payload))
    {
        std::fprintf(stderr,
            "[MappingEngine] 警告: 规则 \"%s\" 的 ActionType 为 MouseMove 但 Payload 类型不匹配\n",
            Rule.Id.c_str());
        return std::nullopt;
    }

    // 径向死区判断
    float32 Magnitude = std::sqrt(
        Event.Axis2D.X * Event.Axis2D.X + Event.Axis2D.Y * Event.Axis2D.Y);

    if (Magnitude <= Rule.Input.Deadzone)
    {
        return std::nullopt;
    }

    // 按灵敏度缩放输出
    SAction Action;
    Action.Type = EActionType::MouseMove;
    Action.Payload = SMouseMoveAction{
        .DeltaX = Event.Axis2D.X * Rule.Output.Sensitivity,
        .DeltaY = Event.Axis2D.Y * Rule.Output.Sensitivity,
    };
    return Action;
}

}  // namespace MappyZ
