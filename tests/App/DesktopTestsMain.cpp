// MappyZDesktopTests 自定义入口。
// 窗口生命周期测试需要真实 QWindow 和 GUI 事件分发，使用 offscreen 平台的
// QGuiApplication 提供最小 GUI 环境，不构造 Widgets 托盘对象。

#include <QGuiApplication>

#include <catch2/catch_session.hpp>

int main(int ArgCount, char* Arguments[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication App(ArgCount, Arguments);
    return Catch::Session().run(ArgCount, Arguments);
}
