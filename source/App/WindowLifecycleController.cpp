// ZWindowLifecycleController 实现。
// Close 通过 eventFilter 拦截并返回 true，阻止事件继续进入 QQuickWindow/QML；
// Minimize 状态变化用 queued 调用隐藏，避免在状态事件处理中直接改窗口状态导致重入。

#include "App/WindowLifecycleController.h"

#include <QEvent>
#include <QTimer>
#include <QWindow>

namespace MappyZ
{

ZWindowLifecycleController::ZWindowLifecycleController(QObject* Parent)
    : QObject(Parent)
{
}

ZWindowLifecycleController::~ZWindowLifecycleController()
{
    // 窗口仍存活时主动移除 event filter；已销毁时 QPointer 为空，无需处理。
    if (Window)
    {
        Window->removeEventFilter(this);
    }
}

void ZWindowLifecycleController::AttachWindow(QWindow* NewWindow)
{
    if (Window == NewWindow)
    {
        return;
    }

    // 重复 Attach：先从旧窗口移除，避免悬空监听和重复处理。
    if (Window)
    {
        Window->removeEventFilter(this);
    }

    Window = NewWindow;

    if (Window)
    {
        Window->installEventFilter(this);
    }
}

void ZWindowLifecycleController::SetTrayAvailable(bool bAvailable)
{
    bTrayAvailable = bAvailable;
}

void ZWindowLifecycleController::BeginExit()
{
    bExiting = true;
}

void ZWindowLifecycleController::RestoreWindow()
{
    if (!Window)
    {
        return;
    }

    // 清除 minimized 状态后再显示，避免恢复出的窗口仍是最小化。
    Window->setWindowStates(Window->windowStates() & ~Qt::WindowMinimized);
    Window->show();
    Window->raise();
    Window->requestActivate();
}

void ZWindowLifecycleController::HideWindow()
{
    if (Window)
    {
        Window->hide();
    }
}

void ZWindowLifecycleController::HideWindowIfMinimized()
{
    // 排队执行时重新确认状态：窗口已被恢复则不再隐藏。
    if (Window && Window->windowStates().testFlag(Qt::WindowMinimized))
    {
        Window->hide();
    }
}

bool ZWindowLifecycleController::eventFilter(QObject* Watched, QEvent* Event)
{
    // 只在托盘可用且未进入真正退出流程时接管窗口事件。
    if (Watched == Window && bTrayAvailable && !bExiting)
    {
        if (Event->type() == QEvent::Close)
        {
            Event->ignore();
            HideWindow();
            // 返回 true 阻止事件继续进入 QQuickWindow/QML 的关闭流程。
            return true;
        }

        if (Event->type() == QEvent::WindowStateChange)
        {
            if (Window && Window->windowStates().testFlag(Qt::WindowMinimized))
            {
                // queued 隐藏，避免在状态事件处理中直接修改窗口状态导致重入。
                QTimer::singleShot(0, this, &ZWindowLifecycleController::HideWindowIfMinimized);
            }
        }
    }

    return QObject::eventFilter(Watched, Event);
}

}  // namespace MappyZ
