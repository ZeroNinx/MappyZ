// 事件日志 QML 数据模型。
// 存储 UI / lifecycle 级日志条目，供 EventLogPanel 展示。
// 容量上限 200 条，超出后丢弃最旧记录。
//
// UI Bridge 层，依赖 Qt Core。
// Append/Clear 不标记 Q_INVOKABLE，QML 只能绑定读取，
// 日志写入入口集中在 ZAppController 内部。

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>

#include "Core/ProjectCore.h"

namespace MappyZ
{

// 单条日志结构
struct SLogEntry
{
    QString Time;
    QString Level;
    QString Message;
};

class ZLogModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    // QML role 枚举，从 Qt::UserRole + 1 开始避免与内置 role 冲突
    enum ELogRole
    {
        TimeRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole,
    };

    // 最大条目数，超过后丢弃最旧记录
    static constexpr int MaxCapacity = 200;

    explicit ZLogModel(QObject* Parent = nullptr);
    ~ZLogModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── C++ 内部接口（不暴露给 QML） ──

    // 追加一条日志，Level 为 Info / Success / Warning / Error
    void Append(const QString& Level, const QString& Message);

    // 清空所有日志，重复调用安全
    void Clear();

private:
    TVector<SLogEntry> Entries;
};

}  // namespace MappyZ
