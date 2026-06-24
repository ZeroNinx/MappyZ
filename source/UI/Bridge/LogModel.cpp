// ZLogModel 实现。
// 将 SLogEntry vector 暴露为 QML 可消费的 list model，
// 超过 MaxCapacity 时自动移除最旧条目。

#include "UI/Bridge/LogModel.h"

#include <QTime>

namespace MappyZ
{

// ── 构造 ──

ZLogModel::ZLogModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

// ── QAbstractListModel 实现 ──

int ZLogModel::rowCount(const QModelIndex& Parent) const
{
    if (Parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(Entries.size());
}

QVariant ZLogModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid()
        || Index.row() < 0
        || Index.row() >= static_cast<int>(Entries.size()))
    {
        return {};
    }

    const auto& Entry = Entries[static_cast<size_t>(Index.row())];

    switch (Role)
    {
    case TimeRole:    return Entry.Time;
    case LevelRole:   return Entry.Level;
    case MessageRole: return Entry.Message;
    default:          return {};
    }
}

QHash<int, QByteArray> ZLogModel::roleNames() const
{
    return {
        {TimeRole,    "time"},
        {LevelRole,   "level"},
        {MessageRole, "message"},
    };
}

// ── C++ 内部接口 ──

void ZLogModel::Append(const QString& Level, const QString& Message)
{
    // 超过容量时移除最旧条目
    if (static_cast<int>(Entries.size()) >= MaxCapacity)
    {
        beginRemoveRows(QModelIndex(), 0, 0);
        Entries.erase(Entries.begin());
        endRemoveRows();
    }

    int NewRow = static_cast<int>(Entries.size());
    beginInsertRows(QModelIndex(), NewRow, NewRow);
    Entries.push_back(SLogEntry{
        .Time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
        .Level = Level,
        .Message = Message,
    });
    endInsertRows();
}

void ZLogModel::Clear()
{
    if (Entries.empty())
    {
        return;
    }

    beginResetModel();
    Entries.clear();
    endResetModel();
}

}  // namespace MappyZ
