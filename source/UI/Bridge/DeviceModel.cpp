// ZDeviceModel 实现。
// 管理设备快照的增删改查，通过 QAbstractListModel 信号通知 QML 视图刷新。
// 所有非预期路径都输出日志。

#include "UI/Bridge/DeviceModel.h"

#include <algorithm>
#include <cstdio>

namespace MappyZ
{

// ── 构造 ──

ZDeviceModel::ZDeviceModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

// ── QAbstractListModel 实现 ──

int ZDeviceModel::rowCount(const QModelIndex& Parent) const
{
    // 列表模型的 parent 始终无效；有效 parent 意味着树模型，返回 0
    if (Parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(Devices.size());
}

QVariant ZDeviceModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid() || Index.column() != 0)
    {
        return {};
    }

    int Row = Index.row();
    if (Row < 0 || Row >= static_cast<int>(Devices.size()))
    {
        return {};
    }

    const SDeviceInfo& Info = Devices[static_cast<size_t>(Row)];

    switch (Role)
    {
    case DeviceIdRole:
        return QString::fromStdString(Info.Id.Value);
    case NameRole:
        return QString::fromStdString(Info.Name);
    case BackendRole:
        return QString::fromStdString(Info.Backend);
    case VendorIdRole:
        return QString::fromStdString(Info.VendorId);
    case ProductIdRole:
        return QString::fromStdString(Info.ProductId);
    case GuidRole:
        return QString::fromStdString(Info.Guid);
    case InstanceIdRole:
        return QString::fromStdString(Info.InstanceId);
    case DisplayNameRole:
        // 优先使用名称，为空时回退到设备 ID
        if (!Info.Name.empty())
        {
            return QString::fromStdString(Info.Name);
        }
        return QString::fromStdString(Info.Id.Value);
    default:
        return {};
    }
}

QHash<int, QByteArray> ZDeviceModel::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {NameRole, "name"},
        {BackendRole, "backend"},
        {VendorIdRole, "vendorId"},
        {ProductIdRole, "productId"},
        {GuidRole, "guid"},
        {InstanceIdRole, "instanceId"},
        {DisplayNameRole, "displayName"},
    };
}

// ── 批量与增量操作 ──

void ZDeviceModel::ReplaceDevices(TVector<SDeviceInfo> NewDevices)
{
    // 对重复 DeviceId 去重，保留最后一个（后出现的覆盖先出现的）
    TVector<SDeviceInfo> Deduplicated;
    Deduplicated.reserve(NewDevices.size());

    for (auto ReverseIterator = NewDevices.rbegin();
         ReverseIterator != NewDevices.rend();
         ++ReverseIterator)
    {
        bool bAlreadySeen = std::any_of(
            Deduplicated.begin(),
            Deduplicated.end(),
            [&ReverseIterator](const SDeviceInfo& Existing)
            {
                return Existing.Id == ReverseIterator->Id;
            });

        if (!bAlreadySeen)
        {
            Deduplicated.push_back(std::move(*ReverseIterator));
        }
    }

    // 反转回原始顺序（因为从 rbegin 遍历时倒序插入）
    std::reverse(Deduplicated.begin(), Deduplicated.end());

    beginResetModel();
    Devices = std::move(Deduplicated);
    endResetModel();

    emit deviceModelReset();
}

void ZDeviceModel::AddOrUpdateDevice(const SDeviceInfo& DeviceInfo)
{
    int ExistingIndex = FindDeviceIndex(DeviceInfo.Id);

    if (ExistingIndex >= 0)
    {
        // 已存在：更新数据并通知视图
        Devices[static_cast<size_t>(ExistingIndex)] = DeviceInfo;

        QModelIndex ModelIndex = index(ExistingIndex);
        emit dataChanged(ModelIndex, ModelIndex);
        emit deviceUpdated(QString::fromStdString(DeviceInfo.Id.Value));
        return;
    }

    // 新设备：插入末尾
    int InsertRow = static_cast<int>(Devices.size());
    beginInsertRows(QModelIndex(), InsertRow, InsertRow);
    Devices.push_back(DeviceInfo);
    endInsertRows();

    emit deviceAdded(QString::fromStdString(DeviceInfo.Id.Value));
}

void ZDeviceModel::RemoveDevice(const SDeviceId& DeviceId)
{
    int ExistingIndex = FindDeviceIndex(DeviceId);

    if (ExistingIndex < 0)
    {
        // 找不到目标设备，no-op
        return;
    }

    beginRemoveRows(QModelIndex(), ExistingIndex, ExistingIndex);
    Devices.erase(Devices.begin() + ExistingIndex);
    endRemoveRows();

    emit deviceRemoved(QString::fromStdString(DeviceId.Value));
}

// ── QML invokable ──

void ZDeviceModel::clear()
{
    if (Devices.empty())
    {
        return;
    }

    beginResetModel();
    Devices.clear();
    endResetModel();

    emit deviceModelReset();
}

QString ZDeviceModel::deviceIdAt(int Row) const
{
    if (Row < 0 || Row >= static_cast<int>(Devices.size()))
    {
        return {};
    }
    return QString::fromStdString(Devices[static_cast<size_t>(Row)].Id.Value);
}

QString ZDeviceModel::displayNameAt(int Row) const
{
    if (Row < 0 || Row >= static_cast<int>(Devices.size()))
    {
        return {};
    }
    const SDeviceInfo& Info = Devices[static_cast<size_t>(Row)];
    if (!Info.Name.empty())
    {
        return QString::fromStdString(Info.Name);
    }
    return QString::fromStdString(Info.Id.Value);
}

// ── C++ 辅助 ──

TVector<SDeviceInfo> ZDeviceModel::ListDevicesSnapshot() const
{
    return Devices;
}

// ── 内部工具 ──

int ZDeviceModel::FindDeviceIndex(const SDeviceId& DeviceId) const
{
    for (size_t Index = 0; Index < Devices.size(); ++Index)
    {
        if (Devices[Index].Id == DeviceId)
        {
            return static_cast<int>(Index);
        }
    }
    return -1;
}

}  // namespace MappyZ
