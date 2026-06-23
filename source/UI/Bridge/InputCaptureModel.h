// 输入捕获状态模型。
// 管理 Binding Capture workflow：用户点击 Capture Input 后进入等待态，
// 下一次有效输入事件自动填充 captured control 并退出等待。
//
// 有效输入判定规则：
//   Button / Hat  — 仅 Pressed 事件
//   Trigger       — Value > 0.5
//   Axis1D        — abs(Value) > 0.7
//   Axis2D        — 向量幅度 > 0.7
//
// UI Bridge 层，依赖 Qt Core 和 Core 层输入事件契约。
// 不包含 SDL、Win32 头，不直接访问后端对象。

#pragma once

#include <QObject>
#include <QString>

#include "Core/InputEvent.h"

namespace MappyZ
{

class ZInputCaptureModel final : public QObject
{
    Q_OBJECT

    // ── QML 属性 ──

    Q_PROPERTY(bool active READ IsActive NOTIFY captureStateChanged)
    Q_PROPERTY(QString deviceId READ DeviceId NOTIFY captureResultChanged)
    Q_PROPERTY(QString controlId READ ControlId NOTIFY captureResultChanged)
    Q_PROPERTY(QString displayText READ DisplayText NOTIFY captureStateChanged)

public:
    explicit ZInputCaptureModel(QObject* Parent = nullptr);
    ~ZInputCaptureModel() override = default;

    // ── QML invokable ──

    // 开始捕获。deviceId 为空表示接受任意设备输入。
    Q_INVOKABLE void begin(QString deviceId = QString());

    // 取消捕获。inactive 时 no-op。
    Q_INVOKABLE void cancel();

    // ── C++ 调用接口 ──

    // 由 AppController 的 input event handler 调用。
    // inactive 时 no-op；仅当输入满足有效 capture 过滤规则时完成捕获。
    void HandleInputEvent(const SInputEvent& Event);

    // ── 属性读取 ──

    NODISCARD bool IsActive() const;
    NODISCARD QString DeviceId() const;
    NODISCARD QString ControlId() const;
    NODISCARD QString DisplayText() const;

signals:
    // capture 状态变化（active 或 displayText 变化时发出）
    void captureStateChanged();

    // capture 结果变化（deviceId / controlId 属性变化时发出）
    void captureResultChanged();

    // capture 完成，payload 为捕获到的 deviceId 和 controlId
    void captureCompleted(QString deviceId, QString controlId);

    // capture 被用户取消
    void captureCancelled();

private:
    // 判断输入事件是否满足有效 capture 过滤规则
    static bool IsCaptureWorthyInput(const SInputEvent& Event);

    // Trigger 有效 capture 阈值
    static constexpr float32 TriggerCaptureThreshold = 0.5f;

    // Axis1D / Axis2D 有效 capture 死区阈值
    static constexpr float32 AxisCaptureDeadzone = 0.7f;

    bool bActive = false;
    QString TargetDeviceId;
    QString CapturedDeviceId;
    QString CapturedControlId;
};

}  // namespace MappyZ
