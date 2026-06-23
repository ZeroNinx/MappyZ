// 设备列表 QML 数据模型。
// 将运行时设备快照暴露给 QML ListView/Repeater，支持热插拔增删和批量替换。
// QML 通过 role 名称（deviceId, name, backend 等）访问每行设备属性。
//
// UI Bridge 层，依赖 Qt Core 和 Core 层 SDeviceInfo。
// 不依赖 SDL、Win32、QML 文件或输出后端。

#pragma once

#include <QAbstractListModel>
#include <QObject>

#include "Core/DeviceId.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZDeviceModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    // QML role 枚举，从 Qt::UserRole + 1 开始避免与内置 role 冲突
    enum EDeviceRole
    {
        DeviceIdRole = Qt::UserRole + 1,
        NameRole,
        BackendRole,
        VendorIdRole,
        ProductIdRole,
        GuidRole,
        InstanceIdRole,
        DisplayNameRole,
    };

    explicit ZDeviceModel(QObject* Parent = nullptr);
    ~ZDeviceModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── 批量与增量操作 ──

    // 批量替换所有设备，内部对重复 DeviceId 去重（保留最后一个）
    void ReplaceDevices(TVector<SDeviceInfo> NewDevices);

    // 新设备插入末尾并发出 rowsInserted；已存在设备更新并发出 dataChanged
    void AddOrUpdateDevice(const SDeviceInfo& DeviceInfo);

    // 删除指定设备并发出 rowsRemoved；找不到时 no-op
    void RemoveDevice(const SDeviceId& DeviceId);

    // ── QML invokable ──

    // 清空所有设备，重复调用安全
    Q_INVOKABLE void clear();

    // 返回指定行的 DeviceId 字符串，越界返回空串
    Q_INVOKABLE QString deviceIdAt(int Row) const;

    // ── C++ 辅助 ──

    // 返回当前设备列表的拷贝，调用方修改不影响 model
    NODISCARD TVector<SDeviceInfo> ListDevicesSnapshot() const;

private:
    // 在 Devices 中查找与指定 DeviceId 匹配的索引，找不到返回 -1
    NODISCARD int FindDeviceIndex(const SDeviceId& DeviceId) const;

    TVector<SDeviceInfo> Devices;
};

}  // namespace MappyZ
