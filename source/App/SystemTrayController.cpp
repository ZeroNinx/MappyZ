// ZSystemTrayController 实现。
// 构造真实托盘图标和右键菜单（末尾唯一的 Exit），把用户操作翻译成 RestoreRequested /
// ExitRequested / ProfileSwitchRequested 信号。QMenu 是 QWidget 无法以 QObject 为父对象，
// 故手动持有并在析构时释放。

#include "App/SystemTrayController.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QVariantMap>

namespace MappyZ
{

ZSystemTrayController::ZSystemTrayController(const QIcon& Icon, QObject* Parent)
    : QObject(Parent)
{
    // 右键菜单：构造时只创建空菜单，再由 helper 建立末尾唯一的 Exit。
    // profile action 由后续 SetProfiles() 动态插入到 Exit 之前。
    Menu = new QMenu();
    BuildExitAction();

    TrayIcon = new QSystemTrayIcon(Icon, this);
    TrayIcon->setToolTip(QStringLiteral("MappyZ"));
    TrayIcon->setContextMenu(Menu);

    connect(TrayIcon, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason Reason)
        {
            // 单击 / 双击请求恢复窗口；Context 激活只打开系统菜单，不额外切换窗口状态。
            if (Reason == QSystemTrayIcon::Trigger
                || Reason == QSystemTrayIcon::DoubleClick)
            {
                emit RestoreRequested();
            }
        });
}

ZSystemTrayController::~ZSystemTrayController()
{
    // TrayIcon 以 this 为父对象自动释放；Menu 无父对象需手动释放。
    // ProfileActionGroup 以 this 为父对象随之释放，其 action 作为组子对象一并销毁。
    delete Menu;
}

void ZSystemTrayController::BuildExitAction()
{
    // Exit 是终态命令：请求退出后立即禁用菜单项，既防止重复发射 ExitRequested，
    // 也向用户表达“退出进行中”。用 action 自身的 enabled 状态作为权威状态。
    ExitAction = Menu->addAction(QStringLiteral("Exit"));
    connect(ExitAction, &QAction::triggered, this,
        [this]()
        {
            ExitAction->setEnabled(false);
            emit ExitRequested();
        });
}

void ZSystemTrayController::SetProfiles(
    const QVariantList& ProfileEntries,
    const QString& ActiveProfileId)
{
    // 先移除上一批 profile action 与 separator。删除 action 时 Qt 会自动把它从
    // 菜单中摘除；action 作为 ProfileActionGroup 的子对象随组一并销毁。
    if (ProfileActionGroup)
    {
        delete ProfileActionGroup;
        ProfileActionGroup = nullptr;
    }
    ProfileActions.clear();
    if (SeparatorAction)
    {
        delete SeparatorAction;
        SeparatorAction = nullptr;
    }

    // 空列表只保留 Exit，不插入 separator。正常初始化成功后列表至少有一项。
    if (ProfileEntries.isEmpty())
    {
        return;
    }

    // 互斥分组：同一时刻只有当前项打勾。组以 this 为父对象。
    ProfileActionGroup = new QActionGroup(this);
    ProfileActionGroup->setExclusive(true);

    for (const QVariant& Entry : ProfileEntries)
    {
        const QVariantMap Fields = Entry.toMap();
        const QString ProfileId = Fields.value(QStringLiteral("id")).toString();
        const QString ProfileName = Fields.value(QStringLiteral("name")).toString();

        QAction* Action = new QAction(ProfileName, ProfileActionGroup);
        Action->setData(ProfileId);
        Action->setCheckable(true);
        Action->setChecked(ProfileId == ActiveProfileId);
        // objectName 供测试按稳定 ID 定位。
        Action->setObjectName(QStringLiteral("profileAction_") + ProfileId);
        ProfileActionGroup->addAction(Action);

        connect(Action, &QAction::triggered, this,
            [this, Action]()
            {
                emit ProfileSwitchRequested(Action->data().toString());
            });

        // 插入到 Exit 之前，逐项追加即可保持传入顺序。
        Menu->insertAction(ExitAction, Action);
        ProfileActions.append(Action);
    }

    // separator 始终位于 profile actions 和 Exit 之间。
    SeparatorAction = Menu->insertSeparator(ExitAction);
}

bool ZSystemTrayController::IsAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void ZSystemTrayController::Show()
{
    if (TrayIcon)
    {
        TrayIcon->show();
    }
}

void ZSystemTrayController::Hide()
{
    if (TrayIcon)
    {
        TrayIcon->hide();
    }
}

}  // namespace MappyZ
