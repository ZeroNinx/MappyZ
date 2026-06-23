#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <Qt>

#include "UI/Bridge/AppController.h"

int main(int ArgCount, char* Arguments[])
{
    QGuiApplication App(ArgCount, Arguments);

    MappyZ::ZAppController AppController;

    QQmlApplicationEngine Engine;

    Engine.rootContext()->setContextProperty("appController", &AppController);

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
