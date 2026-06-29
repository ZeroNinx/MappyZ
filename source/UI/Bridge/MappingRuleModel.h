// 映射规则列表 QML 数据模型。
// 将运行时 active profile 中的 SMappingRule 暴露给 QML Repeater，
// 供 BindingEditor 的 "Current mappings" 列表展示。
//
// UI Bridge 层，依赖 Qt Core 和 Core 层 SMappingRule / SAction。
// 不依赖 SDL、Win32、QML 文件或输出后端。

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>

#include "Core/MappingRule.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZMappingRuleModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    // QML role 枚举，从 Qt::UserRole + 1 开始避免与内置 role 冲突
    enum EMappingRuleRole
    {
        RuleIdRole = Qt::UserRole + 1,
        InputRole,
        OutputRole,
        ActionKindRole,
        ActionValueRole,
        DisplayKindRole,
        EnabledRole,
    };

    explicit ZMappingRuleModel(QObject* Parent = nullptr);
    ~ZMappingRuleModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── 批量替换 ──

    // 用新的规则列表替换全部内容，触发 beginResetModel/endResetModel
    void ReplaceRules(TVector<SMappingRule> NewRules);

    // ── QML invokable ──

    // 清空所有规则，重复调用安全
    Q_INVOKABLE void clear();

    // 返回指定行的 ruleId 字符串，越界返回空串
    Q_INVOKABLE QString ruleIdAt(int Row) const;

    // 按 controlId 查询当前绑定的输出显示文本，未绑定返回空串
    Q_INVOKABLE QString displayOutputForInput(QString controlId) const;

    // 按 controlId 查询当前绑定的 UI 类别标签（Keyboard / Mouse），未绑定返回空串
    Q_INVOKABLE QString displayKindForInput(QString controlId) const;

    // 按 controlId 查询当前绑定的 ruleId，未绑定返回空串
    Q_INVOKABLE QString ruleIdForInput(QString controlId) const;

    // 按 controlId 查询当前绑定的 actionKind（与 ActionCatalog Kind 对齐），未绑定返回空串
    Q_INVOKABLE QString actionKindForInput(QString controlId) const;

    // 按 controlId 查询当前绑定的 actionValue（与 ActionCatalog Value 对齐），未绑定返回空串
    Q_INVOKABLE QString actionValueForInput(QString controlId) const;

    // ── C++ 辅助 ──

    // 返回当前规则列表的拷贝，调用方修改不影响 model
    NODISCARD TVector<SMappingRule> ListRulesSnapshot() const;

private:
    // 从 SAction payload 提取用户可读的输出值文本
    NODISCARD static QString ExtractOutputText(const SAction& Action);

    // 从 SAction type 提取目录 kind 文本（与 ActionCatalog Kind 对齐）
    NODISCARD static QString ExtractActionKindText(const SAction& Action);

    // 从 SAction payload 提取目录 value 文本（与 ActionCatalog Value 对齐）
    NODISCARD static QString ExtractActionValueText(const SAction& Action);

    // 从 SAction type 提取 UI 显示用类别标签（Keyboard / Mouse）
    NODISCARD static QString ExtractDisplayKindText(const SAction& Action);

    TVector<SMappingRule> Rules;
};

}  // namespace MappyZ
