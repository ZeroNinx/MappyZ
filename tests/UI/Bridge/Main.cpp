// MappyZUIBridgeTests 自定义入口。
// QSignalSpy 和 QTimer 需要进程中已有 QCoreApplication 实例，
// 因此不使用 Catch2::Catch2WithMain，而是手动创建 QCoreApplication 后启动 Catch2。

#include <QCoreApplication>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QCoreApplication App(argc, argv);
    return Catch::Session().run(argc, argv);
}
