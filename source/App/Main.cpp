#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <Qt>
#include <QtQml/qqmlextensionplugin.h>

#include "UI/Bridge/AppController.h"

// QML 模块提取为独立 STATIC 库后，需要显式导入插件
Q_IMPORT_QML_PLUGIN(MappyZUIPlugin)

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
