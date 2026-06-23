// ZInputStateModel 实现。
// 管理每个 (deviceId, controlId) 的最近输入状态，
// 通过 QAbstractListModel 信号通知 QML 视图刷新。
// 所有非预期路径都输出日志。

#include "UI/Bridge/InputStateModel.h"

#include <cstdio>

namespace MappyZ
{

// ── 构造 ──

ZInputStateModel::ZInputStateModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

// ── QAbstractListModel 实现 ──

int ZInputStateModel::rowCount(const QModelIndex& Parent) const
{
    // 列表模型的 parent 始终无效；有效 parent 意味着树模型，返回 0
    if (Parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(States.size());
}

QVariant ZInputStateModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid() || Index.column() != 0)
    {
        return {};
    }

    int Row = Index.row();
    if (Row < 0 || Row >= static_cast<int>(States.size()))
    {
        return {};
    }

    const SControlState& State = States[static_cast<size_t>(Row)];

    switch (Role)
    {
    case DeviceIdRole:
        return QString::fromStdString(State.DeviceId);
    case ControlIdRole:
        return QString::fromStdString(State.ControlId);
    case ControlTypeRole:
        return static_cast<int>(State.ControlType);
    case EventTypeRole:
        return static_cast<int>(State.EventType);
    case ValueRole:
        return static_cast<double>(State.Value);
    case AxisXRole:
        return static_cast<double>(State.Axis2D.X);
    case AxisYRole:
        return static_cast<double>(State.Axis2D.Y);
    case PressedRole:
        return ComputePressed(State);
    case DisplayValueRole:
        return ComputeDisplayValue(State);
    case SequenceRole:
        return static_cast<qulonglong>(State.Sequence);
    default:
        return {};
    }
}

QHash<int, QByteArray> ZInputStateModel::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {ControlIdRole, "controlId"},
        {ControlTypeRole, "controlType"},
        {EventTypeRole, "eventType"},
        {ValueRole, "value"},
        {AxisXRole, "axisX"},
        {AxisYRole, "axisY"},
        {PressedRole, "pressed"},
        {DisplayValueRole, "displayValue"},
        {SequenceRole, "sequence"},
    };
}

// ── 状态更新 ──

void ZInputStateModel::ApplyInputEvent(const SInputEvent& Event)
{
    int ExistingIndex = FindStateIndex(Event.DeviceId.Value, Event.ControlId);

    if (ExistingIndex >= 0)
    {
        // 已存在：更新数据并通知视图
        SControlState& State = States[static_cast<size_t>(ExistingIndex)];
        State.ControlType = Event.ControlType;
        State.EventType = Event.EventType;
        State.Value = Event.Value;
        State.Axis2D = Event.Axis2D;
        State.Sequence = NextSequence++;

        QModelIndex ModelIndex = index(ExistingIndex);
        emit dataChanged(ModelIndex, ModelIndex);
        emit ControlStateChanged(
            QString::fromStdString(Event.DeviceId.Value),
            QString::fromStdString(Event.ControlId));
        return;
    }

    // 新控件：插入末尾
    SControlState NewState;
    NewState.DeviceId = Event.DeviceId.Value;
    NewState.ControlId = Event.ControlId;
    NewState.ControlType = Event.ControlType;
    NewState.EventType = Event.EventType;
    NewState.Value = Event.Value;
    NewState.Axis2D = Event.Axis2D;
    NewState.Sequence = NextSequence++;

    int InsertRow = static_cast<int>(States.size());
    beginInsertRows(QModelIndex(), InsertRow, InsertRow);
    States.push_back(std::move(NewState));
    endInsertRows();

    emit ControlStateChanged(
        QString::fromStdString(Event.DeviceId.Value),
        QString::fromStdString(Event.ControlId));
}

void ZInputStateModel::RemoveDevice(const SDeviceId& DeviceId)
{
    bool bRemoved = false;

    // 从后向前删除，避免索引偏移
    for (int Index = static_cast<int>(States.size()) - 1; Index >= 0; --Index)
    {
        if (States[static_cast<size_t>(Index)].DeviceId == DeviceId.Value)
        {
            beginRemoveRows(QModelIndex(), Index, Index);
            States.erase(States.begin() + Index);
            endRemoveRows();
            bRemoved = true;
        }
    }

    if (bRemoved)
    {
        emit DeviceStateRemoved(QString::fromStdString(DeviceId.Value));
    }
}

// ── QML invokable ──

void ZInputStateModel::clear()
{
    if (States.empty())
    {
        return;
    }

    beginResetModel();
    States.clear();
    endResetModel();

    emit InputStateReset();
}

bool ZInputStateModel::isPressed(QString deviceId, QString controlId) const
{
    int Index = FindStateIndex(deviceId.toStdString(), controlId.toStdString());
    if (Index < 0)
    {
        return false;
    }
    return ComputePressed(States[static_cast<size_t>(Index)]);
}

double ZInputStateModel::value(QString deviceId, QString controlId) const
{
    int Index = FindStateIndex(deviceId.toStdString(), controlId.toStdString());
    if (Index < 0)
    {
        return 0.0;
    }
    return static_cast<double>(States[static_cast<size_t>(Index)].Value);
}

double ZInputStateModel::axisX(QString deviceId, QString controlId) const
{
    int Index = FindStateIndex(deviceId.toStdString(), controlId.toStdString());
    if (Index < 0)
    {
        return 0.0;
    }
    return static_cast<double>(States[static_cast<size_t>(Index)].Axis2D.X);
}

double ZInputStateModel::axisY(QString deviceId, QString controlId) const
{
    int Index = FindStateIndex(deviceId.toStdString(), controlId.toStdString());
    if (Index < 0)
    {
        return 0.0;
    }
    return static_cast<double>(States[static_cast<size_t>(Index)].Axis2D.Y);
}

QString ZInputStateModel::displayValue(QString deviceId, QString controlId) const
{
    int Index = FindStateIndex(deviceId.toStdString(), controlId.toStdString());
    if (Index < 0)
    {
        return {};
    }
    return ComputeDisplayValue(States[static_cast<size_t>(Index)]);
}

QString ZInputStateModel::latestControlId(QString deviceId) const
{
    uint64 MaxSequence = 0;
    const SControlState* LatestState = nullptr;

    StdString DeviceFilter = deviceId.toStdString();

    for (const SControlState& State : States)
    {
        if (!DeviceFilter.empty() && State.DeviceId != DeviceFilter)
        {
            continue;
        }

        if (State.Sequence > MaxSequence)
        {
            MaxSequence = State.Sequence;
            LatestState = &State;
        }
    }

    if (LatestState == nullptr)
    {
        return {};
    }

    return QString::fromStdString(LatestState->ControlId);
}

// ── 内部工具 ──

int ZInputStateModel::FindStateIndex(
    const StdString& DeviceId, const StdString& ControlId) const
{
    for (size_t Index = 0; Index < States.size(); ++Index)
    {
        if (States[Index].DeviceId == DeviceId && States[Index].ControlId == ControlId)
        {
            return static_cast<int>(Index);
        }
    }
    return -1;
}

bool ZInputStateModel::ComputePressed(const SControlState& State)
{
    switch (State.ControlType)
    {
    case EInputControlType::Button:
    case EInputControlType::Hat:
        return State.EventType == EInputEventType::Pressed;

    case EInputControlType::Trigger:
        return State.Value > TriggerPressedThreshold;

    case EInputControlType::Axis1D:
    case EInputControlType::Axis2D:
        return false;
    }

    return false;
}

QString ZInputStateModel::ComputeDisplayValue(const SControlState& State)
{
    switch (State.ControlType)
    {
    case EInputControlType::Button:
    case EInputControlType::Hat:
        return (State.EventType == EInputEventType::Pressed)
            ? QStringLiteral("pressed")
            : QStringLiteral("released");

    case EInputControlType::Trigger:
    case EInputControlType::Axis1D:
        return QString::number(static_cast<double>(State.Value), 'f', 2);

    case EInputControlType::Axis2D:
        return QStringLiteral("(%1, %2)")
            .arg(static_cast<double>(State.Axis2D.X), 0, 'f', 2)
            .arg(static_cast<double>(State.Axis2D.Y), 0, 'f', 2);
    }

    return {};
}

}  // namespace MappyZ
