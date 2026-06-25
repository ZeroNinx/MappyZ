// ZMappingRuleModel 实现。
// 将 SMappingRule vector 暴露为 QML 可消费的 list model，
// 从 SAction payload 提取 output 和 actionKind 文本。

#include "UI/Bridge/MappingRuleModel.h"

#include <cstdio>

namespace MappyZ
{

// ── 构造 ──

ZMappingRuleModel::ZMappingRuleModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

// ── QAbstractListModel 实现 ──

int ZMappingRuleModel::rowCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(Rules.size());
}

QVariant ZMappingRuleModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid()
        || Index.row() < 0
        || Index.row() >= static_cast<int>(Rules.size()))
    {
        return {};
    }

    const auto& Rule = Rules[static_cast<size_t>(Index.row())];

    switch (Role)
    {
    case RuleIdRole:
        return QString::fromStdString(Rule.Id);
    case InputRole:
        return QString::fromStdString(Rule.Input.ControlId);
    case OutputRole:
        return ExtractOutputText(Rule.Output.Action);
    case ActionKindRole:
        return ExtractActionKindText(Rule.Output.Action);
    case ActionValueRole:
        return ExtractActionValueText(Rule.Output.Action);
    case DisplayKindRole:
        return ExtractDisplayKindText(Rule.Output.Action);
    case EnabledRole:
        return Rule.bEnabled;
    default:
        return {};
    }
}

QHash<int, QByteArray> ZMappingRuleModel::roleNames() const
{
    return {
        {RuleIdRole,      "ruleId"},
        {InputRole,       "input"},
        {OutputRole,      "output"},
        {ActionKindRole,  "actionKind"},
        {ActionValueRole, "actionValue"},
        {DisplayKindRole, "displayKind"},
        {EnabledRole,     "enabled"},
    };
}

// ── 批量替换 ──

void ZMappingRuleModel::ReplaceRules(TVector<SMappingRule> NewRules)
{
    beginResetModel();
    Rules = std::move(NewRules);
    endResetModel();
}

// ── QML invokable ──

void ZMappingRuleModel::clear()
{
    if (Rules.empty())
    {
        return;
    }

    beginResetModel();
    Rules.clear();
    endResetModel();
}

QString ZMappingRuleModel::ruleIdAt(int Row) const
{
    if (Row < 0 || Row >= static_cast<int>(Rules.size()))
    {
        return {};
    }
    return QString::fromStdString(Rules[static_cast<size_t>(Row)].Id);
}

// ── C++ 辅助 ──

TVector<SMappingRule> ZMappingRuleModel::ListRulesSnapshot() const
{
    return Rules;
}

QString ZMappingRuleModel::ExtractOutputText(const SAction& Action)
{
    switch (Action.Type)
    {
    case EActionType::KeyboardKey:
    {
        const auto* Keyboard = std::get_if<SKeyboardAction>(&Action.Payload);
        if (Keyboard)
        {
            return QString::fromStdString(Keyboard->Key);
        }
        std::fprintf(stderr,
            "[MappingRuleModel] 错误: KeyboardKey action 的 payload 类型不匹配\n");
        return QStringLiteral("Unknown");
    }
    case EActionType::MouseButton:
    {
        const auto* Mouse = std::get_if<SMouseButtonAction>(&Action.Payload);
        if (Mouse)
        {
            switch (Mouse->Button)
            {
            case 0: return QStringLiteral("Left Click");
            case 1: return QStringLiteral("Right Click");
            case 2: return QStringLiteral("Middle Click");
            default:
                return QStringLiteral("Button %1").arg(Mouse->Button);
            }
        }
        std::fprintf(stderr,
            "[MappingRuleModel] 错误: MouseButton action 的 payload 类型不匹配\n");
        return QStringLiteral("Unknown");
    }
    case EActionType::MouseMove:
        return QStringLiteral("Mouse Move");
    case EActionType::MouseWheel:
        return QStringLiteral("Mouse Wheel");
    case EActionType::None:
        return QStringLiteral("None");
    }

    std::fprintf(stderr,
        "[MappingRuleModel] 错误: 未知 action type %d\n",
        static_cast<int>(Action.Type));
    return QStringLiteral("Unknown");
}

QString ZMappingRuleModel::ExtractActionKindText(const SAction& Action)
{
    // 返回与 ActionCatalog Kind 对齐的文本
    switch (Action.Type)
    {
    case EActionType::KeyboardKey:
        return QStringLiteral("Keyboard");
    case EActionType::MouseButton:
        return QStringLiteral("MouseButton");
    case EActionType::MouseMove:
        return QStringLiteral("MouseMove");
    case EActionType::MouseWheel:
        return QStringLiteral("MouseWheel");
    case EActionType::None:
        return QStringLiteral("None");
    }

    std::fprintf(stderr,
        "[MappingRuleModel] 错误: 未知 action type %d\n",
        static_cast<int>(Action.Type));
    return QStringLiteral("Unknown");
}

QString ZMappingRuleModel::ExtractActionValueText(const SAction& Action)
{
    // 返回与 ActionCatalog Value 对齐的文本
    switch (Action.Type)
    {
    case EActionType::KeyboardKey:
    {
        const auto* Keyboard = std::get_if<SKeyboardAction>(&Action.Payload);
        if (Keyboard)
        {
            return QString::fromStdString(Keyboard->Key);
        }
        std::fprintf(stderr,
            "[MappingRuleModel] 错误: KeyboardKey action 的 payload 类型不匹配\n");
        return {};
    }
    case EActionType::MouseButton:
    {
        const auto* Mouse = std::get_if<SMouseButtonAction>(&Action.Payload);
        if (Mouse)
        {
            switch (Mouse->Button)
            {
            case 0: return QStringLiteral("Left");
            case 1: return QStringLiteral("Right");
            case 2: return QStringLiteral("Middle");
            default:
                return QStringLiteral("Button%1").arg(Mouse->Button);
            }
        }
        std::fprintf(stderr,
            "[MappingRuleModel] 错误: MouseButton action 的 payload 类型不匹配\n");
        return {};
    }
    case EActionType::MouseMove:
        return QStringLiteral("Move");
    case EActionType::MouseWheel:
        return QStringLiteral("Wheel");
    case EActionType::None:
        return {};
    }

    std::fprintf(stderr,
        "[MappingRuleModel] 错误: 未知 action type %d\n",
        static_cast<int>(Action.Type));
    return {};
}

QString ZMappingRuleModel::ExtractDisplayKindText(const SAction& Action)
{
    // 返回面向用户的 UI 类别标签
    switch (Action.Type)
    {
    case EActionType::KeyboardKey:
        return QStringLiteral("Keyboard");
    case EActionType::MouseButton:
    case EActionType::MouseMove:
    case EActionType::MouseWheel:
        return QStringLiteral("Mouse");
    case EActionType::None:
        return QStringLiteral("None");
    }

    std::fprintf(stderr,
        "[MappingRuleModel] 错误: 未知 action type %d\n",
        static_cast<int>(Action.Type));
    return QStringLiteral("Unknown");
}

}  // namespace MappyZ
