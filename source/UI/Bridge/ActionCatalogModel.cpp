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
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Backspace"),
        QStringLiteral("Keyboard: Backspace"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Delete"),
        QStringLiteral("Keyboard: Delete"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Insert"),
        QStringLiteral("Keyboard: Insert"), QStringLiteral("Keyboard")});

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

    // ── Keyboard: F1-F12 ──

    for (int FnIndex = 1; FnIndex <= 12; ++FnIndex)
    {
        QString Value = QStringLiteral("F%1").arg(FnIndex);
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

    // ── Keyboard: 导航键 ──

    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Home"),
        QStringLiteral("Keyboard: Home"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("End"),
        QStringLiteral("Keyboard: End"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("PageUp"),
        QStringLiteral("Keyboard: Page Up"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("PageDown"),
        QStringLiteral("Keyboard: Page Down"), QStringLiteral("Keyboard")});

    // ── Keyboard: 修饰键 ──

    Items.append({QStringLiteral("Keyboard"), QStringLiteral("LeftShift"),
        QStringLiteral("Keyboard: Left Shift"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("RightShift"),
        QStringLiteral("Keyboard: Right Shift"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("LeftCtrl"),
        QStringLiteral("Keyboard: Left Ctrl"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("RightCtrl"),
        QStringLiteral("Keyboard: Right Ctrl"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("LeftAlt"),
        QStringLiteral("Keyboard: Left Alt"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("RightAlt"),
        QStringLiteral("Keyboard: Right Alt"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("LeftMeta"),
        QStringLiteral("Keyboard: Left Win"), QStringLiteral("Keyboard")});

    // ── Keyboard: 符号键 (US layout) ──

    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Minus"),
        QStringLiteral("Keyboard: - (Minus)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Equal"),
        QStringLiteral("Keyboard: = (Equal)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("LeftBracket"),
        QStringLiteral("Keyboard: [ (Left Bracket)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("RightBracket"),
        QStringLiteral("Keyboard: ] (Right Bracket)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Backslash"),
        QStringLiteral("Keyboard: \\ (Backslash)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Semicolon"),
        QStringLiteral("Keyboard: ; (Semicolon)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Apostrophe"),
        QStringLiteral("Keyboard: ' (Apostrophe)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Comma"),
        QStringLiteral("Keyboard: , (Comma)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Period"),
        QStringLiteral("Keyboard: . (Period)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Slash"),
        QStringLiteral("Keyboard: / (Slash)"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("Backquote"),
        QStringLiteral("Keyboard: ` (Backquote)"), QStringLiteral("Keyboard")});

    // ── Keyboard: 小键盘 ──

    for (int NumIndex = 0; NumIndex <= 9; ++NumIndex)
    {
        QString Value = QStringLiteral("Num%1").arg(NumIndex);
        Items.append({QStringLiteral("Keyboard"), Value,
            QStringLiteral("Keyboard: Numpad %1").arg(NumIndex),
            QStringLiteral("Keyboard")});
    }
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("NumDivide"),
        QStringLiteral("Keyboard: Numpad /"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("NumMultiply"),
        QStringLiteral("Keyboard: Numpad *"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("NumSubtract"),
        QStringLiteral("Keyboard: Numpad -"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("NumAdd"),
        QStringLiteral("Keyboard: Numpad +"), QStringLiteral("Keyboard")});
    Items.append({QStringLiteral("Keyboard"), QStringLiteral("NumDecimal"),
        QStringLiteral("Keyboard: Numpad ."), QStringLiteral("Keyboard")});

    // ── Mouse: 鼠标按钮 ──

    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Left"),
        QStringLiteral("Mouse: Left Click"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Right"),
        QStringLiteral("Mouse: Right Click"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Middle"),
        QStringLiteral("Mouse: Middle Click"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Button4"),
        QStringLiteral("Mouse: Button 4 (Back)"), QStringLiteral("Mouse")});
    Items.append({QStringLiteral("MouseButton"), QStringLiteral("Button5"),
        QStringLiteral("Mouse: Button 5 (Forward)"), QStringLiteral("Mouse")});

    // ── Mouse: 鼠标移动 ──

    Items.append({QStringLiteral("MouseMove"), QStringLiteral("Cursor"),
        QStringLiteral("Mouse: Move Cursor"), QStringLiteral("Mouse")});
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

int ZActionCatalogModel::findIndex(const QString& Kind, const QString& Value) const
{
    for (int i = 0; i < Items.size(); ++i)
    {
        if (Items[i].Kind == Kind && Items[i].Value == Value)
        {
            return i;
        }
    }
    return -1;
}

}  // namespace MappyZ
