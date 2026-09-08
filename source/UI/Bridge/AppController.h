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
#include <QVariantList>

#include "App/ApplicationBootstrap.h"
#include "Runtime/ProfileManager.h"
#include "UI/Bridge/DeviceModel.h"
#include "UI/Bridge/InputCaptureModel.h"
#include "UI/Bridge/InputStateModel.h"
#include "UI/Bridge/ActionCatalogModel.h"
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
    Q_PROPERTY(ZActionCatalogModel* actionCatalogModel READ ActionCatalogModel CONSTANT)
    Q_PROPERTY(QString activeProfileName READ ActiveProfileName NOTIFY activeProfileChanged)
    Q_PROPERTY(QString outputDisplayText READ OutputDisplayText NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString profilePath READ ProfilePath NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileMessage READ ProfileMessage NOTIFY profileStatusChanged)
    Q_PROPERTY(bool profileDirty READ IsProfileDirty NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileSaveState READ ProfileSaveState NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileDisplayText READ ProfileDisplayText NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileSaveDisplayText READ ProfileSaveDisplayText NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileSaveSeverity READ ProfileSaveSeverity NOTIFY profileStatusChanged)
    Q_PROPERTY(QString runtimeDisplayText READ RuntimeDisplayText NOTIFY runtimeStatusChanged)

    // ── 多配置列表 / 当前项 / 删除能力 ──
    Q_PROPERTY(QVariantList profileEntries READ ProfileEntries NOTIFY profileListChanged)
    Q_PROPERTY(QString activeProfileId READ ActiveProfileId NOTIFY activeProfileChanged)
    Q_PROPERTY(bool canDeleteProfile READ CanDeleteProfile NOTIFY profileListChanged)

public:
    // 生产构造：使用编译期开关的默认后端工厂
    explicit ZAppController(QObject* Parent = nullptr);

    // 测试构造：注入自定义后端工厂
    ZAppController(
        TInputBackendFactory InputFactory,
        TOutputBackendFactory OutputFactory,
        QObject* Parent = nullptr);

    // 测试构造：注入自定义后端工厂并指定隔离的配置目录
    ZAppController(
        TInputBackendFactory InputFactory,
        TOutputBackendFactory OutputFactory,
        StdPath ProfileDirectory,
        QObject* Parent = nullptr);

    ~ZAppController() override;

    // ── QML 属性读取 ──

    NODISCARD QString RuntimeState() const;
    NODISCARD QString RuntimeMessage() const;
    NODISCARD QString OutputState() const;
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
    NODISCARD ZActionCatalogModel* ActionCatalogModel();
    NODISCARD QString ActiveProfileName() const;
    NODISCARD QString OutputDisplayText() const;
    NODISCARD QString ProfilePath() const;
    NODISCARD QString ProfileMessage() const;
    NODISCARD bool IsProfileDirty() const;
    NODISCARD QString ProfileSaveState() const;
    NODISCARD QString ProfileDisplayText() const;
    NODISCARD QString ProfileSaveDisplayText() const;
    NODISCARD QString ProfileSaveSeverity() const;
    NODISCARD QString RuntimeDisplayText() const;

    // ── 多配置属性读取 ──

    // 返回 QML 可直接消费的列表，每项含 id/name 两个键。
    NODISCARD QVariantList ProfileEntries() const;

    // 当前配置的稳定 ID；未初始化时为空。
    NODISCARD QString ActiveProfileId() const;

    // 当前配置是否可删除（配置数量大于 1）。
    NODISCARD bool CanDeleteProfile() const;

    // ── QML invokable ──

    Q_INVOKABLE bool initializeRuntime();
    Q_INVOKABLE bool startRuntime();
    Q_INVOKABLE void stopRuntime();
    Q_INVOKABLE void pumpOnce();
    Q_INVOKABLE void startPumpTimer(int intervalMs = 16);
    Q_INVOKABLE void stopPumpTimer();
    Q_INVOKABLE bool applySelectedBinding(
        QString controlId, QString actionKind, QString actionValue);
    Q_INVOKABLE bool removeBinding(QString ruleId);
    Q_INVOKABLE bool setBindingEnabled(QString ruleId, bool enabled);

    // ── 多配置命令 ──

    // 启动时初始化配置目录，加载并应用当前配置。
    Q_INVOKABLE bool initializeProfiles();

    // 手动保存当前配置到其记录的文件路径。
    Q_INVOKABLE bool saveActiveProfile();

    // 切换到指定 ID 的配置；切换前自动保存当前脏配置。
    Q_INVOKABLE bool switchProfile(QString profileId);

    // 新建空白配置并立即切换过去；切换前自动保存当前脏配置。
    Q_INVOKABLE bool createProfile();

    // 重命名当前配置的显示名，不改变 ID 或文件。
    Q_INVOKABLE bool renameActiveProfile(QString newName);

    // 删除当前配置并切换到回退项；当前配置为最后一个时拒绝。
    Q_INVOKABLE bool deleteActiveProfile();

    // 测试辅助：替换 RuntimeHost 的 active profile 并刷新 UI model。
    // 不暴露给 QML，仅供 C++ 测试代码使用。
    void ReplaceActiveProfileForTest(SMappingProfile Profile);

signals:
    void runtimeStatusChanged();
    void pumpTimerRunningChanged();
    void lastPumpSummaryChanged();
    void runtimeError(QString message);
    void profileStatusChanged();
    void profileSaved(QString profilePath);
    void profileLoaded(QString profilePath);
    void profileListChanged();
    void activeProfileChanged();

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

    // 根据 controlId 推断输入控件类型和事件类型，不支持的控件返回 false
    static bool InferInputFromControlId(
        const StdString& ControlId, SMappingInput& OutInput);

    // 解析配置目录：优先使用注入的覆盖目录，否则用 AppDataLocation/profiles。
    NODISCARD StdPath ResolveProfilesDirectory() const;

    // 把一次成功的加载结果应用到 Runtime、UI model 和状态字段：
    // 替换 snapshot、刷新 model、清除 dirty，并按需发出列表/当前项信号。
    // 不发 profileLoaded/profileSaved，语义信号由各顶层命令按矩阵显式发出。
    void ApplyLoadedProfile(
        SLoadedProfile Loaded, const QString& SuccessMessage, bool bListChanged);

    // 在切换/新建/删除前，如当前配置为脏则先保存，返回是否成功。
    bool SaveDirtyProfileBeforeSelectionChange();

    // 记录一次配置操作失败：写状态为 error、更新消息、发信号，不触碰 dirty。
    void SetProfileOperationError(const QString& Message);

    // 标记 profile 为 dirty 状态
    void MarkProfileDirty();

    // save 统一实现，bAutosave 控制日志噪声
    bool SaveActiveProfileInternal(bool bAutosave);

    // 映射变更后自动保存当前配置
    bool AutosaveActiveProfile();

    // 注册 RuntimeHost event handler（设备连接/断开、输入事件）
    void RegisterEventHandlers();

    ZApplicationBootstrap Bootstrap;
    QTimer PumpTimer;

    // 配置文件生命周期的唯一管理者
    ZProfileManager ProfileManager;

    // 测试注入的配置目录覆盖；为空时使用 AppDataLocation/profiles
    StdPath ProfileDirectoryOverride;

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

    // 输出动作目录模型，供 QML BindingEditor action 选择绑定
    ZActionCatalogModel ActionCatalogModelInstance;

    // 事件日志模型，供 QML EventLogPanel 绑定
    ZLogModel LogModelInstance;

    // 最近一次保存操作的结果消息
    QString CachedProfileMessage;

    // profile 是否有未保存的修改
    bool bProfileDirty = false;

    // profile 保存状态："clean" / "dirty" / "error"
    QString CachedProfileSaveState = QStringLiteral("clean");
};

}  // namespace MappyZ
