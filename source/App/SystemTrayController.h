// 系统托盘适配器。
// 只负责真实托盘资源（QSystemTrayIcon、QMenu、唯一的 Exit QAction）与用户命令，
// 依赖 Qt Widgets。不了解窗口策略、Runtime、Profile 或映射逻辑，只通过信号对外表达
// “请求恢复窗口”和“请求退出”两种语义。

#pragma once

#include <QObject>

class QIcon;
class QSystemTrayIcon;
class QMenu;
class QAction;

namespace MappyZ
{

class ZSystemTrayController final : public QObject
{
    Q_OBJECT

public:
    explicit ZSystemTrayController(const QIcon& Icon, QObject* Parent = nullptr);
    ~ZSystemTrayController() override;

    // 封装 QSystemTrayIcon::isSystemTrayAvailable()。
    // 该结果由 Main.cpp 传给窗口生命周期控制器。
    [[nodiscard]] bool IsAvailable() const;

    // 显示 / 隐藏托盘图标。退出前应主动 Hide，避免残留无效图标。
    void Show();
    void Hide();

signals:
    // 用户单击 / 双击托盘图标，请求恢复主窗口
    void RestoreRequested();

    // 用户选择 Exit 菜单项，请求退出应用
    void ExitRequested();

private:
    QSystemTrayIcon* TrayIcon = nullptr;
    QMenu* Menu = nullptr;
    QAction* ExitAction = nullptr;
};

}  // namespace MappyZ
