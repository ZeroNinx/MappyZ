// ZActionCatalogModel 实现。
// 构造时填充静态 action 目录，运行期不动态变化。

#include "ActionCatalogModel.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(LogActionCatalog, "mappyz.ui.actioncatalog")

namespace MappyZ
{

ZActionCatalogModel::ZActionCatalogModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
    // ── Keyboard: 特殊键 ──

    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Space"),
        QStringLiteral("Keyboard: Space"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Enter"),
        QStringLiteral("Keyboard: Enter"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Escape"),
        QStringLiteral("Keyboard: Escape"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Tab"),
        QStringLiteral("Keyboard: Tab"), QStringLiteral("Keyboard")});

    // ── Keyboard: A-Z ──

    for (char Letter = 'A'; Letter <= 'Z'; ++Letter)
    {
        QString Value = QString(QChar::fromLatin1(Letter));
        Items.append({QStringLiteral("Keyboard"), Value,
            QStringLiteral("Keyboard: %1").arg(Value), QStringLiteral("Keyboard")});
    }

    // ── Keyboard: 0-9 ──

    for (char Digit = '0'; Digit <= '9'; ++Digit)
    {
        QString Value = QString(QChar::fromLatin1(Digit));
        Items.append({QStringLiteral("Keyboard"), Value,
            QStringLiteral("Keyboard: %1").arg(Value), QStringLiteral("Keyboard")});
    }

    // ── Keyboard: 方向键 ──

    Items.append({QStringLiteral("Keyboard"), QStringLiteral("ArrowUp"),
        QStringLiteral("Keyboard: Arrow Up"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("ArrowDown"),
        QStringLiteral("Keyboard: Arrow Down"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("ArrowLeft"),
        QStringLiteral("Keyboard: Arrow Left"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("ArrowRight"),
        QStringLiteral("Keyboard: Arrow Right"), QStringLiteral("Keyboard")});

    // ── Mouse: 鼠标按钮 ──

    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Left"),
        QStringLiteral("Mouse: Left Click"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Right"),
        QStringLiteral("Mouse: Right Click"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Middle"),
        QStringLiteral("Mouse: Middle Click"), QStringLiteral("Mouse")});
}

int ZActionCatalogModel::rowCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
    {
        return 0;
    }
    return Items.size();
}

QVariant ZActionCatalogModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid() || Index.row() < 0 || Index.row() >= Items.size())
    {
        return {};
    }

    const auto& Item = Items[Index.row()];
    switch (Role)
    {
    case KindRole:
        return Item.Kind;
    case ValueRole:
        return Item.Value;
    case DisplayTextRole:
        return Item.DisplayText;
    case CategoryRole:
        return Item.Category;
    default:
        return {};
    }
}

QHash<int, QByteArray> ZActionCatalogModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {ValueRole, "value"},
        {DisplayTextRole, "displayText"},
        {CategoryRole, "category"},
    };
}

QString ZActionCatalogModel::kindAt(int Row) const
{
    if (Row < 0 || Row >= Items.size())
    {
        qCWarning(LogActionCatalog) << "kindAt: 行索引越界" << Row;
        return {};
    }
    return Items[Row].Kind;
}

QString ZActionCatalogModel::valueAt(int Row) const
{
    if (Row < 0 || Row >= Items.size())
    {
        qCWarning(LogActionCatalog) << "valueAt: 行索引越界" << Row;
        return {};
    }
    return Items[Row].Value;
}

QString ZActionCatalogModel::displayTextAt(int Row) const
{
    if (Row < 0 || Row >= Items.size())
    {
        qCWarning(LogActionCatalog) << "displayTextAt: 行索引越界" << Row;
        return {};
    }
    return Items[Row].DisplayText;
}

bool ZActionCatalogModel::Contains(const QString& Kind, const QString& Value) const
{
    for (const auto& Item : Items)
    {
        if (Item.Kind == Kind && Item.Value == Value)
        {
            return true;
        }
    }
    return false;
}

}  // namespace MappyZ
