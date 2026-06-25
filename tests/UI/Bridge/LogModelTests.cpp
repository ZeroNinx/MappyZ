// ZLogModel 单元测试。
// 验证 append、capacity 上限、role 映射、clear 行为、
// 以及通过 AppController 集成的 lifecycle 日志写入。

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include <QMetaMethod>
#include <QSignalSpy>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "UI/Bridge/AppController.h"
#include "UI/Bridge/LogModel.h"

using namespace MappyZ;

// ── 构造辅助 ──

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

// ── 默认状态 ──

TEST_CASE("LogModel default is empty",
    "[UI][LogModel]")
{
    ZLogModel Model;
    REQUIRE(Model.rowCount() == 0);
}

// ── Append ──

TEST_CASE("LogModel append increases row count",
    "[UI][LogModel]")
{
    ZLogModel Model;

    QSignalSpy InsertSpy(&Model, &QAbstractItemModel::rowsInserted);

    Model.Append("Info", "test message");

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(InsertSpy.count() == 1);
}

TEST_CASE("LogModel append stores correct role data",
    "[UI][LogModel]")
{
    ZLogModel Model;
    Model.Append("Success", "binding applied");

    auto Index = Model.index(0);
    REQUIRE(Index.isValid());

    // time 应该是 HH:mm:ss.zzz 格式（12 字符）
    auto Time = Model.data(Index, ZLogModel::TimeRole).toString();
    REQUIRE(Time.length() == 12);

    auto Level = Model.data(Index, ZLogModel::LevelRole).toString();
    REQUIRE(Level == "Success");

    auto Message = Model.data(Index, ZLogModel::MessageRole).toString();
    REQUIRE(Message == "binding applied");
}

// ── Capacity ──

TEST_CASE("LogModel capacity caps at 200 entries",
    "[UI][LogModel]")
{
    ZLogModel Model;

    for (int i = 0; i < 210; ++i)
    {
        Model.Append("Info", QString("message %1").arg(i));
    }

    REQUIRE(Model.rowCount() == ZLogModel::MaxCapacity);

    // 最旧的 10 条被移除，第一条应该是 "message 10"
    auto FirstMessage = Model.data(
        Model.index(0), ZLogModel::MessageRole).toString();
    REQUIRE(FirstMessage == "message 10");

    // 最后一条是 "message 209"
    auto LastMessage = Model.data(
        Model.index(199), ZLogModel::MessageRole).toString();
    REQUIRE(LastMessage == "message 209");
}

// ── Clear ──

TEST_CASE("LogModel clear empties the model",
    "[UI][LogModel]")
{
    ZLogModel Model;
    Model.Append("Info", "first");
    Model.Append("Info", "second");
    REQUIRE(Model.rowCount() == 2);

    Model.Clear();
    REQUIRE(Model.rowCount() == 0);
}

TEST_CASE("LogModel clear is idempotent",
    "[UI][LogModel]")
{
    ZLogModel Model;
    Model.Clear();
    Model.Clear();
    REQUIRE(Model.rowCount() == 0);
}

// ── 越界和未知 role ──

TEST_CASE("LogModel data returns invalid for out of range index",
    "[UI][LogModel]")
{
    ZLogModel Model;
    Model.Append("Info", "test");

    auto Result = Model.data(Model.index(5), ZLogModel::LevelRole);
    REQUIRE_FALSE(Result.isValid());
}

TEST_CASE("LogModel data returns invalid for unknown role",
    "[UI][LogModel]")
{
    ZLogModel Model;
    Model.Append("Info", "test");

    auto Result = Model.data(Model.index(0), Qt::UserRole + 999);
    REQUIRE_FALSE(Result.isValid());
}

// ── roleNames ──

TEST_CASE("LogModel roleNames contains time level message",
    "[UI][LogModel]")
{
    ZLogModel Model;
    auto Roles = Model.roleNames();

    REQUIRE(Roles.contains(ZLogModel::TimeRole));
    REQUIRE(Roles.contains(ZLogModel::LevelRole));
    REQUIRE(Roles.contains(ZLogModel::MessageRole));

    REQUIRE(Roles[ZLogModel::TimeRole] == "time");
    REQUIRE(Roles[ZLogModel::LevelRole] == "level");
    REQUIRE(Roles[ZLogModel::MessageRole] == "message");
}

// ── QML 不可直接调用 Append/Clear ──

TEST_CASE("LogModel Append and Clear are not Q_INVOKABLE",
    "[UI][LogModel]")
{
    const auto* Meta = &ZLogModel::staticMetaObject;
    for (int i = 0; i < Meta->methodCount(); ++i)
    {
        auto Method = Meta->method(i);
        if (Method.methodType() == QMetaMethod::Slot
            || Method.methodType() == QMetaMethod::Method)
        {
            auto Name = QString::fromLatin1(Method.name());
            REQUIRE(Name != "Append");
            REQUIRE(Name != "Clear");
        }
    }
}

// ── AppController 集成：runtimeError 同时写 log ──

TEST_CASE("AppController runtimeError also writes Error log entry",
    "[UI][LogModel]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    auto* Log = Controller.LogModel();

    // 未 initialize 时调用 startRuntime 触发 runtimeError
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    (void)Controller.startRuntime();

    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Log->rowCount() >= 1);

    // 查找 Error 级别日志
    bool bFoundError = false;
    for (int i = 0; i < Log->rowCount(); ++i)
    {
        auto Level = Log->data(Log->index(i), ZLogModel::LevelRole).toString();
        if (Level == "Error")
        {
            bFoundError = true;
            break;
        }
    }
    REQUIRE(bFoundError);
}

// ── AppController 集成：Apply 成功写 Success log ──

TEST_CASE("AppController apply success writes Success log entry",
    "[UI][LogModel]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    auto* Log = Controller.LogModel();

    int CountBefore = Log->rowCount();
    bool bResult = Controller.applySelectedBinding("button_south", "Keyboard", "Space");

    REQUIRE(bResult);
    REQUIRE(Log->rowCount() > CountBefore);

    // 最后一条应为 Success 级别
    auto LastLevel = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    REQUIRE(LastLevel == "Success");
}

// ── AppController 集成：Apply 失败写 Error log ──

TEST_CASE("AppController apply failure writes Error log entry",
    "[UI][LogModel]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    auto* Log = Controller.LogModel();

    int CountBefore = Log->rowCount();
    bool bResult = Controller.applySelectedBinding("", "Keyboard", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(Log->rowCount() > CountBefore);

    // 最后一条应为 Error 级别
    auto LastLevel = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    REQUIRE(LastLevel == "Error");
}

// ── AppController 集成：saveActiveProfile 写 Success log ──

TEST_CASE("AppController saveActiveProfile success writes Success log",
    "[UI][LogModel]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto* Log = Controller.LogModel();

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_log_save_test" / "profile.json";
    Controller.saveActiveProfile(QString::fromStdString(TempPath.string()));

    auto LastLevel = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    REQUIRE(LastLevel == "Success");

    auto LastMessage = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::MessageRole).toString();
    REQUIRE(LastMessage.contains("Profile saved"));

    std::filesystem::remove_all(TempPath.parent_path());
}
