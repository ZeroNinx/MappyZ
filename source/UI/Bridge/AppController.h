// QML 应用控制器。
// 把 ZApplicationBootstrap 暴露给 QML，提供运行时状态查询、
// 启动/停止命令和定时 pump 控制。
//
// UI Bridge 层，依赖 Qt Core 和 App 层。
// 不包含 SDL、Win32 头，不直接访问后端对象。

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "App/ApplicationBootstrap.h"
#include "UI/Bridge/DeviceModel.h"
#include "UI/Bridge/InputCaptureModel.h"
#include "UI/Bridge/InputStateModel.h"

namespace MappyZ
{

class ZAppController final : public QObject
{
    Q_OBJECT

    // ── QML 属性 ──

    Q_PROPERTY(QString runtimeState READ RuntimeState NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString runtimeMessage READ RuntimeMessage NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString outputState READ OutputState NOTIFY runtimeStatusChanged)
    Q_PROPERTY(bool mappingEnabled READ IsMappingEnabled WRITE SetMappingEnabled NOTIFY mappingEnabledChanged)
    Q_PROPERTY(bool pumpTimerRunning READ IsPumpTimerRunning NOTIFY pumpTimerRunningChanged)
    Q_PROPERTY(int lastDrainedEventCount READ LastDrainedEventCount NOTIFY lastPumpSummaryChanged)
    Q_PROPERTY(int lastInputEventCount READ LastInputEventCount NOTIFY lastPumpSummaryChanged)
    Q_PROPERTY(int lastMappedInputCount READ LastMappedInputCount NOTIFY lastPumpSummaryChanged)
    Q_PROPERTY(int lastDispatchedInputCount READ LastDispatchedInputCount NOTIFY lastPumpSummaryChanged)
    Q_PROPERTY(ZDeviceModel* deviceModel READ DeviceModel CONSTANT)
    Q_PROPERTY(ZInputStateModel* inputStateModel READ InputStateModel CONSTANT)
    Q_PROPERTY(ZInputCaptureModel* inputCapture READ InputCapture CONSTANT)

public:
    // 生产构造：使用编译期开关的默认后端工厂
    explicit ZAppController(QObject* Parent = nullptr);

    // 测试构造：注入自定义后端工厂
    ZAppController(
        TInputBackendFactory InputFactory,
        TOutputBackendFactory OutputFactory,
        QObject* Parent = nullptr);

    ~ZAppController() override;

    // ── QML 属性读取 ──

    NODISCARD QString RuntimeState() const;
    NODISCARD QString RuntimeMessage() const;
    NODISCARD QString OutputState() const;
    NODISCARD bool IsMappingEnabled() const;
    void SetMappingEnabled(bool bEnabled);
    NODISCARD bool IsPumpTimerRunning() const;
    NODISCARD int LastDrainedEventCount() const;
    NODISCARD int LastInputEventCount() const;
    NODISCARD int LastMappedInputCount() const;
    NODISCARD int LastDispatchedInputCount() const;
    NODISCARD ZDeviceModel* DeviceModel();
    NODISCARD ZInputStateModel* InputStateModel();
    NODISCARD ZInputCaptureModel* InputCapture();

    // ── QML invokable ──

    Q_INVOKABLE bool initializeRuntime(bool useNullOutput = false);
    Q_INVOKABLE bool startRuntime();
    Q_INVOKABLE void stopRuntime();
    Q_INVOKABLE void pumpOnce();
    Q_INVOKABLE void startPumpTimer(int intervalMs = 16);
    Q_INVOKABLE void stopPumpTimer();

signals:
    void runtimeStatusChanged();
    void mappingEnabledChanged();
    void pumpTimerRunningChanged();
    void lastPumpSummaryChanged();
    void runtimeError(QString message);

private:
    // 将 EApplicationBootstrapState 转为 QML 稳定字符串
    static QString BootstrapStateToString(EApplicationBootstrapState State);

    // 将 EOutputBackendState 转为 QML 稳定字符串
    static QString OutputStateToString(EOutputBackendState State);

    // 从 Bootstrap 刷新设备快照到 DeviceModel
    void RefreshDeviceModelFromBootstrap();

    ZApplicationBootstrap Bootstrap;
    QTimer PumpTimer;

    // mapping enabled 缓存，在 initialize 前保存用户期望值
    bool bCachedMappingEnabled = true;

    // 上一次 PumpOnce 的 summary 缓存
    SRuntimeEventPumpSummary LastSummary;

    // 设备列表数据模型，供 QML 绑定
    ZDeviceModel DeviceModelInstance;

    // 输入状态数据模型，供 QML 绑定
    ZInputStateModel InputStateModelInstance;

    // 输入捕获状态模型，供 QML 绑定
    ZInputCaptureModel InputCaptureInstance;
};

}  // namespace MappyZ
