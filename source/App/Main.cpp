#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <Qt>

int main(int ArgCount, char* Arguments[])
{
    QGuiApplication App(ArgCount, Arguments);

    QQmlApplicationEngine Engine;

    QObject::connect(
        &Engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &App,
        []()
        {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    Engine.loadFromModule("MappyZUI", "Main");

    return App.exec();
}
