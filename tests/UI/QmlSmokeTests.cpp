// QML 离屏冒烟测试。
// 使用 offscreen 平台加载 MappyZUI 模块，验证组件树创建成功且无警告。
// QGuiApplication + offscreen 提供最小 GUI 环境，不需要真实显示器。
// 使用 QTemporaryDir 注入隔离配置目录，避免污染真实 AppData 配置。

#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
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

// 断言当前未收集到任何 QML 警告，并把每条警告输出到测试报告。
static void RequireNoWarnings()
{
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

TEST_CASE("QML module loads without warnings", "[UI][QmlSmoke]")
{
    GCollectedWarnings.clear();
    QtMessageHandler PreviousHandler = qInstallMessageHandler(QmlWarningHandler);

    // 隔离临时配置目录，避免污染真实 AppData
    QTemporaryDir TempDir;
    REQUIRE(TempDir.isValid());

    // 注入 fake/null 后端与隔离目录，确保 initializeRuntime/initializeProfiles 可成功
    ZAppController Controller(
        MakeFakeInputFactory(),
        MakeNullOutputFactory(),
        StdPath(TempDir.path().toStdString()));

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
    RequireNoWarnings();
}

TEST_CASE("ProfileSelector renders and toggles rename state", "[UI][QmlSmoke]")
{
    GCollectedWarnings.clear();
    QtMessageHandler PreviousHandler = qInstallMessageHandler(QmlWarningHandler);

    QTemporaryDir TempDir;
    REQUIRE(TempDir.isValid());

    ZAppController Controller(
        MakeFakeInputFactory(),
        MakeNullOutputFactory(),
        StdPath(TempDir.path().toStdString()));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();

    // 通过根对象查找 ProfileSelector
    QObject* Selector = RootObject->findChild<QObject*>(QStringLiteral("profileSelector"));
    REQUIRE(Selector != nullptr);

    // 初始化成功后默认配置名为 Default
    REQUIRE(Controller.ActiveProfileName() == QStringLiteral("Default"));

    // 三个操作按钮均可创建
    REQUIRE(Selector->findChild<QObject*>(QStringLiteral("profileCreateButton")) != nullptr);
    REQUIRE(Selector->findChild<QObject*>(QStringLiteral("profileRenameButton")) != nullptr);
    REQUIRE(Selector->findChild<QObject*>(QStringLiteral("profileDeleteButton")) != nullptr);

    // 下拉列表 delegate 绑定到 controller 的 profileEntries，count 与列表一致
    QObject* ListViewObject =
        Selector->findChild<QObject*>(QStringLiteral("profileListView"));
    REQUIRE(ListViewObject != nullptr);
    REQUIRE(ListViewObject->property("count").toInt()
        == Controller.ProfileEntries().size());

    // 进入 rename 编辑态：renaming 变 true，输入框可见
    QMetaObject::invokeMethod(Selector, "beginRename");
    QCoreApplication::processEvents();
    REQUIRE(Selector->property("renaming").toBool());

    // 取消 rename：renaming 变 false，恢复权威名称，不调用 C++
    QMetaObject::invokeMethod(Selector, "cancelRename");
    QCoreApplication::processEvents();
    REQUIRE_FALSE(Selector->property("renaming").toBool());

    qInstallMessageHandler(PreviousHandler);

    // 进入 / 取消 rename 全程无 binding loop 或重复提交 warning
    RequireNoWarnings();
}

TEST_CASE("ProfileSelector cancels rename when active profile switches externally",
    "[UI][QmlSmoke]")
{
    GCollectedWarnings.clear();
    QtMessageHandler PreviousHandler = qInstallMessageHandler(QmlWarningHandler);

    QTemporaryDir TempDir;
    REQUIRE(TempDir.isValid());

    ZAppController Controller(
        MakeFakeInputFactory(),
        MakeNullOutputFactory(),
        StdPath(TempDir.path().toStdString()));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();
    QObject* Selector =
        RootObject->findChild<QObject*>(QStringLiteral("profileSelector"));
    REQUIRE(Selector != nullptr);

    // 进入编辑态，记录开始编辑时的配置 ID
    QMetaObject::invokeMethod(Selector, "beginRename");
    QCoreApplication::processEvents();
    REQUIRE(Selector->property("renaming").toBool());

    // 编辑期间外部切换当前配置（新建配置会把当前项切到新建的配置）：
    // activeProfileChanged 触发后应自动结束编辑，避免旧草稿改到新配置。
    REQUIRE(Controller.createProfile());
    QCoreApplication::processEvents();
    REQUIRE_FALSE(Selector->property("renaming").toBool());

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

int main(int ArgCount, char* Arguments[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication App(ArgCount, Arguments);
    return Catch::Session().run(ArgCount, Arguments);
}
