// ZSystemTrayController 单元测试。
// offscreen 下只检查 QMenu / QAction 对象树，不要求系统托盘可用，也不展示原生菜单。
// 验证 profile action 的顺序、文本、data、互斥勾选状态，以及 separator / Exit 的位置，
// 触发行为和两次 SetProfiles 之间的重建语义。
//
// 通过 QSystemTrayIcon::contextMenu() 读取菜单对象树；不依赖 Qt6::Test 的 QSignalSpy，
// 信号次数用手动 connect 计数。

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>
#include <QVariantList>
#include <QVariantMap>

#include "App/SystemTrayController.h"

using namespace MappyZ;

namespace
{

// 构造一个 { id, name } 配置项。
QVariantMap MakeEntry(const QString& Id, const QString& Name)
{
    QVariantMap Entry;
    Entry.insert(QStringLiteral("id"), Id);
    Entry.insert(QStringLiteral("name"), Name);
    return Entry;
}

// 读取托盘控制器持有的右键菜单：QSystemTrayIcon 以控制器为父对象，可直接查找。
QMenu* MenuOf(ZSystemTrayController& Controller)
{
    QSystemTrayIcon* Icon = Controller.findChild<QSystemTrayIcon*>();
    REQUIRE(Icon != nullptr);
    return Icon->contextMenu();
}

// 统计菜单中文本为 Exit 的 action 数量。
int CountExitActions(QMenu* Menu)
{
    int Count = 0;
    for (QAction* Action : Menu->actions())
    {
        if (Action->text() == QStringLiteral("Exit"))
        {
            ++Count;
        }
    }
    return Count;
}

}  // namespace

TEST_CASE("Empty profile list shows only Exit", "[App][Tray]")
{
    ZSystemTrayController Controller(QIcon{});
    QMenu* Menu = MenuOf(Controller);

    // 构造时只有末尾 Exit，没有 separator。
    REQUIRE(Menu->actions().size() == 1);
    REQUIRE(Menu->actions().constFirst()->text() == QStringLiteral("Exit"));

    // 显式传入空列表仍然只保留 Exit。
    Controller.SetProfiles(QVariantList{}, QString{});
    REQUIRE(Menu->actions().size() == 1);
    REQUIRE(Menu->actions().constFirst()->text() == QStringLiteral("Exit"));
}

TEST_CASE("SetProfiles builds ordered checkable actions before separator and Exit",
    "[App][Tray]")
{
    ZSystemTrayController Controller(QIcon{});
    QMenu* Menu = MenuOf(Controller);

    QVariantList Entries;
    Entries.append(MakeEntry(QStringLiteral("a"), QStringLiteral("Alpha")));
    Entries.append(MakeEntry(QStringLiteral("b"), QStringLiteral("Beta")));
    Entries.append(MakeEntry(QStringLiteral("c"), QStringLiteral("Gamma")));

    Controller.SetProfiles(Entries, QStringLiteral("b"));

    const QList<QAction*> Actions = Menu->actions();
    // 3 个 profile + 1 separator + 1 Exit
    REQUIRE(Actions.size() == 5);

    // 顺序与文本、data 与传入一致
    REQUIRE(Actions[0]->text() == QStringLiteral("Alpha"));
    REQUIRE(Actions[0]->data().toString() == QStringLiteral("a"));
    REQUIRE(Actions[1]->text() == QStringLiteral("Beta"));
    REQUIRE(Actions[1]->data().toString() == QStringLiteral("b"));
    REQUIRE(Actions[2]->text() == QStringLiteral("Gamma"));
    REQUIRE(Actions[2]->data().toString() == QStringLiteral("c"));

    // 全部可勾选，且只有当前项 b 被勾选（互斥）
    REQUIRE(Actions[0]->isCheckable());
    REQUIRE(Actions[1]->isCheckable());
    REQUIRE(Actions[2]->isCheckable());
    REQUIRE_FALSE(Actions[0]->isChecked());
    REQUIRE(Actions[1]->isChecked());
    REQUIRE_FALSE(Actions[2]->isChecked());

    // separator 位于 profile 与 Exit 之间，Exit 位于末尾
    REQUIRE(Actions[3]->isSeparator());
    REQUIRE(Actions[4]->text() == QStringLiteral("Exit"));

    // objectName 供定位
    REQUIRE(Actions[0]->objectName() == QStringLiteral("profileAction_a"));
}

TEST_CASE("Triggering a non-active action emits ProfileSwitchRequested once",
    "[App][Tray]")
{
    ZSystemTrayController Controller(QIcon{});
    QMenu* Menu = MenuOf(Controller);

    QVariantList Entries;
    Entries.append(MakeEntry(QStringLiteral("a"), QStringLiteral("Alpha")));
    Entries.append(MakeEntry(QStringLiteral("b"), QStringLiteral("Beta")));
    Controller.SetProfiles(Entries, QStringLiteral("a"));

    int EmitCount = 0;
    QString EmittedId;
    QObject::connect(&Controller,
        &ZSystemTrayController::ProfileSwitchRequested,
        &Controller,
        [&EmitCount, &EmittedId](const QString& Id)
        {
            ++EmitCount;
            EmittedId = Id;
        });

    // 触发非当前项 Beta
    Menu->actions()[1]->trigger();

    REQUIRE(EmitCount == 1);
    REQUIRE(EmittedId == QStringLiteral("b"));
}

TEST_CASE("Rebuild removes old actions, updates rename, keeps single Exit",
    "[App][Tray]")
{
    ZSystemTrayController Controller(QIcon{});
    QMenu* Menu = MenuOf(Controller);

    QVariantList First;
    First.append(MakeEntry(QStringLiteral("a"), QStringLiteral("Alpha")));
    First.append(MakeEntry(QStringLiteral("b"), QStringLiteral("Beta")));
    Controller.SetProfiles(First, QStringLiteral("a"));
    REQUIRE(Menu->actions().size() == 4);  // 2 + separator + Exit

    // 第二次：把 a 重命名为 AlphaRenamed，当前项改为 a
    QVariantList Second;
    Second.append(MakeEntry(QStringLiteral("a"), QStringLiteral("AlphaRenamed")));
    Second.append(MakeEntry(QStringLiteral("b"), QStringLiteral("Beta")));
    Controller.SetProfiles(Second, QStringLiteral("a"));

    const QList<QAction*> Actions = Menu->actions();
    REQUIRE(Actions.size() == 4);

    // 旧文本已移除，重命名已更新
    REQUIRE(Actions[0]->text() == QStringLiteral("AlphaRenamed"));
    REQUIRE(Actions[1]->text() == QStringLiteral("Beta"));
    REQUIRE(Actions[2]->isSeparator());
    REQUIRE(Actions[3]->text() == QStringLiteral("Exit"));

    // Exit 始终只有一个
    REQUIRE(CountExitActions(Menu) == 1);
}

TEST_CASE("Exit action emits ExitRequested once then disables itself",
    "[App][Tray]")
{
    ZSystemTrayController Controller(QIcon{});
    QMenu* Menu = MenuOf(Controller);

    int ExitCount = 0;
    QObject::connect(&Controller,
        &ZSystemTrayController::ExitRequested,
        &Controller,
        [&ExitCount]() { ++ExitCount; });

    QAction* Exit = Menu->actions().constLast();
    REQUIRE(Exit->text() == QStringLiteral("Exit"));

    Exit->trigger();
    REQUIRE(ExitCount == 1);
    REQUIRE_FALSE(Exit->isEnabled());

    // 已禁用后再次触发不再发信号
    Exit->trigger();
    REQUIRE(ExitCount == 1);
}
