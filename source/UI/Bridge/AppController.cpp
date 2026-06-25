// ZAppController 实现。
// 把 ZApplicationBootstrap 的生命周期和状态暴露给 QML。
// 所有非预期路径都通过 EmitRuntimeError 统一输出日志、写 LogModel、发信号。

#include "UI/Bridge/AppController.h"

#include <cstdio>
#include <filesystem>
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

ZActionCatalogModel* ZAppController::ActionCatalogModel()
{
    return &ActionCatalogModelInstance;
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

    // Running 时输出实时生效
    if (Status.State == EApplicationBootstrapState::Running)
    {
        return QStringLiteral("Live Output");
    }

    return QStringLiteral("Ready");
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

bool ZAppController::initializeRuntime()
{
    // Error 状态下再次 Initialize 会走完整 setup，先清理旧输入状态
    if (Bootstrap.GetStatus().State == EApplicationBootstrapState::Error)
    {
        InputStateModelInstance.clear();
    }

    auto Result = Bootstrap.Initialize({
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

    RegisterEventHandlers();

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
        profilePath = DefaultProfilePath();
        if (profilePath.isEmpty())
        {
            CachedProfileMessage = QStringLiteral(
                "Save failed: cannot determine default profile path");
            emit profileStatusChanged();
            EmitRuntimeError(CachedProfileMessage);
            return false;
        }
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

bool ZAppController::loadProfile(QString profilePath)
{
    // Runtime 未 initialize 时拒绝加载
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        CachedProfileMessage = QStringLiteral("Load failed: runtime not initialized");
        emit profileStatusChanged();
        EmitRuntimeError(CachedProfileMessage);
        return false;
    }

    // 记录是否使用默认路径，用于区分软/硬错误
    bool bUsingDefaultPath = profilePath.isEmpty();

    if (bUsingDefaultPath)
    {
        profilePath = DefaultProfilePath();
        if (profilePath.isEmpty())
        {
            CachedProfileMessage = QStringLiteral(
                "Load failed: cannot determine default profile path");
            emit profileStatusChanged();
            EmitRuntimeError(CachedProfileMessage);
            return false;
        }

        // 默认 profile 不存在是正常情况（首次启动），静默返回
        if (!std::filesystem::exists(StdPath(profilePath.toStdString())))
        {
            AppendLog(QStringLiteral("Info"),
                QStringLiteral("No saved profile found, using default"));
            return true;
        }
    }

    // 读取 profile 文件
    ZProfileManager Manager;
    auto Result = Manager.LoadProfile(StdPath(profilePath.toStdString()));

    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        CachedProfileMessage =
            QStringLiteral("Profile load failed: %1").arg(ErrorMessage);
        emit profileStatusChanged();
        EmitRuntimeError(CachedProfileMessage);
        return false;
    }

    // 加载成功，替换 RuntimeHost profile 并刷新 UI
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Result).TakeValue());
    RefreshMappingRuleModelFromHost();

    CachedProfilePath = profilePath;
    CachedProfileMessage = QStringLiteral("Profile loaded");
    AppendLog(QStringLiteral("Success"), CachedProfileMessage);
    emit profileStatusChanged();
    emit runtimeStatusChanged();
    emit profileLoaded(profilePath);
    return true;
}

bool ZAppController::removeBinding(QString ruleId)
{
    if (ruleId.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("Remove failed: ruleId is empty"));
        return false;
    }

    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        EmitRuntimeError(QStringLiteral("Remove failed: runtime not initialized"));
        return false;
    }

    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    StdString RuleIdStd = ruleId.toStdString();

    auto Iterator = std::find_if(Profile.Rules.begin(), Profile.Rules.end(),
        [&RuleIdStd](const SMappingRule& Rule) { return Rule.Id == RuleIdStd; });

    if (Iterator == Profile.Rules.end())
    {
        EmitRuntimeError(
            QStringLiteral("Remove failed: rule \"%1\" not found").arg(ruleId));
        return false;
    }

    Profile.Rules.erase(Iterator);
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Profile));
    RefreshMappingRuleModelFromHost();

    AppendLog(QStringLiteral("Success"),
        QStringLiteral("Removed binding: %1").arg(ruleId));
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

void ZAppController::RegisterEventHandlers()
{
    // 设备热插拔 handler
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

    // 输入事件 handler
    Bootstrap.GetRuntimeHost().GetEventPump().SetInputEventHandler(
        [this](const SInputEvent& Event)
        {
            InputStateModelInstance.ApplyInputEvent(Event);
            InputCaptureInstance.HandleInputEvent(Event);
        });
}

QString ZAppController::DefaultProfilePath() const
{
    QString DataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (DataPath.isEmpty())
    {
        return QString();
    }
    return DataPath + QStringLiteral("/profiles/default.json");
}

// ── applySelectedBinding ──

bool ZAppController::applySelectedBinding(
    QString controlId, QString actionKind, QString actionValue)
{
    // 空 controlId 校验
    if (controlId.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("Apply failed: controlId is empty"));
        return false;
    }

    // 空 actionKind / actionValue 校验
    if (actionKind.isEmpty() || actionValue.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("Apply failed: actionKind or actionValue is empty"));
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

    // 根据 actionKind + actionValue 构造 SAction
    SAction Action;

    if (actionKind == QStringLiteral("Keyboard"))
    {
        if (!ActionCatalogModelInstance.Contains(actionKind, actionValue))
        {
            EmitRuntimeError(
                QStringLiteral("Apply failed: unknown keyboard key \"%1\"")
                    .arg(actionValue));
            return false;
        }

        Action.Type = EActionType::KeyboardKey;
        Action.Payload = SKeyboardAction{
            .Key = actionValue.toStdString(), .bPressed = true};
    }
    else if (actionKind == QStringLiteral("MouseButton"))
    {
        int ButtonIndex = -1;
        if (actionValue == QStringLiteral("Left"))
        {
            ButtonIndex = 0;
        }
        else if (actionValue == QStringLiteral("Right"))
        {
            ButtonIndex = 1;
        }
        else if (actionValue == QStringLiteral("Middle"))
        {
            ButtonIndex = 2;
        }
        else
        {
            EmitRuntimeError(
                QStringLiteral("Apply failed: unknown mouse button \"%1\"")
                    .arg(actionValue));
            return false;
        }

        Action.Type = EActionType::MouseButton;
        Action.Payload = SMouseButtonAction{.Button = ButtonIndex, .bPressed = true};
    }
    else
    {
        EmitRuntimeError(
            QStringLiteral("Apply failed: unknown actionKind \"%1\"").arg(actionKind));
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
        QStringLiteral("Applied binding: %1 → %2:%3")
            .arg(controlId, actionKind, actionValue));
    return true;
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

    // D-Pad（SDL3 Gamepad API 将 DPad 作为普通 Button 上报）
    if (ControlId == MappyZ::ControlId::DpadUp
        || ControlId == MappyZ::ControlId::DpadDown
        || ControlId == MappyZ::ControlId::DpadLeft
        || ControlId == MappyZ::ControlId::DpadRight)
    {
        OutInput.ControlType = EInputControlType::Button;
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
