// ZInputCaptureModel 实现。
// 所有非预期路径都输出日志以方便定位问题。

#include "UI/Bridge/InputCaptureModel.h"

#include <cmath>
#include <cstdio>

namespace MappyZ
{

// ── 构造 ──

ZInputCaptureModel::ZInputCaptureModel(QObject* Parent)
    : QObject(Parent)
{
}

// ── QML invokable ──

void ZInputCaptureModel::begin(QString deviceId)
{
    bActive = true;
    TargetDeviceId = std::move(deviceId);
    CapturedDeviceId.clear();
    CapturedControlId.clear();

    emit captureResultChanged();
    emit captureStateChanged();
}

void ZInputCaptureModel::cancel()
{
    if (!bActive)
    {
        return;
    }

    bActive = false;

    emit captureStateChanged();
    emit captureCancelled();
}

// ── C++ 调用接口 ──

void ZInputCaptureModel::HandleInputEvent(const SInputEvent& Event)
{
    if (!bActive)
    {
        return;
    }

    // target device 非空且不匹配时继续等待
    QString EventDeviceId = QString::fromStdString(Event.DeviceId.Value);
    if (!TargetDeviceId.isEmpty() && EventDeviceId != TargetDeviceId)
    {
        return;
    }

    // 过滤无效输入（摇杆漂移、按钮释放等）
    if (!IsCaptureWorthyInput(Event))
    {
        return;
    }

    // 有效输入：完成捕获
    CapturedDeviceId = EventDeviceId;
    CapturedControlId = QString::fromStdString(Event.ControlId);
    bActive = false;

    emit captureResultChanged();
    emit captureStateChanged();
    emit captureCompleted(CapturedDeviceId, CapturedControlId);
}

// ── 属性读取 ──

bool ZInputCaptureModel::IsActive() const
{
    return bActive;
}

QString ZInputCaptureModel::DeviceId() const
{
    return CapturedDeviceId;
}

QString ZInputCaptureModel::ControlId() const
{
    return CapturedControlId;
}

QString ZInputCaptureModel::DisplayText() const
{
    if (bActive)
    {
        if (!TargetDeviceId.isEmpty())
        {
            return QStringLiteral("Waiting for input...");
        }
        return QStringLiteral("Waiting for any device input...");
    }

    return CapturedControlId;
}

// ── 内部工具 ──

bool ZInputCaptureModel::IsCaptureWorthyInput(const SInputEvent& Event)
{
    switch (Event.ControlType)
    {
    case EInputControlType::Button:
    case EInputControlType::Hat:
        return Event.EventType == EInputEventType::Pressed;

    case EInputControlType::Trigger:
        return Event.Value > TriggerCaptureThreshold;

    case EInputControlType::Axis1D:
        return std::abs(Event.Value) > AxisCaptureDeadzone;

    case EInputControlType::Axis2D:
    {
        float32 Magnitude = std::sqrt(
            Event.Axis2D.X * Event.Axis2D.X + Event.Axis2D.Y * Event.Axis2D.Y);
        return Magnitude > AxisCaptureDeadzone;
    }
    }

    std::fprintf(stderr,
        "[InputCaptureModel] 警告: 未知控件类型 %d，忽略\n",
        static_cast<int>(Event.ControlType));
    return false;
}

}  // namespace MappyZ
