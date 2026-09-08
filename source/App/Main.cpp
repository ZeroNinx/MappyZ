#include <cstdio>

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSharedMemory>
#include <QWindow>
#include <Qt>
#include <QtQml/qqmlextensionplugin.h>

#include "App/SystemTrayController.h"
#include "App/WindowLifecycleController.h"
#include "UI/Bridge/AppController.h"

// QML 模块提取为独立 STATIC 库后，需要显式导入插件
Q_IMPORT_QML_PLUGIN(MappyZUIPlugin)

int main(int ArgCount, char* Arguments[])
{
    // QSystemTrayIcon / QMenu 需要 QApplication（Widgets）。
    QApplication App(ArgCount, Arguments);

    // 单实例守卫：尝试创建命名共享内存段，创建失败即说明已有实例在运行，
    // 本次启动直接退出，不再加载 QML / 初始化 runtime，避免重复抢占托盘、SDL 等资源。
    // 段随本进程存活，进程结束（含崩溃）时由系统回收（Windows 内核对象，无残留）。
    QSharedMemory SingleInstanceGuard(QStringLiteral("MappyZ_SingleInstanceGuard"));
    if (!SingleInstanceGuard.create(1))
    {
        std::fprintf(stderr, "[MappyZ] 检测到已有实例在运行，退出本次启动\n");
        return 0;
    }

    const QIcon AppIcon(QStringLiteral(":/assets/icon.ico"));
    QApplication::setWindowIcon(AppIcon);

    MappyZ::ZAppController AppController;

    // 托盘适配器：查询可用性并驱动恢复 / 退出命令。
    MappyZ::ZSystemTrayController Tray(AppIcon);
    const bool bTrayAvailable = Tray.IsAvailable();

    // 托盘可用时最后窗口关闭不退出；回退模式下关闭主窗口正常退出。
    QApplication::setQuitOnLastWindowClosed(!bTrayAvailable);

    // 窗口生命周期策略控制器（不依赖托盘实现，可测）。
    // 构造早于 Engine，确保其生命周期长于 QML 根窗口。
    MappyZ::ZWindowLifecycleController WindowLifecycle;
    WindowLifecycle.SetTrayAvailable(bTrayAvailable);

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &AppController);

    QObject::connect(
        &Engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &App,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 托盘配置同步：把 controller 的配置列表和当前项渲染到托盘菜单。
    // controller 是配置状态的唯一来源，托盘只渲染，不复制业务规则。
    auto SyncTrayProfiles = [&Tray, &AppController]()
    {
        Tray.SetProfiles(
            AppController.ProfileEntries(),
            AppController.ActiveProfileId());
    };

    // 列表或当前项变化都重新同步托盘菜单。
    QObject::connect(&AppController, &MappyZ::ZAppController::profileListChanged,
        &Tray, SyncTrayProfiles);
    QObject::connect(&AppController, &MappyZ::ZAppController::activeProfileChanged,
        &Tray, SyncTrayProfiles);

    // 托盘点击某项：请求 controller 切换。用 QueuedConnection 把切换推迟到当前
    // QAction::triggered 派发完全解栈之后再执行——否则切换会同步发出
    // activeProfileChanged 触发菜单重建，进而删除正在发信号的 QAction，
    // 造成原生菜单事件处理期间的悬空对象/崩溃。延迟后成功切换依赖
    // activeProfileChanged 自动同步，失败时显式重建以恢复点击产生的临时勾选，
    // 二者此时都不再位于 triggered 调用栈内，删除 action 是安全的。
    QObject::connect(&Tray, &MappyZ::ZSystemTrayController::ProfileSwitchRequested,
        &AppController,
        [&AppController, &SyncTrayProfiles](const QString& ProfileId)
        {
            if (!AppController.switchProfile(ProfileId))
            {
                SyncTrayProfiles();
            }
        },
        Qt::QueuedConnection);

    Engine.loadFromModule("MappyZUI", "Main");

    if (Engine.rootObjects().isEmpty())
    {
        // objectCreationFailed 失败路径：根对象未创建，直接退出。
        return -1;
    }

    // 安全取得根对象并验证它是 QWindow。
    QObject* RootObject = Engine.rootObjects().constFirst();
    QWindow* MainWindow = qobject_cast<QWindow*>(RootObject);
    if (!MainWindow)
    {
        std::fprintf(stderr, "[MappyZ] 错误: QML 根对象不是 QWindow，无法建立窗口生命周期\n");
        return -1;
    }

    WindowLifecycle.AttachWindow(MainWindow);

    QObject::connect(&Tray, &MappyZ::ZSystemTrayController::RestoreRequested,
        &WindowLifecycle, &MappyZ::ZWindowLifecycleController::RestoreWindow);

    QObject::connect(&Tray, &MappyZ::ZSystemTrayController::ExitRequested,
        &App,
        [&WindowLifecycle]()
        {
            // 先解除关闭拦截，再请求退出，交由 aboutToQuit 做统一清理。
            WindowLifecycle.BeginExit();
            QCoreApplication::quit();
        });

    // 根对象创建成功后显式同步一次，覆盖启动信号时序差异（此时配置已初始化）。
    SyncTrayProfiles();

    if (bTrayAvailable)
    {
        // 托盘可用：显示托盘图标，主窗口保持隐藏（QML 中 visible: false）。
        Tray.Show();
    }
    else
    {
        // 托盘不可用：直接显示主窗口，避免应用不可访问。
        std::fprintf(stderr, "[MappyZ] 警告: 系统托盘不可用，回退为显示主窗口\n");
        MainWindow->show();
    }

    // 退出清理路径：隐藏托盘图标 -> 停 pump -> 停 runtime。
    QObject::connect(&App, &QCoreApplication::aboutToQuit,
        &App,
        [&Tray, &AppController]()
        {
            Tray.Hide();
            AppController.stopPumpTimer();
            AppController.stopRuntime();
        });

    return App.exec();
}
