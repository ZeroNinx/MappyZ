// QML 离屏冒烟测试。
// 使用 offscreen 平台加载 MappyZUI 模块，验证组件树创建成功且无警告。
// QGuiApplication + offscreen 提供最小 GUI 环境，不需要真实显示器。
// 使用 QTemporaryDir 注入隔离配置目录，避免污染真实 AppData 配置。

#include <QGuiApplication>
#include <QKeyEvent>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
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
#include "App/ForegroundApplicationService.h"
#include "App/SettingsManager.h"
#include "UI/Bridge/AppController.h"
#include "UI/Bridge/AutoProfileRuleModel.h"

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

// 接口与 ZSettingsManager 一致的测试替身：精确计数 setter 调用次数，并可按需
// 模拟写入失败，用于验证 CheckBox 只调用一次 setter 及失败后的视觉回滚。
// todo 明确允许 QML 测试注入接口一致的测试对象。
class ZSettingsManagerSpy final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool startMinimized READ IsStartMinimized NOTIFY startMinimizedChanged)

public:
    explicit ZSettingsManagerSpy(QObject* Parent = nullptr) : QObject(Parent) {}

    bool IsStartMinimized() const { return bValue; }

    Q_INVOKABLE bool setStartMinimized(bool bEnabled)
    {
        ++SetterCallCount;
        if (bFailNextWrite)
        {
            emit settingsError(QStringLiteral("simulated failure"));
            return false;
        }
        if (bEnabled == bValue)
        {
            return true;
        }
        bValue = bEnabled;
        emit startMinimizedChanged();
        return true;
    }

    int SetterCallCount = 0;
    bool bFailNextWrite = false;
    bool bValue = true;

signals:
    void startMinimizedChanged();
    void settingsError(const QString& Message);
};

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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);

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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
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

TEST_CASE("Settings button drives the full open chain and checkbox reflects manager",
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();

    // Settings 按钮位于 ProfileSelector 内，且即使单 profile（Delete 禁用）仍可用。
    QObject* SettingsButton =
        RootObject->findChild<QObject*>(QStringLiteral("profileSettingsButton"));
    REQUIRE(SettingsButton != nullptr);
    REQUIRE(SettingsButton->property("enabled").toBool());

    QObject* DeleteButton =
        RootObject->findChild<QObject*>(QStringLiteral("profileDeleteButton"));
    REQUIRE(DeleteButton != nullptr);
    REQUIRE_FALSE(DeleteButton->property("enabled").toBool());

    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    REQUIRE_FALSE(Dialog->property("visible").toBool());

    // 点击 Settings 按钮：驱动真实信号链 ProfileSelector.settingsRequested →
    // TopBar 转发 → Main.qml 打开 SettingsDialog，默认选中 General。
    REQUIRE(QMetaObject::invokeMethod(SettingsButton, "clicked"));
    QCoreApplication::processEvents();
    REQUIRE(Dialog->property("visible").toBool());
    REQUIRE(Dialog->property("currentCategory").toString()
        == QStringLiteral("general"));

    // Start minimized 复选框初始反映管理器默认值 true（Qt.Checked）。
    QObject* CheckBox =
        Dialog->findChild<QObject*>(QStringLiteral("startMinimizedCheckBox"));
    REQUIRE(CheckBox != nullptr);
    REQUIRE(CheckBox->property("checkState").toInt() == static_cast<int>(Qt::Checked));

    // 管理器状态改变后，绑定的复选框随之更新，二者保持一致。
    REQUIRE(SettingsManager.setStartMinimized(false));
    QCoreApplication::processEvents();
    REQUIRE(CheckBox->property("checkState").toInt() == static_cast<int>(Qt::Unchecked));

    // 关闭对话框：不再可见。
    QMetaObject::invokeMethod(Dialog, "close");
    QCoreApplication::processEvents();
    REQUIRE_FALSE(Dialog->property("visible").toBool());

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Checkbox click calls setter once and rolls back on failure",
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

    // 注入接口一致的 spy，精确计数 setter 调用并模拟写失败。
    ZSettingsManagerSpy SettingsSpy;

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsSpy);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();
    QQuickWindow* Window = qobject_cast<QQuickWindow*>(RootObject);
    REQUIRE(Window != nullptr);
    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    QCoreApplication::processEvents();

    QObject* CheckBox =
        Dialog->findChild<QObject*>(QStringLiteral("startMinimizedCheckBox"));
    REQUIRE(CheckBox != nullptr);
    // spy 默认 true → Checked。
    REQUIRE(CheckBox->property("checkState").toInt() == static_cast<int>(Qt::Checked));

    // 让复选框获得键盘焦点，随后用空格键模拟真实用户切换：空格会走 nextCheckState
    // 回调（点击 / 空格才触发，toggle() 不会），从而真正调用一次 setter。
    REQUIRE(QMetaObject::invokeMethod(CheckBox, "forceActiveFocus"));
    QCoreApplication::processEvents();

    // 成功路径：一次空格只调用一次 setter，值翻转为 false，复选框随之 Unchecked。
    {
        QKeyEvent SpacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent SpaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(Window, &SpacePress);
        QCoreApplication::sendEvent(Window, &SpaceRelease);
    }
    QCoreApplication::processEvents();
    REQUIRE(SettingsSpy.SetterCallCount == 1);
    REQUIRE_FALSE(SettingsSpy.bValue);
    REQUIRE(CheckBox->property("checkState").toInt() == static_cast<int>(Qt::Unchecked));

    // 失败路径：下一次写入失败，一次空格仍只调用一次 setter，管理器值不变，
    // 复选框视觉回滚到管理器权威值（仍 Unchecked）。
    SettingsSpy.bFailNextWrite = true;
    {
        QKeyEvent SpacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent SpaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(Window, &SpacePress);
        QCoreApplication::sendEvent(Window, &SpaceRelease);
    }
    QCoreApplication::processEvents();
    REQUIRE(SettingsSpy.SetterCallCount == 2);
    REQUIRE_FALSE(SettingsSpy.bValue);
    REQUIRE(CheckBox->property("checkState").toInt() == static_cast<int>(Qt::Unchecked));

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Escape closes the settings dialog without modifying settings",
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

    ZSettingsManagerSpy SettingsSpy;

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsSpy);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();
    QQuickWindow* Window = qobject_cast<QQuickWindow*>(RootObject);
    REQUIRE(Window != nullptr);

    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    QCoreApplication::processEvents();
    REQUIRE(Dialog->property("visible").toBool());

    // 向窗口投递物理 Escape：对话框持有焦点，应关闭且不修改设置。
    QKeyEvent PressEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(Window, &PressEvent);
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Dialog->property("visible").toBool());
    REQUIRE(SettingsSpy.SetterCallCount == 0);

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Automatic profile switching page drives rule add through the real UI chain",
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    // 注入带预置运行应用列表的 fake 前台服务，驱动进程选择器。
    auto FakeSource = std::make_unique<ZFakeForegroundApplicationSource>();
    SForegroundWindowInfo GameApp;
    GameApp.ProcessName = QStringLiteral("game.exe");
    GameApp.DisplayName = QStringLiteral("Game Window");
    SForegroundWindowInfo EditorApp;
    EditorApp.ProcessName = QStringLiteral("editor.exe");
    EditorApp.DisplayName = QStringLiteral("Editor");
    FakeSource->SetApplications({GameApp, EditorApp});
    ZForegroundApplicationService ForegroundService(std::move(FakeSource));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
    Engine.rootContext()->setContextProperty(
        "foregroundApplicationService", &ForegroundService);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();

    // 打开设置对话框并切到 Automatic Switching 分类。
    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    Dialog->setProperty("currentCategory", QStringLiteral("automatic"));
    QCoreApplication::processEvents();

    QObject* AutomaticPage =
        RootObject->findChild<QObject*>(QStringLiteral("settingsAutomaticPage"));
    REQUIRE(AutomaticPage != nullptr);
    REQUIRE(AutomaticPage->property("visible").toBool());

    // 初始无规则。
    REQUIRE(Controller.AutoProfileRuleModel()->rowCount() == 0);

    // 打开添加对话框，再打开进程选择器：列表应含两条运行应用。
    QObject* AddDialog =
        RootObject->findChild<QObject*>(QStringLiteral("automaticAddRuleDialog"));
    REQUIRE(AddDialog != nullptr);
    QMetaObject::invokeMethod(AddDialog, "open");
    QCoreApplication::processEvents();
    REQUIRE(AddDialog->property("visible").toBool());

    QObject* Picker =
        RootObject->findChild<QObject*>(QStringLiteral("processPickerDialog"));
    REQUIRE(Picker != nullptr);
    QMetaObject::invokeMethod(Picker, "open");
    QCoreApplication::processEvents();

    QObject* PickerList =
        RootObject->findChild<QObject*>(QStringLiteral("processPickerList"));
    REQUIRE(PickerList != nullptr);
    REQUIRE(PickerList->property("count").toInt() == 2);
    QMetaObject::invokeMethod(Picker, "close");
    QCoreApplication::processEvents();

    // 通过真实提交链新增一条规则：填入进程名后调用 submit()（默认选中 Default）。
    QObject* ProcessField =
        RootObject->findChild<QObject*>(QStringLiteral("addRuleProcessField"));
    REQUIRE(ProcessField != nullptr);
    ProcessField->setProperty("text", QStringLiteral("game.exe"));
    QMetaObject::invokeMethod(AddDialog, "submit");
    QCoreApplication::processEvents();

    // 提交成功：对话框关闭，规则模型多出一行。
    REQUIRE_FALSE(AddDialog->property("visible").toBool());
    REQUIRE(Controller.AutoProfileRuleModel()->rowCount() == 1);

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Escape closes the three automatic-switching overlays one layer at a time",
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    // 注入带一条运行应用的 fake 前台服务，使进程选择器列表非空。
    auto FakeSource = std::make_unique<ZFakeForegroundApplicationSource>();
    SForegroundWindowInfo GameApp;
    GameApp.ProcessName = QStringLiteral("game.exe");
    GameApp.DisplayName = QStringLiteral("Game Window");
    FakeSource->SetApplications({GameApp});
    ZForegroundApplicationService ForegroundService(std::move(FakeSource));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
    Engine.rootContext()->setContextProperty(
        "foregroundApplicationService", &ForegroundService);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();
    QQuickWindow* Window = qobject_cast<QQuickWindow*>(RootObject);
    REQUIRE(Window != nullptr);

    // 打开三层 overlay：Settings → Add rule → Process picker。
    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    Dialog->setProperty("currentCategory", QStringLiteral("automatic"));
    QCoreApplication::processEvents();

    QObject* AddDialog =
        RootObject->findChild<QObject*>(QStringLiteral("automaticAddRuleDialog"));
    REQUIRE(AddDialog != nullptr);
    QMetaObject::invokeMethod(AddDialog, "open");
    QCoreApplication::processEvents();

    QObject* Picker =
        RootObject->findChild<QObject*>(QStringLiteral("processPickerDialog"));
    REQUIRE(Picker != nullptr);
    QMetaObject::invokeMethod(Picker, "open");
    QCoreApplication::processEvents();

    REQUIRE(Dialog->property("visible").toBool());
    REQUIRE(AddDialog->property("visible").toBool());
    REQUIRE(Picker->property("visible").toBool());

    auto SendEscape = [Window]() {
        QKeyEvent Press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(Window, &Press);
        QCoreApplication::processEvents();
    };

    // 第一次 Escape：只关闭最上层的进程选择器。
    SendEscape();
    REQUIRE_FALSE(Picker->property("visible").toBool());
    REQUIRE(AddDialog->property("visible").toBool());
    REQUIRE(Dialog->property("visible").toBool());

    // 第二次 Escape：焦点已交还，关闭 Add 对话框。
    SendEscape();
    REQUIRE_FALSE(AddDialog->property("visible").toBool());
    REQUIRE(Dialog->property("visible").toBool());

    // 第三次 Escape：关闭 Settings 对话框。
    SendEscape();
    REQUIRE_FALSE(Dialog->property("visible").toBool());

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Adding a duplicate process keeps the add dialog open and adds no row",
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();

    // 预置一条 game.exe 规则（切到 Default）。
    REQUIRE(Controller.addAutomaticProfileRule(
        QStringLiteral("game.exe"), QStringLiteral("default")));
    REQUIRE(Controller.AutoProfileRuleModel()->rowCount() == 1);

    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    Dialog->setProperty("currentCategory", QStringLiteral("automatic"));
    QCoreApplication::processEvents();

    QObject* AddDialog =
        RootObject->findChild<QObject*>(QStringLiteral("automaticAddRuleDialog"));
    REQUIRE(AddDialog != nullptr);
    QMetaObject::invokeMethod(AddDialog, "open");
    QCoreApplication::processEvents();

    // 再次提交相同进程名：C++ 拒绝重复，dialog 保持打开、规则不增加。
    QObject* ProcessField =
        RootObject->findChild<QObject*>(QStringLiteral("addRuleProcessField"));
    REQUIRE(ProcessField != nullptr);
    ProcessField->setProperty("text", QStringLiteral("game.exe"));
    QMetaObject::invokeMethod(AddDialog, "submit");
    QCoreApplication::processEvents();

    REQUIRE(AddDialog->property("visible").toBool());
    REQUIRE(Controller.AutoProfileRuleModel()->rowCount() == 1);

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

TEST_CASE("Delete removes the rule matching the selected ruleId regardless of row",
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

    ZSettingsManager SettingsManager(
        TempDir.path() + QStringLiteral("/settings.ini"));

    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("appController", &Controller);
    Engine.rootContext()->setContextProperty("settingsManager", &SettingsManager);
    Engine.loadFromModule("MappyZUI", "Main");
    QCoreApplication::processEvents();

    REQUIRE_FALSE(Engine.rootObjects().isEmpty());
    QObject* RootObject = Engine.rootObjects().constFirst();

    // 预置两条规则。
    REQUIRE(Controller.addAutomaticProfileRule(
        QStringLiteral("first.exe"), QStringLiteral("default")));
    REQUIRE(Controller.addAutomaticProfileRule(
        QStringLiteral("second.exe"), QStringLiteral("default")));
    ZAutoProfileRuleModel* Model = Controller.AutoProfileRuleModel();
    REQUIRE(Model->rowCount() == 2);

    // 取第二行的权威 ruleId。
    QString SecondRuleId;
    REQUIRE(QMetaObject::invokeMethod(
        Model, "ruleIdAt", Q_RETURN_ARG(QString, SecondRuleId), Q_ARG(int, 1)));
    REQUIRE_FALSE(SecondRuleId.isEmpty());

    QObject* Dialog =
        RootObject->findChild<QObject*>(QStringLiteral("settingsDialog"));
    REQUIRE(Dialog != nullptr);
    QMetaObject::invokeMethod(Dialog, "open");
    Dialog->setProperty("currentCategory", QStringLiteral("automatic"));
    QCoreApplication::processEvents();

    // 选中第二行并点击 Delete：应删除 second.exe，保留 first.exe。
    QObject* AutomaticPage =
        RootObject->findChild<QObject*>(QStringLiteral("settingsAutomaticPage"));
    REQUIRE(AutomaticPage != nullptr);
    AutomaticPage->setProperty("selectedRuleId", SecondRuleId);
    QCoreApplication::processEvents();

    QObject* DeleteButton =
        RootObject->findChild<QObject*>(QStringLiteral("automaticDeleteRuleButton"));
    REQUIRE(DeleteButton != nullptr);
    REQUIRE(DeleteButton->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(DeleteButton, "clicked"));
    QCoreApplication::processEvents();

    REQUIRE(Model->rowCount() == 1);
    QString RemainingProcess =
        Model->data(Model->index(0, 0),
            ZAutoProfileRuleModel::ProcessNameRole).toString();
    REQUIRE(RemainingProcess == QStringLiteral("first.exe"));

    qInstallMessageHandler(PreviousHandler);
    RequireNoWarnings();
}

int main(int ArgCount, char* Arguments[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication App(ArgCount, Arguments);
    return Catch::Session().run(ArgCount, Arguments);
}

// .cpp 内定义的 Q_OBJECT（ZSettingsManagerSpy）需显式包含 AUTOMOC 生成的 moc。
#include "QmlSmokeTests.moc"
