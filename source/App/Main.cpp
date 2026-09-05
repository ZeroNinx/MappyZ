#include <cstdio>

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
