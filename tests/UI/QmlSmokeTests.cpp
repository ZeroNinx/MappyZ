// QML 离屏冒烟测试。
// 使用 offscreen 平台加载 MappyZUI 模块，验证组件树创建成功且无警告。
// QGuiApplication + offscreen 提供最小 GUI 环境，不需要真实显示器。

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlextensionplugin.h>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "UI/Bridge/AppController.h"

// 静态链接 QML 模块时必须显式导入插件
Q_IMPORT_QML_PLUGIN(MappyZUIPlugin)

using namespace MappyZ;

// 全局警告收集器，在 QML 加载期间只捕获 QML 相关警告
static QStringList GCollectedWarnings;

static bool IsQmlRelatedWarning(
    const QMessageLogContext& Context, const QString& Message)
{
    const char* Category = Context.category ? Context.category : "";

    // QML 引擎分类的警告（binding/import/property 等）
    if (std::strncmp(Category, "qml", 3) == 0
        || std::strncmp(Category, "qt.qml", 6) == 0)
    {
        return true;
    }

    // JavaScript 运行时错误（ReferenceError、TypeError 等）
    if (Message.contains(QLatin1String("ReferenceError"))
        || Message.contains(QLatin1String("TypeError")))
    {
        return true;
    }

    return false;
}

static void QmlWarningHandler(
    QtMsgType Type,
    const QMessageLogContext& Context,
    const QString& Message)
{
    if (Type == QtWarningMsg && IsQmlRelatedWarning(Context, Message))
    {
        GCollectedWarnings.append(Message);
    }
}

// 测试用工厂，与 AppControllerTests 一致
static TInputBackendFactory MakeFakeInputFactory()
{
    return []() -> TResult<TUniquePtr<IInputBackend>> {
        return TResult<TUniquePtr<IInputBackend>>::Ok(
            std::make_unique<ZFakeInputBackend>());
    };
}

static TOutputBackendFactory MakeNullOutputFactory()
{
    return []() -> TResult<TUniquePtr<IOutputBackend>> {
        return TResult<TUniquePtr<IOutputBackend>>::Ok(
            std::make_unique<ZNullOutputBackend>());
    };
}

TEST_CASE("QML module loads without warnings", "[UI][QmlSmoke]")
{
    GCollectedWarnings.clear();
    QtMessageHandler PreviousHandler = qInstallMessageHandler(QmlWarningHandler);

    // 注入 fake/null 后端，确保 initializeRuntime 可成功
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);

    bool bCreationFailed = false;

    QObject::connect(
        &Engine,
        &QQmlApplicationEngine::objectCreationFailed,
        [&bCreationFailed]() { bCreationFailed = true; });

    Engine.loadFromModule("MappyZUI", "Main");

    // 处理挂起事件，确保 QML 组件完成创建
    QCoreApplication::processEvents();

    qInstallMessageHandler(PreviousHandler);

    REQUIRE_FALSE(bCreationFailed);

    if (!GCollectedWarnings.isEmpty())
    {
        for (const auto& Warning : GCollectedWarnings)
        {
            WARN(Warning.toStdString());
        }

        FAIL("QML module produced "
            + std::to_string(GCollectedWarnings.size()) + " warning(s)");
    }
}

int main(int ArgCount, char* Arguments[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication App(ArgCount, Arguments);
    return Catch::Session().run(ArgCount, Arguments);
}
