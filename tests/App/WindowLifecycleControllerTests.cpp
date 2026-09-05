// ZWindowLifecycleController 单元测试。
// 使用真实 QWindow（offscreen）驱动窗口事件策略，验证 Close / Minimize 隐藏、
// RestoreWindow 恢复，以及托盘不可用 / 真正退出时不拦截关闭。
// 不构造 QSystemTrayIcon：托盘可用性和原生菜单展示不作为 CI 前提。
//
// 事件通过 QCoreApplication::sendEvent 直接投递，绕开各平台差异，保证 offscreen 下确定性。

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QEvent>
#include <QWindow>
#include <QWindowStateChangeEvent>

#include "App/WindowLifecycleController.h"

using namespace MappyZ;

namespace
{

// 处理挂起事件，让 queued 隐藏（QTimer::singleShot(0)）生效。
void DrainEvents()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

// 向窗口投递一个默认 accepted 的 Close 事件；返回事件是否仍被 accept。
// 被 event filter 拦截时会调用 ignore()，事件变为未 accept。
bool SendClose(QWindow& Window)
{
    QEvent CloseEvent(QEvent::Close);
    CloseEvent.setAccepted(true);
    QCoreApplication::sendEvent(&Window, &CloseEvent);
    return CloseEvent.isAccepted();
}

// 将窗口置为最小化并投递 WindowStateChange 事件，触发策略判断。
void SendMinimize(QWindow& Window)
{
    Window.setWindowStates(Window.windowStates() | Qt::WindowMinimized);
    QWindowStateChangeEvent StateEvent(Qt::WindowNoState);
    QCoreApplication::sendEvent(&Window, &StateEvent);
}

}  // namespace

TEST_CASE("Close is intercepted and window hidden when tray available",
    "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.show();
    DrainEvents();
    REQUIRE(Window.isVisible());

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&Window);

    const bool bStillAccepted = SendClose(Window);
    DrainEvents();

    // 被拦截：事件被 ignore、窗口隐藏，但窗口对象仍存活（可再次恢复）。
    REQUIRE_FALSE(bStillAccepted);
    REQUIRE_FALSE(Window.isVisible());

    Controller.RestoreWindow();
    DrainEvents();
    REQUIRE(Window.isVisible());
}

TEST_CASE("Minimize hides window when tray available", "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.show();
    DrainEvents();

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&Window);

    SendMinimize(Window);
    DrainEvents();  // queued hide 生效

    REQUIRE_FALSE(Window.isVisible());
}

TEST_CASE("RestoreWindow clears minimized state and shows window",
    "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.setWindowStates(Window.windowStates() | Qt::WindowMinimized);

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&Window);

    Controller.RestoreWindow();
    DrainEvents();

    REQUIRE(Window.isVisible());
    REQUIRE_FALSE(Window.windowStates().testFlag(Qt::WindowMinimized));
}

TEST_CASE("Repeated Close/Minimize/Restore stays consistent",
    "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.show();
    DrainEvents();

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&Window);

    for (int Round = 0; Round < 3; ++Round)
    {
        SendClose(Window);
        DrainEvents();
        REQUIRE_FALSE(Window.isVisible());

        Controller.RestoreWindow();
        DrainEvents();
        REQUIRE(Window.isVisible());

        SendMinimize(Window);
        DrainEvents();
        REQUIRE_FALSE(Window.isVisible());

        Controller.RestoreWindow();
        DrainEvents();
        REQUIRE(Window.isVisible());
        REQUIRE_FALSE(Window.windowStates().testFlag(Qt::WindowMinimized));
    }
}

TEST_CASE("Close is not intercepted after BeginExit", "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.show();
    DrainEvents();

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&Window);
    Controller.BeginExit();

    // 真正退出流程中不再拦截：事件保持 accepted，交给默认关闭语义。
    const bool bStillAccepted = SendClose(Window);
    REQUIRE(bStillAccepted);
}

TEST_CASE("Close is not intercepted when tray unavailable",
    "[App][WindowLifecycle]")
{
    QWindow Window;
    Window.show();
    DrainEvents();

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(false);
    Controller.AttachWindow(&Window);

    // 托盘不可用：保持普通窗口关闭语义，event filter 不 ignore。
    const bool bStillAccepted = SendClose(Window);
    REQUIRE(bStillAccepted);
}

TEST_CASE("AttachWindow twice does not double-handle events",
    "[App][WindowLifecycle]")
{
    QWindow FirstWindow;
    QWindow SecondWindow;
    SecondWindow.show();
    DrainEvents();

    ZWindowLifecycleController Controller;
    Controller.SetTrayAvailable(true);
    Controller.AttachWindow(&FirstWindow);
    Controller.AttachWindow(&SecondWindow);

    // 旧窗口已解除监听：关闭第一个窗口不应被拦截。
    QEvent FirstClose(QEvent::Close);
    FirstClose.setAccepted(true);
    QCoreApplication::sendEvent(&FirstWindow, &FirstClose);
    REQUIRE(FirstClose.isAccepted());

    // 当前窗口仍被正常拦截。
    const bool bSecondAccepted = SendClose(SecondWindow);
    DrainEvents();
    REQUIRE_FALSE(bSecondAccepted);
    REQUIRE_FALSE(SecondWindow.isVisible());
}
