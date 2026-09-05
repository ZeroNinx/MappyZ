// 窗口生命周期策略控制器。
// 负责“Close / Minimize -> 隐藏还是放行”的窗口事件策略，只依赖 Qt Core/Gui，
// 不包含 QSystemTrayIcon、QMenu 或 QAction。托盘可用性由装配层通过
// SetTrayAvailable 注入，控制器自身不探测系统托盘，因此可在 offscreen 环境下
// 用真实 QWindow 做自动化测试。

#pragma once

#include <QObject>
#include <QPointer>

class QWindow;
class QEvent;

namespace MappyZ
{

class ZWindowLifecycleController final : public QObject
{
    Q_OBJECT

public:
    explicit ZWindowLifecycleController(QObject* Parent = nullptr);
    ~ZWindowLifecycleController() override;

    // 绑定主窗口。重复调用会先从旧窗口移除 event filter。不拥有窗口。
    void AttachWindow(QWindow* Window);

    // 由装配层设置托盘是否可用：
    // 可用时 Close 被拦截并隐藏；不可用时放行普通关闭语义。
    void SetTrayAvailable(bool bAvailable);

    // 进入真正退出流程：解除关闭拦截，后续 Close 不再转为隐藏。
    void BeginExit();

    // 恢复并激活主窗口：清除 minimized 状态，调用 show/raise/requestActivate。
    void RestoreWindow();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    // Close 拦截时立即隐藏窗口。
    void HideWindow();

    // 最小化的 queued 隐藏槽：执行时重新确认窗口仍处于最小化才隐藏，
    // 避免在排队期间用户已恢复窗口却被过期的隐藏请求再次隐藏。
    void HideWindowIfMinimized();

    // 使用 QPointer，窗口被 QML engine 销毁后指针自动失效。
    QPointer<QWindow> Window;

    bool bTrayAvailable = false;
    bool bExiting = false;
};

}  // namespace MappyZ
