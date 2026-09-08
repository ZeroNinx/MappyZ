// ZAppController 实现。
// 把 ZApplicationBootstrap 的生命周期和状态暴露给 QML。
// 所有非预期路径都通过 EmitRuntimeError 统一输出日志、写 LogModel、发信号。

#include "UI/Bridge/AppController.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include <QHash>
#include <QStandardPaths>
#include <QVariantMap>

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
    : ZAppController(std::move(InputFactory), std::move(OutputFactory), StdPath(), Parent)
{
}

ZAppController::ZAppController(
    TInputBackendFactory InputFactory,
    TOutputBackendFactory OutputFactory,
    StdPath ProfileDirectory,
    QObject* Parent)
    : QObject(Parent)
    , Bootstrap(std::move(InputFactory), std::move(OutputFactory))
    , ProfileDirectoryOverride(std::move(ProfileDirectory))
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

ZAutoProfileRuleModel* ZAppController::AutoProfileRuleModel()
{
    return &AutoProfileRuleModelInstance;
}

QString ZAppController::ForegroundProcessName() const
{
    return QString::fromStdString(ForegroundProcessNameCache);
}

QString ZAppController::AutomaticProfileMessage() const
{
    return CachedAutomaticProfileMessage;
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
    // 名称由 manager 的当前描述提供；未初始化或名称为空时回退到 Default
    auto ActiveInfo = ProfileManager.GetActiveProfileInfo();
    if (!ActiveInfo || ActiveInfo->Name.empty())
    {
        return QStringLiteral("Default");
    }

    return QString::fromStdString(ActiveInfo->Name);
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
    // 活动路径由 manager 的当前描述提供；未初始化时为空
    auto ActiveInfo = ProfileManager.GetActiveProfileInfo();
    if (!ActiveInfo)
    {
        return QString();
    }

    return QString::fromStdString(ActiveInfo->FilePath.string());
}

QString ZAppController::ProfileMessage() const
{
    return CachedProfileMessage;
}

bool ZAppController::IsProfileDirty() const
{
    return bProfileDirty;
}

QString ZAppController::ProfileSaveState() const
{
    return CachedProfileSaveState;
}

QString ZAppController::ProfileDisplayText() const
{
    QString Name = ActiveProfileName();
    if (CachedProfileSaveState == QStringLiteral("dirty"))
        return Name + QStringLiteral(" (unsaved)");
    if (CachedProfileSaveState == QStringLiteral("error"))
        return Name + QStringLiteral(" (save error)");
    return Name;
}

QString ZAppController::ProfileSaveDisplayText() const
{
    if (CachedProfileSaveState == QStringLiteral("error"))
        return QStringLiteral("Save Error");
    if (CachedProfileSaveState == QStringLiteral("dirty"))
        return QStringLiteral("Unsaved");
    return QStringLiteral("Saved");
}

QString ZAppController::ProfileSaveSeverity() const
{
    if (CachedProfileSaveState == QStringLiteral("error"))
        return QStringLiteral("danger");
    if (CachedProfileSaveState == QStringLiteral("dirty"))
        return QStringLiteral("caution");
    return QStringLiteral("normal");
}

QString ZAppController::RuntimeDisplayText() const
{
    switch (Bootstrap.GetStatus().State)
    {
    case EApplicationBootstrapState::Created: return QStringLiteral("Created");
    case EApplicationBootstrapState::Ready:   return QStringLiteral("Ready");
    case EApplicationBootstrapState::Running: return QStringLiteral("Running");
    case EApplicationBootstrapState::Error:   return QStringLiteral("Error");
    default: return QStringLiteral("Unknown");
    }
}

QVariantList ZAppController::ProfileEntries() const
{
    // 把 manager 已排序的稳定列表转成 QML 可消费的 { id, name } 列表
    QVariantList Entries;
    for (const SProfileInfo& Info : ProfileManager.GetProfiles())
    {
        QVariantMap Entry;
        Entry.insert(QStringLiteral("id"), QString::fromStdString(Info.Id));
        Entry.insert(QStringLiteral("name"), QString::fromStdString(Info.Name));
        Entries.append(Entry);
    }
    return Entries;
}

QString ZAppController::ActiveProfileId() const
{
    auto ActiveInfo = ProfileManager.GetActiveProfileInfo();
    if (!ActiveInfo)
    {
        return QString();
    }
    return QString::fromStdString(ActiveInfo->Id);
}

bool ZAppController::CanDeleteProfile() const
{
    return ProfileManager.CanDeleteActiveProfile();
}

bool ZAppController::CanRenameProfile() const
{
    return ProfileManager.CanRenameActiveProfile();
}

// ── invokable ──

bool ZAppController::initializeRuntime()
{
    // Error 状态下再次 Initialize 会走完整 setup，先清理旧输入状态
    if (Bootstrap.GetStatus().State == EApplicationBootstrapState::Error)
    {
        InputStateModelInstance.clear();
    }

    auto Result = Bootstrap.Initialize();

    if (!Result)
    {
        auto Message = QString::fromStdString(Result.Failure().Message);
        emit runtimeStatusChanged();
        EmitRuntimeError(QStringLiteral("Initialize failed: %1").arg(Message));
        return false;
    }

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

// ── 多配置命令 ──

bool ZAppController::initializeProfiles()
{
    // 只允许在 Runtime 已初始化后加载配置
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        SetProfileOperationError(
            QStringLiteral("Initialize profiles failed: runtime not initialized"));
        return false;
    }

    auto Result = ProfileManager.Initialize(ResolveProfilesDirectory());
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        SetProfileOperationError(
            QStringLiteral("Initialize profiles failed: %1").arg(ErrorMessage));
        return false;
    }

    ApplyLoadedProfile(std::move(Result).TakeValue(),
        QStringLiteral("Profile loaded"), true);
    emit profileLoaded(ProfilePath());
    AppendLog(QStringLiteral("Info"),
        QStringLiteral("Profiles initialized, active: %1").arg(ActiveProfileName()));

    // profile 就绪后加载自动切换规则并推给模型，再对当前前台求值一次。
    LoadAutomaticProfileRules();
    PushRulesToModel();
    EvaluateAndApplyAutomaticProfile();
    return true;
}

bool ZAppController::saveActiveProfile()
{
    return SaveActiveProfileInternal(false);
}

bool ZAppController::switchProfile(QString profileId)
{
    if (profileId.isEmpty())
    {
        SetProfileOperationError(QStringLiteral("Switch failed: profileId is empty"));
        return false;
    }

    // 等于当前项时无需切换
    if (profileId == ActiveProfileId())
    {
        return true;
    }

    return SwitchProfileInternal(profileId.toStdString(), /*bAutomatic=*/false);
}

bool ZAppController::SwitchProfileInternal(const StdString& ProfileId, bool bAutomatic)
{
    // 目标等于当前项时视为成功空操作（自动路径可能在求值后到达此处）
    if (QString::fromStdString(ProfileId) == ActiveProfileId())
    {
        return true;
    }

    // 切换前先保存当前脏配置，只写切换前活动项的文件
    if (!SaveDirtyProfileBeforeSelectionChange())
    {
        return false;
    }

    auto Result = ProfileManager.ActivateProfile(ProfileId);
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        if (bAutomatic)
        {
            // 自动切换失败不污染 profile 保存状态，只更新自动切换状态消息，
            // 并按计划以 Error 级别写入日志（失败是用户应看到的错误）。
            SetAutomaticProfileMessage(
                QStringLiteral("Auto-switch failed: %1").arg(ErrorMessage));
            AppendLog(QStringLiteral("Error"),
                QStringLiteral("Auto-switch failed: %1").arg(ErrorMessage));
        }
        else
        {
            SetProfileOperationError(
                QStringLiteral("Switch failed: %1").arg(ErrorMessage));
        }
        return false;
    }

    ApplyLoadedProfile(std::move(Result).TakeValue(),
        QStringLiteral("Profile loaded"), false);
    emit profileLoaded(ProfilePath());

    if (bAutomatic)
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Auto-switched to %1").arg(ActiveProfileName()));
        // 自动切换成功属于后台信息，按计划以 Info 级别记录（区别于用户手动
        // 切换的 Success 确认）。只记录 profile 显示名，不含进程路径。
        AppendLog(QStringLiteral("Info"),
            QStringLiteral("Auto-switched profile: %1").arg(ActiveProfileName()));
    }
    else
    {
        AppendLog(QStringLiteral("Success"),
            QStringLiteral("Switched profile: %1").arg(ActiveProfileName()));
    }
    return true;
}

bool ZAppController::createProfile()
{
    // 新建也是选择项变化，先保存当前脏配置
    if (!SaveDirtyProfileBeforeSelectionChange())
    {
        return false;
    }

    auto Result = ProfileManager.CreateProfile();
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        SetProfileOperationError(QStringLiteral("Create failed: %1").arg(ErrorMessage));
        return false;
    }

    ApplyLoadedProfile(std::move(Result).TakeValue(),
        QStringLiteral("Profile created"), true);

    // 新建同时是一次保存和一次加载
    QString NewPath = ProfilePath();
    emit profileSaved(NewPath);
    emit profileLoaded(NewPath);
    AppendLog(QStringLiteral("Success"),
        QStringLiteral("Created profile: %1").arg(ActiveProfileName()));

    // profile 集合变化：刷新规则查找表让 invalid 状态与新 profile 同步。
    RefreshRuleLookup();
    return true;
}

bool ZAppController::renameActiveProfile(QString newName)
{
    // 在 Qt 边界去除首尾空白，空名立即报错
    QString Trimmed = newName.trimmed();
    if (Trimmed.isEmpty())
    {
        SetProfileOperationError(QStringLiteral("Rename failed: name is empty"));
        return false;
    }

    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        SetProfileOperationError(QStringLiteral("Rename failed: runtime not initialized"));
        return false;
    }

    // Default 显示名固定不可重命名；UI 已禁用按钮，此处再拦一层以防绕过。
    if (!ProfileManager.CanRenameActiveProfile())
    {
        SetProfileOperationError(
            QStringLiteral("Rename failed: cannot rename the default profile"));
        return false;
    }

    // 使用当前 Runtime snapshot，manager 会强制保留 ID 并写入新名称
    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    StdString NewNameStd = Trimmed.toStdString();
    auto Result = ProfileManager.RenameActiveProfile(Profile, NewNameStd);
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        SetProfileOperationError(QStringLiteral("Rename failed: %1").arg(ErrorMessage));
        return false;
    }

    ApplyLoadedProfile(std::move(Result).TakeValue(),
        QStringLiteral("Profile renamed"), true);
    // 重命名把新名称落盘，因此是一次保存，但不触发加载语义
    emit profileSaved(ProfilePath());
    AppendLog(QStringLiteral("Success"),
        QStringLiteral("Renamed profile: %1").arg(ActiveProfileName()));

    // 显示名变化：刷新规则查找表以更新引用该 profile 的行显示名。
    RefreshRuleLookup();
    return true;
}

bool ZAppController::deleteActiveProfile()
{
    if (!ProfileManager.CanDeleteActiveProfile())
    {
        SetProfileOperationError(
            QStringLiteral("Delete failed: cannot delete the last profile"));
        return false;
    }

    // 删除前先保存当前脏配置
    if (!SaveDirtyProfileBeforeSelectionChange())
    {
        return false;
    }

    // 记录被删除项名称与 ID，用于日志和规则清理
    QString DeletedName = ActiveProfileName();
    QString DeletedProfileId = ActiveProfileId();

    auto Result = ProfileManager.DeleteActiveProfile();
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        SetProfileOperationError(QStringLiteral("Delete failed: %1").arg(ErrorMessage));
        return false;
    }

    ApplyLoadedProfile(std::move(Result).TakeValue(),
        QStringLiteral("Profile deleted"), true);
    emit profileLoaded(ProfilePath());
    AppendLog(QStringLiteral("Success"),
        QStringLiteral("Deleted profile: %1, active: %2")
            .arg(DeletedName, ActiveProfileName()));

    // 删除的 profile 若被规则引用，清理这些规则（回退语义已由匹配策略提供）。
    // Default 永不可删，因此这里删除的必是非 Default profile。
    const StdString DeletedId = DeletedProfileId.toStdString();
    if (!DeletedId.empty())
    {
        TVector<SAutoProfileRule> Kept;
        Kept.reserve(AutomaticRules.size());
        bool bRemovedAny = false;
        for (const auto& Rule : AutomaticRules)
        {
            if (Rule.ProfileId == DeletedId)
            {
                bRemovedAny = true;
                continue;
            }
            Kept.push_back(Rule);
        }
        if (bRemovedAny)
        {
            CommitAutomaticProfileRules(std::move(Kept),
                QStringLiteral("Removed rules referencing deleted profile"));
        }
        else
        {
            // 规则未变，但 profile 列表变了，刷新查找表让残留 invalid 状态更新。
            RefreshRuleLookup();
        }
    }
    else
    {
        RefreshRuleLookup();
    }

    // profile 集合变化后重新求值当前前台，可能需要回退/命中新的目标。
    EvaluateAndApplyAutomaticProfile();
    return true;
}

bool ZAppController::addAutomaticProfileRule(QString processName, QString profileId)
{
    auto Normalized = NormalizeProcessName(processName.toStdString());
    if (!Normalized.has_value())
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Add rule failed: invalid process name"));
        return false;
    }

    if (profileId.isEmpty())
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Add rule failed: profileId is empty"));
        return false;
    }

    // 进程名唯一：已存在同一规范化进程名的规则时拒绝。
    for (const auto& Rule : AutomaticRules)
    {
        if (Rule.ProcessName == *Normalized)
        {
            SetAutomaticProfileMessage(
                QStringLiteral("Add rule failed: a rule for \"%1\" already exists")
                    .arg(QString::fromStdString(*Normalized)));
            return false;
        }
    }

    SAutoProfileRule NewRule;
    NewRule.Id = GenerateRuleId();
    NewRule.bEnabled = true;
    NewRule.ProcessName = *Normalized;
    NewRule.ProfileId = profileId.toStdString();

    TVector<SAutoProfileRule> Next = AutomaticRules;
    Next.push_back(std::move(NewRule));
    return CommitAutomaticProfileRules(std::move(Next),
        QStringLiteral("Rule added: %1").arg(QString::fromStdString(*Normalized)));
}

bool ZAppController::removeAutomaticProfileRule(QString ruleId)
{
    if (ruleId.isEmpty())
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Remove rule failed: ruleId is empty"));
        return false;
    }

    const StdString Target = ruleId.toStdString();
    TVector<SAutoProfileRule> Next;
    Next.reserve(AutomaticRules.size());
    bool bFound = false;
    for (const auto& Rule : AutomaticRules)
    {
        if (Rule.Id == Target)
        {
            bFound = true;
            continue;
        }
        Next.push_back(Rule);
    }

    if (!bFound)
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Remove rule failed: rule not found"));
        return false;
    }

    return CommitAutomaticProfileRules(std::move(Next),
        QStringLiteral("Rule removed"));
}

bool ZAppController::setAutomaticProfileRuleEnabled(QString ruleId, bool enabled)
{
    if (ruleId.isEmpty())
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Set rule enabled failed: ruleId is empty"));
        return false;
    }

    const StdString Target = ruleId.toStdString();
    TVector<SAutoProfileRule> Next = AutomaticRules;
    bool bFound = false;
    for (auto& Rule : Next)
    {
        if (Rule.Id == Target)
        {
            bFound = true;
            if (Rule.bEnabled == enabled)
            {
                // 状态未变，无需写盘。
                return true;
            }
            Rule.bEnabled = enabled;
            break;
        }
    }

    if (!bFound)
    {
        SetAutomaticProfileMessage(
            QStringLiteral("Set rule enabled failed: rule not found"));
        return false;
    }

    return CommitAutomaticProfileRules(std::move(Next),
        enabled ? QStringLiteral("Rule enabled") : QStringLiteral("Rule disabled"));
}

void ZAppController::reportForegroundListenerUnavailable()
{
    // 只提示一次：自动切换不可用，但手动切换与其余功能照常。
    const QString Message = QStringLiteral(
        "Automatic switching is unavailable: could not start the foreground "
        "listener. Manual profile switching still works.");
    SetAutomaticProfileMessage(Message);
    AppendLog(QStringLiteral("Error"), Message);
}

void ZAppController::handleForegroundApplicationChanged(QString processName)
{
    auto Normalized = NormalizeProcessName(processName.toStdString());
    // 非法/空前台进程名视为“未知前台”，缓存为空并回退求值。
    const StdString NewName = Normalized.value_or(StdString());

    // 已观察过且前台名未变化：不重复求值，避免高频事件抖动。
    // 首次观察即使为空（未知前台）也要求值一次，以便按需求回退 Default。
    if (bForegroundObserved && NewName == ForegroundProcessNameCache)
    {
        return;
    }

    bForegroundObserved = true;
    ForegroundProcessNameCache = NewName;
    // 前台变化时清除上一次失败抑制，允许对新前台重新尝试。
    LastFailedAutomaticTargetId.clear();
    emit foregroundProcessChanged();

    EvaluateAndApplyAutomaticProfile();
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

    MarkProfileDirty();
    bool bSaved = AutosaveActiveProfile();

    AppendLog(QStringLiteral("Success"),
        bSaved ? QStringLiteral("Binding removed and saved: %1").arg(ruleId)
               : QStringLiteral("Binding removed, save failed: %1").arg(ruleId));
    return true;
}

bool ZAppController::setBindingEnabled(QString ruleId, bool enabled)
{
    if (ruleId.isEmpty())
    {
        EmitRuntimeError(QStringLiteral("SetEnabled failed: ruleId is empty"));
        return false;
    }

    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        EmitRuntimeError(QStringLiteral("SetEnabled failed: runtime not initialized"));
        return false;
    }

    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    StdString RuleIdStd = ruleId.toStdString();

    auto Iterator = std::find_if(Profile.Rules.begin(), Profile.Rules.end(),
        [&RuleIdStd](const SMappingRule& Rule) { return Rule.Id == RuleIdStd; });

    if (Iterator == Profile.Rules.end())
    {
        EmitRuntimeError(
            QStringLiteral("SetEnabled failed: rule \"%1\" not found").arg(ruleId));
        return false;
    }

    // 目标状态与当前一致，无需变更
    if (Iterator->bEnabled == enabled)
    {
        return true;
    }

    Iterator->bEnabled = enabled;
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Profile));
    RefreshMappingRuleModelFromHost();

    MarkProfileDirty();
    bool bSaved = AutosaveActiveProfile();

    QString Action = enabled ? QStringLiteral("Binding enabled")
                             : QStringLiteral("Binding disabled");
    AppendLog(QStringLiteral("Success"),
        bSaved ? QStringLiteral("%1 and saved: %2").arg(Action, ruleId)
               : QStringLiteral("%1, save failed: %2").arg(Action, ruleId));
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

StdPath ZAppController::ResolveProfilesDirectory() const
{
    // 测试注入的隔离目录优先
    if (!ProfileDirectoryOverride.empty())
    {
        return ProfileDirectoryOverride;
    }

    QString DataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (DataPath.isEmpty())
    {
        return StdPath();
    }
    return StdPath((DataPath + QStringLiteral("/profiles")).toStdString());
}

StdPath ZAppController::ResolveRuleFilePath() const
{
    // 测试注入的隔离目录优先。规则文件放进 override 下的 config 子目录，与 profiles
    // 目录分离：ProfileManager 只非递归扫描 profiles 目录本身，故规则 JSON 不会被
    // 误当作 profile 扫描（生产环境下二者本就分属 AppConfig 与 AppData/profiles）。
    // 仍在同一 temp 根下，便于测试统一清理。
    if (!ProfileDirectoryOverride.empty())
    {
        return ProfileDirectoryOverride / "config" / "automatic_profile_rules.json";
    }

    QString DataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    if (DataPath.isEmpty())
    {
        return StdPath();
    }
    return StdPath(
        (DataPath + QStringLiteral("/automatic_profile_rules.json")).toStdString());
}

QHash<QString, QString> ZAppController::BuildProfileLookup() const
{
    QHash<QString, QString> Lookup;
    for (const SProfileInfo& Info : ProfileManager.GetProfiles())
    {
        Lookup.insert(QString::fromStdString(Info.Id),
            QString::fromStdString(Info.Name));
    }
    return Lookup;
}

TVector<StdString> ZAppController::CollectProfileIds() const
{
    TVector<StdString> Ids;
    const auto& Profiles = ProfileManager.GetProfiles();
    Ids.reserve(Profiles.size());
    for (const SProfileInfo& Info : Profiles)
    {
        Ids.push_back(Info.Id);
    }
    return Ids;
}

void ZAppController::SetAutomaticProfileMessage(const QString& Message)
{
    CachedAutomaticProfileMessage = Message;
    emit automaticProfileStatusChanged();
}

void ZAppController::LoadAutomaticProfileRules()
{
    ZAutoProfileRuleStore Store(ResolveRuleFilePath());
    auto Result = Store.Load();
    if (!Result)
    {
        // 规则文件损坏：按空集处理并报错，不覆盖原文件（待用户首次成功 mutation 时替换）。
        AutomaticRules.clear();
        SetAutomaticProfileMessage(
            QStringLiteral("Automatic profile rules file is corrupt; starting empty"));
        AppendLog(QStringLiteral("Warning"),
            QStringLiteral("Automatic profile rules file is corrupt; starting empty"));
        return;
    }

    auto Loaded = std::move(Result).TakeValue();
    AutomaticRules = std::move(Loaded.Rules);

    // 加载时发生了规范化/去重/ID 修复：立即写回一次持久化规范形式。
    if (Loaded.bRepaired)
    {
        auto SaveResult = Store.Save(AutomaticRules);
        if (!SaveResult)
        {
            AppendLog(QStringLiteral("Warning"),
                QStringLiteral("Failed to persist repaired automatic profile rules"));
        }
    }
}

void ZAppController::PushRulesToModel()
{
    AutoProfileRuleModelInstance.SetProfileLookup(BuildProfileLookup());
    AutoProfileRuleModelInstance.ReplaceRules(AutomaticRules);
}

void ZAppController::RefreshRuleLookup()
{
    AutoProfileRuleModelInstance.SetProfileLookup(BuildProfileLookup());
}

bool ZAppController::CommitAutomaticProfileRules(
    TVector<SAutoProfileRule> NextRules, const QString& SuccessLog)
{
    ZAutoProfileRuleStore Store(ResolveRuleFilePath());
    auto Result = Store.Save(NextRules);
    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        SetAutomaticProfileMessage(
            QStringLiteral("Save rules failed: %1").arg(ErrorMessage));
        AppendLog(QStringLiteral("Error"),
            QStringLiteral("Save automatic profile rules failed: %1").arg(ErrorMessage));
        return false;
    }

    // 写盘成功后才提交内存状态与模型，保证磁盘/内存一致。
    AutomaticRules = std::move(NextRules);
    PushRulesToModel();
    SetAutomaticProfileMessage(SuccessLog);
    AppendLog(QStringLiteral("Success"), SuccessLog);

    // 规则变化可能改变当前前台的匹配结果，重新求值。
    LastFailedAutomaticTargetId.clear();
    EvaluateAndApplyAutomaticProfile();
    return true;
}

void ZAppController::EvaluateAndApplyAutomaticProfile()
{
    // profile 未就绪时不求值（initializeProfiles 之前不应触发自动切换）。
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        return;
    }
    if (!ProfileManager.GetActiveProfileInfo())
    {
        return;
    }

    // 尚未观察到任何前台（初始化、新建配置、增删规则等管理操作发生在首次前台观察前）：
    // 不做自动决策，保留用户当前选择，避免把管理操作误当作“前台未知”而强制切到 Default。
    // 一旦观察到前台（哪怕是不可解析的空前台），才按需求回退 Default。
    if (!bForegroundObserved)
    {
        return;
    }

    // 前台已观察但不可解析（空进程名，如安全桌面/无标题窗口）时，按需求视为“无规则命中”
    // 回退 Default：ResolveAutomaticProfileId 对空进程名归一化失败即返回 Default。
    const StdString TargetId = ResolveAutomaticProfileId(
        ForegroundProcessNameCache, AutomaticRules, CollectProfileIds());

    // 已是目标配置：无需切换。清除失败抑制（当前状态已一致）。
    if (QString::fromStdString(TargetId) == ActiveProfileId())
    {
        LastFailedAutomaticTargetId.clear();
        return;
    }

    // 上一次对相同目标已失败：抑制高频重试，直到前台/规则/profile 集合变化。
    if (TargetId == LastFailedAutomaticTargetId)
    {
        return;
    }

    if (!SwitchProfileInternal(TargetId, /*bAutomatic=*/true))
    {
        LastFailedAutomaticTargetId = TargetId;
    }
    else
    {
        LastFailedAutomaticTargetId.clear();
    }
}

void ZAppController::ApplyLoadedProfile(
    SLoadedProfile Loaded, const QString& SuccessMessage, bool bListChanged)
{
    // 应用 snapshot 到 Runtime 并刷新 UI model
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(Loaded.Profile));
    RefreshMappingRuleModelFromHost();

    // 成功加载后统一清除 dirty；语义信号由各顶层命令按矩阵显式发出
    CachedProfileMessage = SuccessMessage;
    bProfileDirty = false;
    CachedProfileSaveState = QStringLiteral("clean");

    if (bListChanged)
    {
        emit profileListChanged();
    }
    emit activeProfileChanged();
    emit profileStatusChanged();
}

bool ZAppController::SaveDirtyProfileBeforeSelectionChange()
{
    // 不脏则无需保存，直接放行
    if (!bProfileDirty)
    {
        return true;
    }

    // 此时 manager 的当前 ID 尚未变化，只会写切换前活动项的文件
    return saveActiveProfile();
}

void ZAppController::SetProfileOperationError(const QString& Message)
{
    // 记录错误消息、置 error 状态、发状态信号，然后走统一错误输出。
    // 绝不修改 bProfileDirty：由调用前的操作语义保留原值。
    CachedProfileMessage = Message;
    CachedProfileSaveState = QStringLiteral("error");
    emit profileStatusChanged();
    EmitRuntimeError(Message);
}

void ZAppController::MarkProfileDirty()
{
    bProfileDirty = true;
    CachedProfileSaveState = QStringLiteral("dirty");
    emit profileStatusChanged();
}

bool ZAppController::SaveActiveProfileInternal(bool bAutosave)
{
    // Runtime 未 initialize 时拒绝保存
    auto Status = Bootstrap.GetStatus();
    if (Status.State != EApplicationBootstrapState::Ready
        && Status.State != EApplicationBootstrapState::Running)
    {
        SetProfileOperationError(
            QStringLiteral("Save failed: runtime not initialized"));
        return false;
    }

    // 把当前 Runtime snapshot 写回 manager 当前配置文件
    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    auto Result = ProfileManager.SaveActiveProfile(Profile);

    if (!Result)
    {
        auto ErrorMessage = QString::fromStdString(Result.Failure().Message);
        // 保存失败保留 dirty 原值（绝不修改）：mapping 变更前已由 MarkProfileDirty()
        // 置 true；clean 配置遇临时写入失败仍应保持 clean，避免阻塞后续切换/新建/删除。
        CachedProfileMessage =
            QStringLiteral("Profile save failed: %1").arg(ErrorMessage);
        CachedProfileSaveState = QStringLiteral("error");
        emit profileStatusChanged();
        if (bAutosave)
        {
            AppendLog(QStringLiteral("Error"), CachedProfileMessage);
        }
        else
        {
            EmitRuntimeError(CachedProfileMessage);
        }
        return false;
    }

    // 保存成功
    CachedProfileMessage = QStringLiteral("Profile saved");
    bProfileDirty = false;
    CachedProfileSaveState = QStringLiteral("clean");

    if (!bAutosave)
    {
        AppendLog(QStringLiteral("Success"), CachedProfileMessage);
    }

    emit profileStatusChanged();
    emit profileSaved(ProfilePath());
    return true;
}

bool ZAppController::AutosaveActiveProfile()
{
    return SaveActiveProfileInternal(true);
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
        else if (actionValue == QStringLiteral("Button4"))
        {
            ButtonIndex = 3;
        }
        else if (actionValue == QStringLiteral("Button5"))
        {
            ButtonIndex = 4;
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
    else if (actionKind == QStringLiteral("MouseMove"))
    {
        if (!ActionCatalogModelInstance.Contains(actionKind, actionValue))
        {
            EmitRuntimeError(
                QStringLiteral("Apply failed: unknown mouse move value \"%1\"")
                    .arg(actionValue));
            return false;
        }

        Action.Type = EActionType::MouseMove;
        Action.Payload = SMouseMoveAction{.DeltaX = 0.0f, .DeltaY = 0.0f};
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

    // 兼容性校验：Axis2D 只能绑定 MouseMove，MouseMove 只能绑定 Axis2D
    if (Input.ControlType == EInputControlType::Axis2D
        && Action.Type != EActionType::MouseMove)
    {
        EmitRuntimeError(
            QStringLiteral("Apply failed: stick input only supports MouseMove"));
        return false;
    }
    if (Action.Type == EActionType::MouseMove
        && Input.ControlType != EInputControlType::Axis2D)
    {
        EmitRuntimeError(
            QStringLiteral("Apply failed: MouseMove requires stick input"));
        return false;
    }

    // 构造规则
    SMappingRule Rule;
    Rule.Id = ControlIdStd;
    Rule.DisplayName = ControlIdStd;
    Rule.bEnabled = true;
    Rule.Input = std::move(Input);
    Rule.Output.Action = std::move(Action);

    // MouseMove 使用 Analog 模式和专用灵敏度
    if (Rule.Output.Action.Type == EActionType::MouseMove)
    {
        Rule.Output.Mode = EMappingActionMode::Analog;
        Rule.Output.Sensitivity = 12.0f;
    }
    else
    {
        Rule.Output.Mode = EMappingActionMode::PressRelease;
        Rule.Output.Sensitivity = 1.0f;
    }

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

    MarkProfileDirty();
    bool bSaved = AutosaveActiveProfile();

    AppendLog(QStringLiteral("Success"),
        bSaved ? QStringLiteral("Applied and saved: %1 → %2:%3")
                   .arg(controlId, actionKind, actionValue)
               : QStringLiteral("Applied, save failed: %1 → %2:%3")
                   .arg(controlId, actionKind, actionValue));
    return true;
}

// ── controlId 到输入类型推断 ──

bool ZAppController::InferInputFromControlId(
    const StdString& ControlId, SMappingInput& OutInput)
{
    OutInput.ControlId = ControlId;

    // Axis2D：摇杆输入，用于 MouseMove 等模拟量映射
    if (ControlId == MappyZ::ControlId::LeftStick
        || ControlId == MappyZ::ControlId::RightStick)
    {
        OutInput.ControlType = EInputControlType::Axis2D;
        OutInput.EventType = EInputEventType::Changed;
        OutInput.Deadzone = 0.20f;
        OutInput.Threshold = 0.0f;
        return true;
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

    // Button 白名单：面板按钮、功能键、肩键、摇杆按下、摇杆方向虚拟输入
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
        || ControlId == MappyZ::ControlId::RightStickButton
        || ControlId == MappyZ::ControlId::LeftStickUp
        || ControlId == MappyZ::ControlId::LeftStickDown
        || ControlId == MappyZ::ControlId::LeftStickLeft
        || ControlId == MappyZ::ControlId::LeftStickRight
        || ControlId == MappyZ::ControlId::RightStickUp
        || ControlId == MappyZ::ControlId::RightStickDown
        || ControlId == MappyZ::ControlId::RightStickLeft
        || ControlId == MappyZ::ControlId::RightStickRight)
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
