// ZAutoProfileRuleModel 实现。
// 把 SAutoProfileRule vector 暴露为 QML list model，并结合 profile 查找表派生
// profileName / valid / statusText 显示字段。匹配与保存始终使用 profileId。

#include "UI/Bridge/AutoProfileRuleModel.h"

namespace MappyZ
{

ZAutoProfileRuleModel::ZAutoProfileRuleModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

int ZAutoProfileRuleModel::rowCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(Rules.size());
}

bool ZAutoProfileRuleModel::ProfileExists(const StdString& ProfileId) const
{
    return ProfileNames.contains(QString::fromStdString(ProfileId));
}

QVariant ZAutoProfileRuleModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid()
        || Index.row() < 0
        || Index.row() >= static_cast<int>(Rules.size()))
    {
        return {};
    }

    const auto& Rule = Rules[static_cast<size_t>(Index.row())];
    const bool bValid = ProfileExists(Rule.ProfileId);

    switch (Role)
    {
    case RuleIdRole:
        return QString::fromStdString(Rule.Id);
    case EnabledRole:
        return Rule.bEnabled;
    case ProcessNameRole:
        return QString::fromStdString(Rule.ProcessName);
    case ProfileIdRole:
        return QString::fromStdString(Rule.ProfileId);
    case ProfileNameRole:
        if (bValid)
        {
            return ProfileNames.value(QString::fromStdString(Rule.ProfileId));
        }
        return QStringLiteral("Missing profile");
    case ValidRole:
        return bValid;
    case StatusTextRole:
        if (!bValid)
        {
            return QStringLiteral("References a profile that no longer exists");
        }
        return QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> ZAutoProfileRuleModel::roleNames() const
{
    return {
        {RuleIdRole,      "ruleId"},
        {EnabledRole,     "enabled"},
        {ProcessNameRole, "processName"},
        {ProfileIdRole,   "profileId"},
        {ProfileNameRole, "profileName"},
        {ValidRole,       "valid"},
        {StatusTextRole,  "statusText"},
    };
}

void ZAutoProfileRuleModel::ReplaceRules(TVector<SAutoProfileRule> NewRules)
{
    beginResetModel();
    Rules = std::move(NewRules);
    endResetModel();
}

void ZAutoProfileRuleModel::SetProfileLookup(QHash<QString, QString> ProfileIdToName)
{
    ProfileNames = std::move(ProfileIdToName);

    // 仅显示派生字段变化，规则行本身不动：对全部行发一次 dataChanged。
    if (!Rules.empty())
    {
        const QModelIndex TopLeft = index(0, 0);
        const QModelIndex BottomRight = index(static_cast<int>(Rules.size()) - 1, 0);
        emit dataChanged(TopLeft, BottomRight,
            {ProfileNameRole, ValidRole, StatusTextRole});
    }
}

QString ZAutoProfileRuleModel::ruleIdAt(int Row) const
{
    if (Row < 0 || Row >= static_cast<int>(Rules.size()))
    {
        return {};
    }
    return QString::fromStdString(Rules[static_cast<size_t>(Row)].Id);
}

int ZAutoProfileRuleModel::rowForRuleId(QString ruleId) const
{
    const StdString Target = ruleId.toStdString();
    for (size_t Row = 0; Row < Rules.size(); ++Row)
    {
        if (Rules[Row].Id == Target)
        {
            return static_cast<int>(Row);
        }
    }
    return -1;
}

TVector<SAutoProfileRule> ZAutoProfileRuleModel::ListRulesSnapshot() const
{
    return Rules;
}

}  // namespace MappyZ
