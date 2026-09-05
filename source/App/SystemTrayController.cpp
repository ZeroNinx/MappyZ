// ZSystemTrayController 实现。
// 构造真实托盘图标和只含 Exit 的右键菜单，把用户操作翻译成 RestoreRequested /
// ExitRequested 信号。QMenu 是 QWidget 无法以 QObject 为父对象，故手动持有并在析构时释放。

#include "App/SystemTrayController.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace MappyZ
{

ZSystemTrayController::ZSystemTrayController(const QIcon& Icon, QObject* Parent)
    : QObject(Parent)
{
    // 右键菜单：本轮只提供 Exit。QMenu 无 QObject 父对象，手动释放。
    Menu = new QMenu();
    ExitAction = Menu->addAction(QStringLiteral("Exit"));

    TrayIcon = new QSystemTrayIcon(Icon, this);
    TrayIcon->setToolTip(QStringLiteral("MappyZ"));
    TrayIcon->setContextMenu(Menu);

    connect(ExitAction, &QAction::triggered, this,
        [this]()
        {
            // Exit 是终态命令：请求退出后立即禁用菜单项，既防止重复发射
            // ExitRequested，也向用户表达“退出进行中”。用 action 自身的 enabled
            // 状态作为权威状态，无需额外的防重入标志。
            ExitAction->setEnabled(false);
            emit ExitRequested();
        });

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
    delete Menu;
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
