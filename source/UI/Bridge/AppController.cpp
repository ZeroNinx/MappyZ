// ZAppController 实现。
// 把 ZApplicationBootstrap 的生命周期和状态暴露给 QML。
// 所有非预期路径都通过 EmitRuntimeError 统一输出日志、写 LogModel、发信号。

#include "UI/Bridge/AppController.h"

#include <cstdio>
#include <utility>

#include <QStandardPaths>

#include "Core/ControlId.h"
#include "Runtime/ProfileManager.h"

namespace MappyZ
{

// ── 构造与析构 ──

ZAppController::ZAppController(QObject* Parent)
    : QObject(Parent)
{
    connect(&PumpTimer, &QTimer::timeout, this, &ZAppController::pumpOnce);

    // capture 完成/取消时写日志
    connect(&InputCaptureInstance, &ZInputCaptureModel::captureCompleted,
        this, [this](const QString& /*DeviceId*/, const QString& ControlId) {
            AppendLog(QStringLiteral("Success"),
                QStringLiteral("Capture completed: %1").arg(ControlId));
        });

    connect(&InputCaptureInstance, &ZInputCaptureModel::captureCancelled,
        this, [this]() {
            AppendLog(QStringLiteral("Warning"),
                QStringLiteral("Capture cancelled"));
        });
}

ZAppController::ZAppController(
    TInputBackendFactory InputFactory,
    TOutputBackendFactory OutputFactory,
    QObject* Parent)
    : QObject(Parent)
    , Bootstrap(std::move(InputFactory), std::move(OutputFactory))
{
    connect(&PumpTimer, &QTimer::timeout, this, &ZAppController::pumpOnce);

    // capture 完成/取消时写日志
    connect(&InputCaptureInstance, &ZInputCaptureModel::captureCompleted,
        this, [this](const QString& /*DeviceId*/, const QString& ControlId) {
            AppendLog(QStringLiteral("Success"),
                QStringLiteral("Capture completed: %1").arg(ControlId));
        });

    connect(&InputCaptureInstance, &ZInputCaptureModel::captureCancelled,
        this, [this]() {
            AppendLog(QStringLiteral("Warning"),
                QStringLiteral("Capture cancelled"));
        });
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

    AppendLog(QStringLiteral("Info"),
        bEnabled ? QStringLiteral("Mapping enabled")
                 : QStringLiteral("Mapping disabled"));

    emit mappingEnabledChanged();
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

ZDeviceModel* ZAppController::DeviceModel()
{
    return &DeviceModelInstance;
}

ZInputStateModel* ZAppController::InputStateModel()
{
    return &InputStateModelInstance;
}

ZInputCaptureModel* ZAppController::InputCapture()
{
    return &InputCaptureInstance;
}

ZMappingRuleModel* ZAppController::MappingRuleModel()
{
    return &MappingRuleModelInstance;
}

ZLogModel* ZAppController::LogModel()
{
    return &LogModelInstance;
}

QString ZAppController::ActiveProfileName() const
{
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        return QStringLiteral("Default");
    }

    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    if (Profile.Name.empty())
    {
        return QStringLiteral("Default");
    }

    return QString::fromStdString(Profile.Name);
}

QString ZAppController::OutputDisplayText() const
{
    auto Status = Bootstrap.GetStatus();

    // 未 initialize 时显示 Unavailable
    if (Status.State == EApplicationBootstrapState::Created
        || Status.State == EApplicationBootstrapState::Error)
    {
        return QStringLiteral("Unavailable");
    }

    auto OutputStatus = Status.RuntimeStatus.OutputStatus.State;

    if (OutputStatus == EOutputBackendState::Unavailable)
    {
        return QStringLiteral("Unavailable");
    }

    if (OutputStatus == EOutputBackendState::Error)
    {
        return QStringLiteral("Output Error");
    }

    // Ready 状态下根据 NullOutput 标志区分
    if (Bootstrap.IsUsingNullOutput())
    {
        return QStringLiteral("NullOutput");
    }

    return QStringLiteral("RealOutput");
}

QString ZAppController::ProfilePath() const
{
    return CachedProfilePath;
}

QString ZAppController::ProfileMessage() const
{
    return CachedProfileMessage;
}

// ── invokable ──

bool ZAppController::initializeRuntime(bool useNullOutput)
{
    // 真实重建路径：Error 状态下 Bootstrap 会走完整 setup，先清理旧输入状态
    if (Bootstrap.GetStatus().State == EApplicationBootstrapState::Error)
    {
        InputStateModelInstance.clear();
    }

    auto Result = Bootstrap.Initialize({
        .bUseNullOutput = useNullOutput,
        .bEnableMapping = bCachedMappingEnabled,
    });

    if (!Result)
    {
        auto Message = QString::fromStdString(Result.Failure().Message);
        emit runtimeStatusChanged();
        EmitRuntimeError(QStringLiteral("Initialize failed: %1").arg(Message));
        return false;
    }

    // 应用缓存的 mapping enabled 到 host，保证 stopped host 与 UI 状态一致
    Bootstrap.GetRuntimeHost().SetMappingEnabled(bCachedMappingEnabled);

    // 注册设备热插拔 handler，pump 分发设备事件时自动更新 DeviceModel
    Bootstrap.GetRuntimeHost().GetEventPump().SetDeviceConnectedHandler(
        [this](const SDeviceInfo& Info)
        {
            DeviceModelInstance.AddOrUpdateDevice(Info);
        });

    Bootstrap.GetRuntimeHost().GetEventPump().SetDeviceDisconnectedHandler(
        [this](const SDeviceId& Id)
        {
            DeviceModelInstance.RemoveDevice(Id);
            InputStateModelInstance.RemoveDevice(Id);
        });

    // 注册输入事件 handler，pump 分发输入事件时自动更新 InputStateModel 和 InputCapture
    Bootstrap.GetRuntimeHost().GetEventPump().SetInputEventHandler(
        [this](const SInputEvent& Event)
        {
            InputStateModelInstance.ApplyInputEvent(Event);
            InputCaptureInstance.HandleInputEvent(Event);
        });

    // 初始化后立即刷新设备快照
    RefreshDeviceModelFromBootstrap();

    // 初始化后刷新映射规则模型
    RefreshMappingRuleModelFromHost();

    AppendLog(QStringLiteral("Info"), QStringLiteral("Runtime initialized"));
    emit runtimeStatusChanged();
    return true;
}

bool ZAppController::startRuntime()
{
    if (Bootstrap.GetStatus().State == EApplicationBootstrapState::Created
        || Bootstrap.GetStatus().State == EApplicationBootstrapState::Error)
    {
        EmitRuntimeError(
            QStringLiteral("Start failed: runtime not initialized"));
        return false;
    }

    auto Result = Bootstrap.StartRuntime();

    if (!Result)
    {
        auto Message = QString::fromStdString(Result.Failure().Message);
        emit runtimeStatusChanged();
        EmitRuntimeError(QStringLiteral("Start failed: %1").arg(Message));
        return false;
    }

    // 覆盖 Host.Start() 中 Session.SetEnabled() 的默认值，
    // 确保 initialize 与 start 之间用户切换 mapping enabled 不丢失
    Bootstrap.GetRuntimeHost().SetMappingEnabled(bCachedMappingEnabled);

    // start 后重新刷新设备快照，覆盖真实后端 Start() 后才完成枚举的场景
    RefreshDeviceModelFromBootstrap();

    AppendLog(QStringLiteral("Info"), QStringLiteral("Runtime started"));
    emit runtimeStatusChanged();
    return true;
}

void ZAppController::stopRuntime()
{
    stopPumpTimer();
    Bootstrap.StopRuntime();

    AppendLog(QStringLiteral("Info"), QStringLiteral("Runtime stopped"));
    emit runtimeStatusChanged();
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
        emit lastPumpSummaryChanged();
    }
}

void ZAppController::startPumpTimer(int intervalMs)
{
    PumpTimer.setInterval(intervalMs);
    PumpTimer.start();
    emit pumpTimerRunningChanged();
}

void ZAppController::stopPumpTimer()
{
    if (PumpTimer.isActive())
    {
        PumpTimer.stop();
        emit pumpTimerRunningChanged();
    }
}

bool ZAppController::saveActiveProfile(QString profilePath)
{
    // Runtime 未 initialize 时拒绝保存
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        CachedProfileMessage = QStringLiteral("Save failed: runtime not initialized");
        emit profileStatusChanged();
        EmitRuntimeError(CachedProfileMessage);
        return false;
    }

    // 参数为空时使用默认路径
    if (profilePath.isEmpty())
    {
        QString DataPath = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        if (DataPath.isEmpty())
        {
            CachedProfileMessage = QStringLiteral(
                "Save failed: cannot determine default profile path");
            emit profileStatusChanged();
            EmitRuntimeError(CachedProfileMessage);
            return false;
        }
        profilePath = DataPath + QStringLiteral("/profiles/default.json");
    }

    // 从 RuntimeHost 获取当前 profile 快照
    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();

    // 序列化并写入文件
    ZProfileManager Manager;
    auto Result = Manager.SaveProfile(
        Profile, StdPath(profilePath.toStdString()));

    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        CachedProfileMessage =
            QStringLiteral("Profile save failed: %1").arg(ErrorMessage);
        emit profileStatusChanged();
        EmitRuntimeError(CachedProfileMessage);
        return false;
    }

    // 保存成功，更新状态并通知 QML
    CachedProfilePath = profilePath;
    CachedProfileMessage = QStringLiteral("Profile saved");
    AppendLog(QStringLiteral("Success"), CachedProfileMessage);
    emit profileStatusChanged();
    emit profileSaved(profilePath);
    return true;
}

// ── 测试辅助 ──

void ZAppController::ReplaceActiveProfileForTest(SMappingProfile Profile)
{
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Profile));
    RefreshMappingRuleModelFromHost();
    emit runtimeStatusChanged();
}

// ── 内部工具 ──

void ZAppController::AppendLog(const QString& Level, const QString& Message)
{
    LogModelInstance.Append(Level, Message);
}

void ZAppController::EmitRuntimeError(const QString& Message)
{
    std::fprintf(stderr, "[AppController] 错误: %s\n",
        Message.toUtf8().constData());
    AppendLog(QStringLiteral("Error"), Message);
    emit runtimeError(Message);
}

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

void ZAppController::RefreshDeviceModelFromBootstrap()
{
    DeviceModelInstance.ReplaceDevices(Bootstrap.ListInputDevices());
}

void ZAppController::RefreshMappingRuleModelFromHost()
{
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        return;
    }

    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    MappingRuleModelInstance.ReplaceRules(std::move(Profile.Rules));
}

// ── applySelectedBinding ──

bool ZAppController::applySelectedBinding(QString controlId, QString actionText)
{
    // 空 controlId 校验
    if (controlId.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("Apply failed: controlId is empty"));
        return false;
    }

    // 空 actionText 校验
    if (actionText.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("Apply failed: actionText is empty"));
        return false;
    }

    // Runtime 必须已 initialize
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        EmitRuntimeError(QStringLiteral("Apply failed: runtime not initialized"));
        return false;
    }

    // 解析 actionText 为 SAction
    SAction Action = ParseActionText(actionText);
    if (Action.Type == EActionType::None)
    {
        EmitRuntimeError(
            QStringLiteral("Apply failed: cannot parse action \"%1\"").arg(actionText));
        return false;
    }

    // 推断输入控件类型
    StdString ControlIdStd = controlId.toStdString();
    SMappingInput Input;
    if (!InferInputFromControlId(ControlIdStd, Input))
    {
        EmitRuntimeError(
            QStringLiteral("Apply failed: unsupported control \"%1\"").arg(controlId));
        return false;
    }

    // 构造规则
    SMappingRule Rule;
    Rule.Id = ControlIdStd;
    Rule.DisplayName = ControlIdStd;
    Rule.bEnabled = true;
    Rule.Input = std::move(Input);
    Rule.Output.Action = std::move(Action);
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    Rule.Output.Sensitivity = 1.0f;

    // 获取当前 profile 快照，按 Input.ControlId 替换或追加
    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();

    bool bReplaced = false;
    for (auto& Existing : Profile.Rules)
    {
        if (Existing.Input.ControlId == ControlIdStd)
        {
            Existing = std::move(Rule);
            bReplaced = true;
            break;
        }
    }
    if (!bReplaced)
    {
        Profile.Rules.push_back(std::move(Rule));
    }

    // 写回 RuntimeHost
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Profile));

    // 刷新 UI model
    RefreshMappingRuleModelFromHost();

    AppendLog(QStringLiteral("Success"),
        QStringLiteral("Applied binding: %1 → %2").arg(controlId, actionText));
    return true;
}

// ── actionText 解析 ──

SAction ZAppController::ParseActionText(const QString& ActionText)
{
    if (ActionText == QStringLiteral("Keyboard: Space"))
    {
        SAction Action;
        Action.Type = EActionType::KeyboardKey;
        Action.Payload = SKeyboardAction{.Key = "Space", .bPressed = true};
        return Action;
    }

    if (ActionText == QStringLiteral("Mouse: Left Click"))
    {
        SAction Action;
        Action.Type = EActionType::MouseButton;
        Action.Payload = SMouseButtonAction{.Button = 0, .bPressed = true};
        return Action;
    }

    return SAction{};
}

// ── controlId 到输入类型推断 ──

bool ZAppController::InferInputFromControlId(
    const StdString& ControlId, SMappingInput& OutInput)
{
    OutInput.ControlId = ControlId;

    // Axis2D 暂不支持创建
    if (ControlId == MappyZ::ControlId::LeftStick
        || ControlId == MappyZ::ControlId::RightStick)
    {
        return false;
    }

    // Trigger
    if (ControlId == MappyZ::ControlId::LeftTrigger
        || ControlId == MappyZ::ControlId::RightTrigger)
    {
        OutInput.ControlType = EInputControlType::Trigger;
        OutInput.EventType = EInputEventType::Changed;
        OutInput.Threshold = 0.5f;
        OutInput.Deadzone = 0.0f;
        return true;
    }

    // D-Pad (Hat)
    if (ControlId == MappyZ::ControlId::DpadUp
        || ControlId == MappyZ::ControlId::DpadDown
        || ControlId == MappyZ::ControlId::DpadLeft
        || ControlId == MappyZ::ControlId::DpadRight)
    {
        OutInput.ControlType = EInputControlType::Hat;
        OutInput.EventType = EInputEventType::Pressed;
        OutInput.Threshold = 0.5f;
        OutInput.Deadzone = 0.0f;
        return true;
    }

    // Button 白名单：面板按钮、功能键、肩键、摇杆按下
    if (ControlId == MappyZ::ControlId::ButtonSouth
        || ControlId == MappyZ::ControlId::ButtonEast
        || ControlId == MappyZ::ControlId::ButtonWest
        || ControlId == MappyZ::ControlId::ButtonNorth
        || ControlId == MappyZ::ControlId::ButtonStart
        || ControlId == MappyZ::ControlId::ButtonBack
        || ControlId == MappyZ::ControlId::ButtonGuide
        || ControlId == MappyZ::ControlId::LeftShoulder
        || ControlId == MappyZ::ControlId::RightShoulder
        || ControlId == MappyZ::ControlId::LeftStickButton
        || ControlId == MappyZ::ControlId::RightStickButton)
    {
        OutInput.ControlType = EInputControlType::Button;
        OutInput.EventType = EInputEventType::Pressed;
        OutInput.Threshold = 0.5f;
        OutInput.Deadzone = 0.0f;
        return true;
    }

    return false;
}

}  // namespace MappyZ
