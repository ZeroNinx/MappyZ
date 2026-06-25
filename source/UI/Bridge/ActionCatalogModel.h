// 输出动作目录数据模型。
// 提供所有可选输出动作的只读列表，作为 BindingEditor action 选择的单一事实来源。
// UI Bridge 层，依赖 Qt Core。不依赖运行时状态或后端。

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QVector>

#include "Core/ProjectCore.h"

namespace MappyZ
{

// 目录条目内部表示
struct SActionCatalogItem
{
    QString Kind;        // "Keyboard" / "MouseButton"
    QString Value;       // "Space" / "Enter" / "A" / "Left"
    QString DisplayText; // "Keyboard: Space" / "Mouse: Left Click"
    QString Category;    // "Keyboard" / "Mouse"
};

class ZActionCatalogModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ERole
    {
        KindRole = Qt::UserRole + 1,
        ValueRole,
        DisplayTextRole,
        CategoryRole,
    };

    explicit ZActionCatalogModel(QObject* Parent = nullptr);
    ~ZActionCatalogModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── QML invokable ──

    Q_INVOKABLE QString kindAt(int row) const;
    Q_INVOKABLE QString valueAt(int row) const;
    Q_INVOKABLE QString displayTextAt(int row) const;

    // 校验 kind+value 组合是否存在于目录中
    NODISCARD bool Contains(const QString& Kind, const QString& Value) const;

private:
    // 构造时填充的静态目录
    QVector<SActionCatalogItem> Items;
};

}  // namespace MappyZ
