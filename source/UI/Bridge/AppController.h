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
#include "UI/Bridge/LogModel.h"
#include "UI/Bridge/MappingRuleModel.h"

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
    Q_PROPERTY(ZMappingRuleModel* mappingRuleModel READ MappingRuleModel CONSTANT)
    Q_PROPERTY(ZLogModel* logModel READ LogModel CONSTANT)
    Q_PROPERTY(QString activeProfileName READ ActiveProfileName NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString outputDisplayText READ OutputDisplayText NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString profilePath READ ProfilePath NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileMessage READ ProfileMessage NOTIFY profileStatusChanged)
    Q_PROPERTY(bool realOutputEnabled READ IsRealOutputEnabled NOTIFY outputModeChanged)
    Q_PROPERTY(bool outputModeSwitching READ IsOutputModeSwitching NOTIFY outputModeChanged)

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
    NODISCARD ZMappingRuleModel* MappingRuleModel();
    NODISCARD ZLogModel* LogModel();
    NODISCARD QString ActiveProfileName() const;
    NODISCARD QString OutputDisplayText() const;
    NODISCARD QString ProfilePath() const;
    NODISCARD QString ProfileMessage() const;
    NODISCARD bool IsRealOutputEnabled() const;
    NODISCARD bool IsOutputModeSwitching() const;

    // ── QML invokable ──

    Q_INVOKABLE bool initializeRuntime(bool useNullOutput = false);
    Q_INVOKABLE bool startRuntime();
    Q_INVOKABLE void stopRuntime();
    Q_INVOKABLE void pumpOnce();
    Q_INVOKABLE void startPumpTimer(int intervalMs = 16);
    Q_INVOKABLE void stopPumpTimer();
    Q_INVOKABLE bool applySelectedBinding(QString controlId, QString actionText);
    Q_INVOKABLE bool saveActiveProfile(QString profilePath = QString());
    Q_INVOKABLE bool loadProfile(QString profilePath = QString());
    Q_INVOKABLE bool setRealOutputEnabled(bool enabled);

    // 测试辅助：替换 RuntimeHost 的 active profile 并刷新 UI model。
    // 不暴露给 QML，仅供 C++ 测试代码使用。
    void ReplaceActiveProfileForTest(SMappingProfile Profile);

signals:
    void runtimeStatusChanged();
    void mappingEnabledChanged();
    void pumpTimerRunningChanged();
    void lastPumpSummaryChanged();
    void runtimeError(QString message);
    void profileStatusChanged();
    void profileSaved(QString profilePath);
    void profileLoaded(QString profilePath);
    void outputModeChanged();

private:
    // 将 EApplicationBootstrapState 转为 QML 稳定字符串
    static QString BootstrapStateToString(EApplicationBootstrapState State);

    // 将 EOutputBackendState 转为 QML 稳定字符串
    static QString OutputStateToString(EOutputBackendState State);

    // 从 Bootstrap 刷新设备快照到 DeviceModel
    void RefreshDeviceModelFromBootstrap();

    // 从 RuntimeHost profile 快照刷新 MappingRuleModel
    void RefreshMappingRuleModelFromHost();

    // 统一日志写入入口，所有 lifecycle 日志走此方法
    void AppendLog(const QString& Level, const QString& Message);

    // 统一错误处理：写 Error 日志 + stderr + emit runtimeError
    void EmitRuntimeError(const QString& Message);

    // 解析 actionText 构造 SAction，失败返回 None 类型
    static SAction ParseActionText(const QString& ActionText);

    // 根据 controlId 推断输入控件类型和事件类型，不支持的控件返回 false
    static bool InferInputFromControlId(
        const StdString& ControlId, SMappingInput& OutInput);

    // save/load 共用的默认 profile 路径
    NODISCARD QString DefaultProfilePath() const;

    // 注册 RuntimeHost event handler（设备连接/断开、输入事件）
    void RegisterEventHandlers();

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

    // 映射规则列表模型，供 QML 绑定
    ZMappingRuleModel MappingRuleModelInstance;

    // 事件日志模型，供 QML EventLogPanel 绑定
    ZLogModel LogModelInstance;

    // 上次成功保存的 profile 文件路径
    QString CachedProfilePath;

    // 最近一次保存操作的结果消息
    QString CachedProfileMessage;

    // 输出模式切换防重入标志
    bool bOutputModeSwitching = false;
};

}  // namespace MappyZ
