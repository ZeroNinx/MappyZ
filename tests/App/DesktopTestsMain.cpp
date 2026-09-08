// MappyZDesktopTests 自定义入口。
// 窗口生命周期测试需要真实 QWindow 和 GUI 事件分发；托盘测试需要 QMenu / QAction
// 等 Widgets 对象。二者统一使用 offscreen 平台的 QApplication 提供最小 GUI 环境。

#include <QApplication>

#include <catch2/catch_session.hpp>

int main(int ArgCount, char* Arguments[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication App(ArgCount, Arguments);
    return Catch::Session().run(ArgCount, Arguments);
}
