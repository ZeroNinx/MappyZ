// 自动切换规则列表 QML 数据模型。
// 向 QML 暴露权威规则快照及每行的显示派生信息（profileName / valid / statusText），
// 本身不切换 profile、不持久化，也不直接修改规则；所有 mutation 经 ZAppController。
//
// UI Bridge 层，依赖 Qt Core 和 App 层 SAutoProfileRule。
// 不依赖 SDL、Win32、QML 文件或输出后端。

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QString>

#include "App/AutoProfileRule.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZAutoProfileRuleModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    // QML role 枚举，从 Qt::UserRole + 1 开始避免与内置 role 冲突。
    enum EAutoProfileRuleRole
    {
        RuleIdRole = Qt::UserRole + 1,
        EnabledRole,
        ProcessNameRole,
        ProfileIdRole,
        ProfileNameRole,
        ValidRole,
        StatusTextRole,
    };

    explicit ZAutoProfileRuleModel(QObject* Parent = nullptr);
    ~ZAutoProfileRuleModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── 批量替换 ──

    // 用新的规则列表替换全部内容，触发 beginResetModel/endResetModel。
    void ReplaceRules(TVector<SAutoProfileRule> NewRules);

    // 更新 profile ID -> 显示名 查找表并刷新所有行的派生显示（profileName/valid/statusText）。
    // 只影响显示，不改变规则本身；profile rename/create/delete 后调用。
    void SetProfileLookup(QHash<QString, QString> ProfileIdToName);

    // ── QML invokable ──

    // 返回指定行的 ruleId，越界返回空串。QML selection 以此为权威身份，不缓存 row。
    Q_INVOKABLE QString ruleIdAt(int Row) const;

    // 返回 ruleId 对应行号，未找到返回 -1。
    Q_INVOKABLE int rowForRuleId(QString ruleId) const;

    // ── C++ 辅助 ──

    // 返回当前规则列表拷贝，调用方修改不影响 model。
    NODISCARD TVector<SAutoProfileRule> ListRulesSnapshot() const;

private:
    // 判断某个 profileId 在当前查找表中是否存在。
    NODISCARD bool ProfileExists(const StdString& ProfileId) const;

    TVector<SAutoProfileRule> Rules;
    QHash<QString, QString> ProfileNames;  // profileId -> 显示名
};

}  // namespace MappyZ
