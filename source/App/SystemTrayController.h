// 系统托盘适配器。
// 只负责真实托盘资源（QSystemTrayIcon、QMenu、唯一的 Exit QAction）与用户命令，
// 依赖 Qt Widgets。不了解窗口策略、Runtime、Profile 或映射逻辑，只通过信号对外表达
// “请求恢复窗口”和“请求退出”两种语义。

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

class QIcon;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QActionGroup;

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

    // 用传入的配置列表重建菜单：删除上一批 profile action 与 separator，
    // 在 Exit 之前按传入顺序插入新 action，当前项打勾。空列表只保留 Exit。
    // 只渲染数据、只发出 ProfileSwitchRequested，不读取文件或访问 Runtime。
    void SetProfiles(
        const QVariantList& ProfileEntries,
        const QString& ActiveProfileId);

signals:
    // 用户单击 / 双击托盘图标，请求恢复主窗口
    void RestoreRequested();

    // 用户选择 Exit 菜单项，请求退出应用
    void ExitRequested();

    // 用户点击某个 profile 菜单项，携带该项的稳定 ID
    void ProfileSwitchRequested(QString ProfileId);

private:
    // 创建末尾唯一的 Exit action 并接好“首次触发即禁用并发信号”逻辑。
    void BuildExitAction();

    QSystemTrayIcon* TrayIcon = nullptr;
    QMenu* Menu = nullptr;
    QAction* ExitAction = nullptr;

    // profile 菜单项与其互斥分组；重建时整体销毁再重建。
    QActionGroup* ProfileActionGroup = nullptr;
    QList<QAction*> ProfileActions;
    QAction* SeparatorAction = nullptr;
};

}  // namespace MappyZ
