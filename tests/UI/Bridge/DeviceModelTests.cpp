// ZDeviceModel 和 ZAppController deviceModel 集成测试。
// 验证设备列表模型的增删改查、QML 信号发射、去重逻辑，
// 以及通过 AppController + FakeInputBackend + RuntimeEventPump 的热插拔集成。

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "UI/Bridge/AppController.h"
#include "UI/Bridge/DeviceModel.h"

using namespace MappyZ;

// ── 测试用 factory 辅助 ──

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

// ── 构造辅助 ──

static SDeviceInfo MakeDeviceInfo(
    const StdString& Id,
    const StdString& Name = "",
    const StdString& Backend = "fake")
{
    SDeviceInfo Info;
    Info.Id.Value = Id;
    Info.Name = Name;
    Info.Backend = Backend;
    Info.VendorId = "045e";
    Info.ProductId = "02fd";
    Info.Guid = "test-guid-" + Id;
    Info.InstanceId = "inst-" + Id;
    return Info;
}

// ══════════════════════════════════════════════════════════════
// ZDeviceModel 单元测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("DeviceModel default rowCount is 0", "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    REQUIRE(Model.rowCount() == 0);
}

TEST_CASE("DeviceModel ReplaceDevices fills all roles", "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    SDeviceInfo Info = MakeDeviceInfo("dev_1", "Xbox Controller");

    Model.ReplaceDevices({Info});

    REQUIRE(Model.rowCount() == 1);

    QModelIndex Index = Model.index(0);

    REQUIRE(Model.data(Index, ZDeviceModel::DeviceIdRole).toString() == "dev_1");
    REQUIRE(Model.data(Index, ZDeviceModel::NameRole).toString() == "Xbox Controller");
    REQUIRE(Model.data(Index, ZDeviceModel::BackendRole).toString() == "fake");
    REQUIRE(Model.data(Index, ZDeviceModel::VendorIdRole).toString() == "045e");
    REQUIRE(Model.data(Index, ZDeviceModel::ProductIdRole).toString() == "02fd");
    REQUIRE(Model.data(Index, ZDeviceModel::GuidRole).toString() == "test-guid-dev_1");
    REQUIRE(Model.data(Index, ZDeviceModel::InstanceIdRole).toString() == "inst-dev_1");
    REQUIRE(Model.data(Index, ZDeviceModel::DisplayNameRole).toString() == "Xbox Controller");
}

TEST_CASE("DeviceModel DisplayNameRole falls back to DeviceId when Name is empty",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    // Name 为空的设备
    Model.ReplaceDevices({MakeDeviceInfo("dev_no_name", "")});

    QModelIndex Index = Model.index(0);
    REQUIRE(Model.data(Index, ZDeviceModel::DisplayNameRole).toString() == "dev_no_name");
}

TEST_CASE("DeviceModel ReplaceDevices deduplicates by DeviceId keeping last",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    SDeviceInfo First = MakeDeviceInfo("dup_id", "First");
    SDeviceInfo Second = MakeDeviceInfo("dup_id", "Second");

    Model.ReplaceDevices({First, Second});

    REQUIRE(Model.rowCount() == 1);

    QModelIndex Index = Model.index(0);
    REQUIRE(Model.data(Index, ZDeviceModel::NameRole).toString() == "Second");
}

TEST_CASE("DeviceModel AddOrUpdateDevice inserts new device with rowsInserted",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    QSignalSpy InsertSpy(&Model, &QAbstractItemModel::rowsInserted);

    Model.AddOrUpdateDevice(MakeDeviceInfo("new_dev", "New Device"));

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(InsertSpy.count() == 1);

    QModelIndex Index = Model.index(0);
    REQUIRE(Model.data(Index, ZDeviceModel::NameRole).toString() == "New Device");
}

TEST_CASE("DeviceModel AddOrUpdateDevice updates existing device with dataChanged",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1", "Original")});

    QSignalSpy ChangeSpy(&Model, &QAbstractItemModel::dataChanged);

    Model.AddOrUpdateDevice(MakeDeviceInfo("dev_1", "Updated"));

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(ChangeSpy.count() == 1);

    QModelIndex Index = Model.index(0);
    REQUIRE(Model.data(Index, ZDeviceModel::NameRole).toString() == "Updated");
}

TEST_CASE("DeviceModel RemoveDevice removes existing device with rowsRemoved",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1"), MakeDeviceInfo("dev_2")});
    REQUIRE(Model.rowCount() == 2);

    QSignalSpy RemoveSpy(&Model, &QAbstractItemModel::rowsRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(RemoveSpy.count() == 1);

    // 剩余设备应为 dev_2
    QModelIndex Index = Model.index(0);
    REQUIRE(Model.data(Index, ZDeviceModel::DeviceIdRole).toString() == "dev_2");
}

TEST_CASE("DeviceModel RemoveDevice for non-existent device is no-op",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    QSignalSpy RemoveSpy(&Model, &QAbstractItemModel::rowsRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "not_exist"});

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(RemoveSpy.count() == 0);
}

TEST_CASE("DeviceModel clear empties model and emits reset", "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1"), MakeDeviceInfo("dev_2")});
    REQUIRE(Model.rowCount() == 2);

    QSignalSpy ResetSpy(&Model, &QAbstractItemModel::modelReset);

    Model.clear();

    REQUIRE(Model.rowCount() == 0);
    REQUIRE(ResetSpy.count() == 1);
}

TEST_CASE("DeviceModel clear on empty model is safe no-op", "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    QSignalSpy ResetSpy(&Model, &QAbstractItemModel::modelReset);

    Model.clear();

    REQUIRE(Model.rowCount() == 0);
    REQUIRE(ResetSpy.count() == 0);
}

TEST_CASE("DeviceModel deviceIdAt returns empty string for out-of-bounds",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    REQUIRE(Model.deviceIdAt(0) == "dev_1");
    REQUIRE(Model.deviceIdAt(-1).isEmpty());
    REQUIRE(Model.deviceIdAt(1).isEmpty());
    REQUIRE(Model.deviceIdAt(999).isEmpty());
}

TEST_CASE("DeviceModel ListDevicesSnapshot returns a copy", "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    TVector<SDeviceInfo> Snapshot = Model.ListDevicesSnapshot();
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Snapshot[0].Id.Value == "dev_1");

    // 修改快照不影响 model
    Snapshot.clear();
    REQUIRE(Model.rowCount() == 1);
}

TEST_CASE("DeviceModel data returns empty QVariant for invalid index",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    // 无效 index
    QVariant Result = Model.data(QModelIndex(), ZDeviceModel::DeviceIdRole);
    REQUIRE_FALSE(Result.isValid());
}

TEST_CASE("DeviceModel data returns empty QVariant for non-zero column",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    QModelIndex Index = Model.index(0, 1);
    QVariant Result = Model.data(Index, ZDeviceModel::DeviceIdRole);

    REQUIRE_FALSE(Result.isValid());
}

TEST_CASE("DeviceModel data returns empty QVariant for unknown role",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    QModelIndex Index = Model.index(0);
    QVariant Result = Model.data(Index, Qt::UserRole + 999);
    REQUIRE_FALSE(Result.isValid());
}

TEST_CASE("DeviceModel roleNames covers all custom roles", "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    auto Roles = Model.roleNames();

    REQUIRE(Roles.contains(ZDeviceModel::DeviceIdRole));
    REQUIRE(Roles.contains(ZDeviceModel::NameRole));
    REQUIRE(Roles.contains(ZDeviceModel::BackendRole));
    REQUIRE(Roles.contains(ZDeviceModel::VendorIdRole));
    REQUIRE(Roles.contains(ZDeviceModel::ProductIdRole));
    REQUIRE(Roles.contains(ZDeviceModel::GuidRole));
    REQUIRE(Roles.contains(ZDeviceModel::InstanceIdRole));
    REQUIRE(Roles.contains(ZDeviceModel::DisplayNameRole));
}

// ══════════════════════════════════════════════════════════════
// AppController deviceModel 集成测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController deviceModel property returns non-null QObject",
    "[UI][DeviceModel]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QObject* Model = Controller.DeviceModel();
    REQUIRE(Model != nullptr);
}

TEST_CASE("AppController initialize refreshes DeviceModel from fake backend snapshot",
    "[UI][DeviceModel]")
{
    ZFakeInputBackend* RawBackend = nullptr;
    auto InputFactory = [&RawBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());

    // 在 initialize 前通过 factory 捕获的 backend 添加设备
    // （factory 在 initialize 内调用，所以先 initialize 再 AddDevice 到快照）
    (void)Controller.initializeRuntime(true);
    REQUIRE(RawBackend != nullptr);

    // FakeInputBackend 在 initialize 时已创建但未 Start，
    // 手动添加设备到快照（不触发回调，因为未 running）
    RawBackend->AddDevice(MakeDeviceInfo("fake_dev_1", "Fake Pad 1"));

    // start 后会刷新快照
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    REQUIRE(Model != nullptr);
    REQUIRE(Model->rowCount() == 1);
    REQUIRE(Model->deviceIdAt(0) == "fake_dev_1");
}

TEST_CASE("AppController start then AddDevice + pumpOnce updates DeviceModel",
    "[UI][DeviceModel]")
{
    ZFakeInputBackend* RawBackend = nullptr;
    auto InputFactory = [&RawBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();
    REQUIRE(RawBackend != nullptr);

    auto* Model = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    REQUIRE(Model != nullptr);

    int InitialCount = Model->rowCount();

    // 通过 FakeInputBackend 注入热插拔设备
    RawBackend->AddDevice(MakeDeviceInfo("hotplug_dev", "Hotplug Pad"));

    // pump 将 DeviceConnected 事件从 EventQueue 分发到 DeviceModel
    Controller.pumpOnce();

    REQUIRE(Model->rowCount() == InitialCount + 1);
}

TEST_CASE("AppController RemoveDevice + pumpOnce removes row from DeviceModel",
    "[UI][DeviceModel]")
{
    ZFakeInputBackend* RawBackend = nullptr;
    auto InputFactory = [&RawBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();
    REQUIRE(RawBackend != nullptr);

    // 先添加一个设备并 pump
    RawBackend->AddDevice(MakeDeviceInfo("removable_dev", "Removable Pad"));
    Controller.pumpOnce();

    auto* Model = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    REQUIRE(Model != nullptr);

    int CountBeforeRemove = Model->rowCount();
    REQUIRE(CountBeforeRemove >= 1);

    // 移除设备并 pump
    RawBackend->RemoveDevice(SDeviceId{.Value = "removable_dev"});
    Controller.pumpOnce();

    REQUIRE(Model->rowCount() == CountBeforeRemove - 1);
}

TEST_CASE("AppController stopRuntime does not clear DeviceModel",
    "[UI][DeviceModel]")
{
    ZFakeInputBackend* RawBackend = nullptr;
    auto InputFactory = [&RawBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();
    REQUIRE(RawBackend != nullptr);

    // 添加设备并 pump 使 DeviceModel 有数据
    RawBackend->AddDevice(MakeDeviceInfo("persist_dev", "Persist Pad"));
    Controller.pumpOnce();

    auto* Model = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    REQUIRE(Model != nullptr);
    REQUIRE(Model->rowCount() >= 1);

    int CountBeforeStop = Model->rowCount();

    // 停止运行时后设备仍保留
    Controller.stopRuntime();

    REQUIRE(Model->rowCount() == CountBeforeStop);
}

TEST_CASE("AppController initializeRuntime failure does not modify DeviceModel",
    "[UI][DeviceModel]")
{
    auto FailingFactory = []() -> TResult<TUniquePtr<IInputBackend>> {
        return TResult<TUniquePtr<IInputBackend>>::Err(
            MakeError(EErrorCode::Unknown, "test failure"));
    };

    ZAppController Controller(FailingFactory, MakeNullOutputFactory());

    auto* Model = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    REQUIRE(Model != nullptr);
    REQUIRE(Model->rowCount() == 0);

    (void)Controller.initializeRuntime(true);

    // 失败后 DeviceModel 仍为空
    REQUIRE(Model->rowCount() == 0);
}

// ── header 依赖检查 ──

TEST_CASE("DeviceModel header has no SDL or Win32 dependencies",
    "[UI][DeviceModel]")
{
    // 如果 DeviceModel.h 包含了 SDL 或 Win32 头，本文件无法在纯 Qt 环境编译。
    // 能编译通过即验证通过。
    ZDeviceModel Model;
    REQUIRE(Model.rowCount() == 0);
}

TEST_CASE("DeviceModel signals use lowerCamelCase for QML Connections compatibility",
    "[UI][DeviceModel]")
{
    const QMetaObject* Meta = &ZDeviceModel::staticMetaObject;

    REQUIRE(Meta->indexOfSignal("deviceAdded(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("deviceUpdated(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("deviceRemoved(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("deviceModelReset()") >= 0);
}

// ══════════════════════════════════════════════════════════════
// ZDeviceModel 语义 signal 测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("DeviceModel AddOrUpdateDevice emits DeviceAdded for new device",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    QSignalSpy AddedSpy(&Model, &ZDeviceModel::deviceAdded);

    Model.AddOrUpdateDevice(MakeDeviceInfo("dev_1", "Pad 1"));

    REQUIRE(AddedSpy.count() == 1);
    REQUIRE(AddedSpy.takeFirst().at(0).toString() == "dev_1");
}

TEST_CASE("DeviceModel AddOrUpdateDevice emits DeviceUpdated for existing device",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1", "Original")});

    QSignalSpy UpdatedSpy(&Model, &ZDeviceModel::deviceUpdated);
    QSignalSpy AddedSpy(&Model, &ZDeviceModel::deviceAdded);

    Model.AddOrUpdateDevice(MakeDeviceInfo("dev_1", "Updated"));

    REQUIRE(UpdatedSpy.count() == 1);
    REQUIRE(UpdatedSpy.takeFirst().at(0).toString() == "dev_1");
    REQUIRE(AddedSpy.count() == 0);
}

TEST_CASE("DeviceModel RemoveDevice emits DeviceRemoved for existing device",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1"), MakeDeviceInfo("dev_2")});

    QSignalSpy RemovedSpy(&Model, &ZDeviceModel::deviceRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(RemovedSpy.count() == 1);
    REQUIRE(RemovedSpy.takeFirst().at(0).toString() == "dev_1");
}

TEST_CASE("DeviceModel RemoveDevice does not emit DeviceRemoved for non-existent device",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    QSignalSpy RemovedSpy(&Model, &ZDeviceModel::deviceRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "not_exist"});

    REQUIRE(RemovedSpy.count() == 0);
}

TEST_CASE("DeviceModel clear emits DeviceModelReset for non-empty model",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1"), MakeDeviceInfo("dev_2")});

    QSignalSpy ResetSpy(&Model, &ZDeviceModel::deviceModelReset);

    Model.clear();

    REQUIRE(ResetSpy.count() == 1);
}

TEST_CASE("DeviceModel clear does not emit DeviceModelReset for empty model",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    QSignalSpy ResetSpy(&Model, &ZDeviceModel::deviceModelReset);

    Model.clear();

    REQUIRE(ResetSpy.count() == 0);
}

TEST_CASE("DeviceModel ReplaceDevices emits DeviceModelReset",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    QSignalSpy ResetSpy(&Model, &ZDeviceModel::deviceModelReset);

    Model.ReplaceDevices({MakeDeviceInfo("dev_1")});

    REQUIRE(ResetSpy.count() == 1);
}

TEST_CASE("DeviceModel DeviceRemoved signal fires after snapshot is updated",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;
    Model.ReplaceDevices({MakeDeviceInfo("dev_1"), MakeDeviceInfo("dev_2")});

    // 在 DeviceRemoved slot 中验证快照已不包含被移除设备
    bool bSnapshotCorrect = false;
    QObject::connect(&Model, &ZDeviceModel::deviceRemoved,
        [&Model, &bSnapshotCorrect](const QString& RemovedId)
        {
            // 遍历 model，确认被移除的设备不在里面
            for (int Row = 0; Row < Model.rowCount(); ++Row)
            {
                if (Model.deviceIdAt(Row) == RemovedId)
                {
                    bSnapshotCorrect = false;
                    return;
                }
            }
            bSnapshotCorrect = true;
        });

    Model.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(bSnapshotCorrect);
}

TEST_CASE("DeviceModel DeviceAdded and DeviceUpdated signals fire after snapshot is updated",
    "[UI][DeviceModel]")
{
    ZDeviceModel Model;

    // 验证 DeviceAdded 发出时可读到新设备的 displayName
    QString CapturedDisplayName;
    QObject::connect(&Model, &ZDeviceModel::deviceAdded,
        [&Model, &CapturedDisplayName](const QString&)
        {
            CapturedDisplayName = Model.displayNameAt(Model.rowCount() - 1);
        });

    Model.AddOrUpdateDevice(MakeDeviceInfo("dev_1", "New Pad"));
    REQUIRE(CapturedDisplayName == "New Pad");

    // 验证 DeviceUpdated 发出时可读到更新后的 displayName
    QString CapturedUpdatedName;
    QObject::connect(&Model, &ZDeviceModel::deviceUpdated,
        [&Model, &CapturedUpdatedName](const QString&)
        {
            CapturedUpdatedName = Model.displayNameAt(0);
        });

    Model.AddOrUpdateDevice(MakeDeviceInfo("dev_1", "Updated Pad"));
    REQUIRE(CapturedUpdatedName == "Updated Pad");
}
