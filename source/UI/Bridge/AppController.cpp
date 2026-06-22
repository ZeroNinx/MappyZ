// ZAppController 实现。
// 把 ZApplicationBootstrap 的生命周期和状态暴露给 QML。
// 所有非预期路径都输出日志并发出 RuntimeError 信号。

#include "UI/Bridge/AppController.h"

#include <cstdio>
#include <utility>

namespace ZeroMapper
{

// ── 构造与析构 ──

ZAppController::ZAppController(QObject* Parent)
    : QObject(Parent)
{
    connect(&PumpTimer, &QTimer::timeout, this, &ZAppController::pumpOnce);
}

ZAppController::ZAppController(
    TInputBackendFactory InputFactory,
    TOutputBackendFactory OutputFactory,
    QObject* Parent)
    : QObject(Parent)
    , Bootstrap(std::move(InputFactory), std::move(OutputFactory))
{
    connect(&PumpTimer, &QTimer::timeout, this, &ZAppController::pumpOnce);
}

ZAppController::~ZAppController()
{
    stopPumpTimer();
    Bootstrap.StopRuntime();
}

// ── 属性读取 ──

QString ZAppController::RuntimeState() const
{
    return BootstrapStateToString(Bootstrap.GetStatus().State);
}

QString ZAppController::RuntimeMessage() const
{
    return QString::fromStdString(Bootstrap.GetStatus().Message);
}

QString ZAppController::OutputState() const
{
    return OutputStateToString(Bootstrap.GetStatus().RuntimeStatus.OutputStatus.State);
}

bool ZAppController::IsMappingEnabled() const
{
    return bCachedMappingEnabled;
}

void ZAppController::SetMappingEnabled(bool bEnabled)
{
    if (bCachedMappingEnabled == bEnabled)
    {
        return;
    }

    bCachedMappingEnabled = bEnabled;

    // 已 initialize 时透传到 host
    auto Status = Bootstrap.GetStatus();
    if (Status.State == EApplicationBootstrapState::Ready
        || Status.State == EApplicationBootstrapState::Running)
    {
        Bootstrap.GetRuntimeHost().SetMappingEnabled(bEnabled);
    }

    emit MappingEnabledChanged();
}

bool ZAppController::IsPumpTimerRunning() const
{
    return PumpTimer.isActive();
}

int ZAppController::LastDrainedEventCount() const
{
    return static_cast<int>(LastSummary.DrainedEventCount);
}

int ZAppController::LastInputEventCount() const
{
    return static_cast<int>(LastSummary.InputEventCount);
}

int ZAppController::LastMappedInputCount() const
{
    return static_cast<int>(LastSummary.MappedInputCount);
}

int ZAppController::LastDispatchedInputCount() const
{
    return static_cast<int>(LastSummary.DispatchedInputCount);
}

// ── invokable ──

bool ZAppController::initializeRuntime(bool useNullOutput)
{
    auto Result = Bootstrap.Initialize({
        .bUseNullOutput = useNullOutput,
        .bEnableMapping = bCachedMappingEnabled,
    });

    if (!Result)
    {
        auto Message = QString::fromStdString(Result.Failure().Message);
        std::fprintf(stderr, "[AppController] 错误: 初始化失败: \"%s\"\n",
            Message.toUtf8().constData());
        emit RuntimeStatusChanged();
        emit RuntimeError(Message);
        return false;
    }

    // 应用缓存的 mapping enabled 到 host，保证 stopped host 与 UI 状态一致
    Bootstrap.GetRuntimeHost().SetMappingEnabled(bCachedMappingEnabled);

    emit RuntimeStatusChanged();
    return true;
}

bool ZAppController::startRuntime()
{
    if (Bootstrap.GetStatus().State == EApplicationBootstrapState::Created
        || Bootstrap.GetStatus().State == EApplicationBootstrapState::Error)
    {
        auto Message = QStringLiteral("必须先调用 initializeRuntime() 再调用 startRuntime()");
        std::fprintf(stderr, "[AppController] 错误: %s\n",
            Message.toUtf8().constData());
        emit RuntimeError(Message);
        return false;
    }

    auto Result = Bootstrap.StartRuntime();

    if (!Result)
    {
        auto Message = QString::fromStdString(Result.Failure().Message);
        std::fprintf(stderr, "[AppController] 错误: 启动运行时失败: \"%s\"\n",
            Message.toUtf8().constData());
        emit RuntimeStatusChanged();
        emit RuntimeError(Message);
        return false;
    }

    // 覆盖 Host.Start() 中 Session.SetEnabled() 的默认值，
    // 确保 initialize 与 start 之间用户切换 mapping enabled 不丢失
    Bootstrap.GetRuntimeHost().SetMappingEnabled(bCachedMappingEnabled);

    emit RuntimeStatusChanged();
    return true;
}

void ZAppController::stopRuntime()
{
    stopPumpTimer();
    Bootstrap.StopRuntime();
    emit RuntimeStatusChanged();
}

void ZAppController::pumpOnce()
{
    auto Summary = Bootstrap.PumpOnce();

    bool bChanged = (Summary.DrainedEventCount != LastSummary.DrainedEventCount
        || Summary.InputEventCount != LastSummary.InputEventCount
        || Summary.MappedInputCount != LastSummary.MappedInputCount
        || Summary.DispatchedInputCount != LastSummary.DispatchedInputCount);

    LastSummary = Summary;

    if (bChanged)
    {
        emit LastPumpSummaryChanged();
    }
}

void ZAppController::startPumpTimer(int intervalMs)
{
    PumpTimer.setInterval(intervalMs);
    PumpTimer.start();
    emit PumpTimerRunningChanged();
}

void ZAppController::stopPumpTimer()
{
    if (PumpTimer.isActive())
    {
        PumpTimer.stop();
        emit PumpTimerRunningChanged();
    }
}

// ── 内部工具 ──

QString ZAppController::BootstrapStateToString(EApplicationBootstrapState State)
{
    switch (State)
    {
    case EApplicationBootstrapState::Created: return QStringLiteral("created");
    case EApplicationBootstrapState::Ready:   return QStringLiteral("ready");
    case EApplicationBootstrapState::Running: return QStringLiteral("running");
    case EApplicationBootstrapState::Error:   return QStringLiteral("error");
    }

    return QStringLiteral("unknown");
}

QString ZAppController::OutputStateToString(EOutputBackendState State)
{
    switch (State)
    {
    case EOutputBackendState::Unavailable: return QStringLiteral("unavailable");
    case EOutputBackendState::Ready:       return QStringLiteral("ready");
    case EOutputBackendState::Error:       return QStringLiteral("error");
    }

    return QStringLiteral("unknown");
}

}  // namespace ZeroMapper
