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
#include "UI/Bridge/InputStateModel.h"

namespace MappyZ
{

class ZAppController final : public QObject
{
    Q_OBJECT

    // ── QML 属性 ──

    Q_PROPERTY(QString runtimeState READ RuntimeState NOTIFY RuntimeStatusChanged)
    Q_PROPERTY(QString runtimeMessage READ RuntimeMessage NOTIFY RuntimeStatusChanged)
    Q_PROPERTY(QString outputState READ OutputState NOTIFY RuntimeStatusChanged)
    Q_PROPERTY(bool mappingEnabled READ IsMappingEnabled WRITE SetMappingEnabled NOTIFY MappingEnabledChanged)
    Q_PROPERTY(bool pumpTimerRunning READ IsPumpTimerRunning NOTIFY PumpTimerRunningChanged)
    Q_PROPERTY(int lastDrainedEventCount READ LastDrainedEventCount NOTIFY LastPumpSummaryChanged)
    Q_PROPERTY(int lastInputEventCount READ LastInputEventCount NOTIFY LastPumpSummaryChanged)
    Q_PROPERTY(int lastMappedInputCount READ LastMappedInputCount NOTIFY LastPumpSummaryChanged)
    Q_PROPERTY(int lastDispatchedInputCount READ LastDispatchedInputCount NOTIFY LastPumpSummaryChanged)
    Q_PROPERTY(QObject* deviceModel READ DeviceModel CONSTANT)
    Q_PROPERTY(QObject* inputStateModel READ InputStateModel CONSTANT)

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
    NODISCARD QObject* DeviceModel();
    NODISCARD QObject* InputStateModel();

    // ── QML invokable ──

    Q_INVOKABLE bool initializeRuntime(bool useNullOutput = false);
    Q_INVOKABLE bool startRuntime();
    Q_INVOKABLE void stopRuntime();
    Q_INVOKABLE void pumpOnce();
    Q_INVOKABLE void startPumpTimer(int intervalMs = 16);
    Q_INVOKABLE void stopPumpTimer();

signals:
    void RuntimeStatusChanged();
    void MappingEnabledChanged();
    void PumpTimerRunningChanged();
    void LastPumpSummaryChanged();
    void RuntimeError(QString message);

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
};

}  // namespace MappyZ
