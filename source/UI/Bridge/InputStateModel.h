// 输入状态 QML 数据模型。
// 将运行时 SInputEvent 转换为每个 (deviceId, controlId) 一行的最近状态，
// 供 QML Gamepad View 读取按钮、扳机、摇杆和方向键的实时值。
//
// UI Bridge 层，依赖 Qt Core 和 Core 层 SInputEvent / SDeviceId。
// 不依赖 SDL、Win32、QML 文件或输出后端。

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>

#include "Core/DeviceId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZInputStateModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    // QML role 枚举，从 Qt::UserRole + 1 开始避免与内置 role 冲突
    enum EInputStateRole
    {
        DeviceIdRole = Qt::UserRole + 1,
        ControlIdRole,
        ControlTypeRole,
        EventTypeRole,
        ValueRole,
        AxisXRole,
        AxisYRole,
        PressedRole,
        DisplayValueRole,
        SequenceRole,
    };

    explicit ZInputStateModel(QObject* Parent = nullptr);
    ~ZInputStateModel() override = default;

    // ── QAbstractListModel 必须实现 ──

    NODISCARD int rowCount(
        const QModelIndex& Parent = QModelIndex()) const override;

    NODISCARD QVariant data(
        const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // ── 状态更新 ──

    // 应用输入事件：新 (deviceId, controlId) 插入一行，已存在则更新
    void ApplyInputEvent(const SInputEvent& Event);

    // 删除指定设备的全部控件状态
    void RemoveDevice(const SDeviceId& DeviceId);

    // ── QML invokable ──

    // 清空所有控件状态，重复调用安全
    Q_INVOKABLE void clear();

    // 查询指定设备和控件是否处于按下状态
    Q_INVOKABLE bool isPressed(QString deviceId, QString controlId) const;

    // 查询指定设备和控件的单轴值
    Q_INVOKABLE double value(QString deviceId, QString controlId) const;

    // 查询指定设备和控件的双轴 X 值
    Q_INVOKABLE double axisX(QString deviceId, QString controlId) const;

    // 查询指定设备和控件的双轴 Y 值
    Q_INVOKABLE double axisY(QString deviceId, QString controlId) const;

    // 查询指定设备和控件的显示文本
    Q_INVOKABLE QString displayValue(QString deviceId, QString controlId) const;

    // 返回最近输入的 controlId；传入 deviceId 时仅在该设备内查找
    Q_INVOKABLE QString latestControlId(QString deviceId = QString()) const;

signals:
    // 指定设备的指定控件状态发生变化（插入新状态或更新已有状态）
    void controlStateChanged(QString deviceId, QString controlId);

    // 指定设备的全部输入状态被移除
    void deviceStateRemoved(QString deviceId);

    // 全部输入状态被清空
    void inputStateReset();

private:
    // Trigger PressedRole 的固定阈值
    static constexpr float TriggerPressedThreshold = 0.5f;

    // 单个控件的最近状态
    struct SControlState
    {
        StdString DeviceId;
        StdString ControlId;
        EInputControlType ControlType = EInputControlType::Button;
        EInputEventType EventType = EInputEventType::Released;
        float32 Value = 0.0f;
        SAxis2DValue Axis2D;
        uint64 Sequence = 0;
    };

    // 在 States 中查找与指定 (deviceId, controlId) 匹配的索引，找不到返回 -1
    NODISCARD int FindStateIndex(
        const StdString& DeviceId, const StdString& ControlId) const;

    // 根据控件状态计算 PressedRole
    NODISCARD static bool ComputePressed(const SControlState& State);

    // 根据控件状态计算 DisplayValueRole
    NODISCARD static QString ComputeDisplayValue(const SControlState& State);

    TVector<SControlState> States;

    // 全局单调递增序列号，每次 ApplyInputEvent 自增
    uint64 NextSequence = 1;
};

}  // namespace MappyZ
