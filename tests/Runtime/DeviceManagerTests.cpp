// ZDeviceManager 单元测试。
// 使用 ZFakeInputBackend 验证初始设备枚举、热插拔、快照隔离和回调生命周期安全。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/FakeInputBackend.h"
#include "Runtime/DeviceManager.h"

using namespace ZeroMapper;

// ── 构造辅助 ──

static SDeviceInfo MakeDevice(const StdString& Id, const StdString& Name)
{
    SDeviceInfo Info;
    Info.Id = SDeviceId{.Value = Id};
    Info.Name = Name;
    Info.Backend = "fake";
    return Info;
}

// ── 构造与生命周期 ──

TEST_CASE("DeviceManager default state is not attached with zero devices", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    ZDeviceManager Manager(Backend);

    REQUIRE_FALSE(Manager.IsAttached());
    REQUIRE(Manager.GetDeviceCount() == 0);
    REQUIRE(Manager.ListDevices().empty());
}

TEST_CASE("DeviceManager Attach reads initial devices from backend", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    REQUIRE(Manager.IsAttached());
    REQUIRE(Manager.GetDeviceCount() == 2);
    REQUIRE(Manager.ListDevices()[0].Id.Value == "dev_1");
    REQUIRE(Manager.ListDevices()[1].Id.Value == "dev_2");
}

TEST_CASE("DeviceManager repeated Attach does not duplicate devices", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();
    Manager.Attach();

    REQUIRE(Manager.GetDeviceCount() == 1);
}

TEST_CASE("DeviceManager Detach and re-Attach refreshes snapshot", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();
    REQUIRE(Manager.GetDeviceCount() == 1);

    Manager.Detach();
    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));
    Manager.Attach();

    REQUIRE(Manager.GetDeviceCount() == 2);
}

// ── 热插拔 ──

TEST_CASE("DeviceManager hot-plug adds device to snapshot", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZDeviceManager Manager(Backend);
    Manager.Attach();
    REQUIRE(Manager.GetDeviceCount() == 0);

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    REQUIRE(Manager.GetDeviceCount() == 1);
    REQUIRE(Manager.ListDevices()[0].Name == "Controller A");
}

TEST_CASE("DeviceManager hot-unplug removes device from snapshot", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();
    REQUIRE(Manager.GetDeviceCount() == 1);

    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(Manager.GetDeviceCount() == 0);
}

TEST_CASE("DeviceManager duplicate device ID via callback does not add", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Backend.AddDevice(MakeDevice("dev_1", "Controller A Dup"));

    REQUIRE(Manager.GetDeviceCount() == 1);
    REQUIRE(Manager.ListDevices()[0].Name == "Controller A");
}

TEST_CASE("DeviceManager disconnect unknown device does not change snapshot", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    Backend.RemoveDevice(SDeviceId{.Value = "nonexistent"});

    REQUIRE(Manager.GetDeviceCount() == 1);
}

// ── 查询 ──

TEST_CASE("DeviceManager FindDevice returns device when found", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    auto Result = Manager.FindDevice(SDeviceId{.Value = "dev_1"});
    REQUIRE(Result.has_value());
    REQUIRE(Result->Name == "Controller A");
}

TEST_CASE("DeviceManager FindDevice returns empty for unknown device", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    auto Result = Manager.FindDevice(SDeviceId{.Value = "nonexistent"});
    REQUIRE_FALSE(Result.has_value());
}

// ── 快照隔离 ──

TEST_CASE("DeviceManager ListDevices returns snapshot copy", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    auto Snapshot = Manager.ListDevices();
    Snapshot.clear();

    REQUIRE(Manager.GetDeviceCount() == 1);
}

// ── 回调生命周期 ──

TEST_CASE("DeviceManager Detach stops receiving backend events", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZDeviceManager Manager(Backend);
    Manager.Attach();

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    REQUIRE(Manager.GetDeviceCount() == 1);

    Manager.Detach();
    REQUIRE_FALSE(Manager.IsAttached());

    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));

    // detach 后新设备不应出现在 manager 快照中
    REQUIRE(Manager.GetDeviceCount() == 1);
}

TEST_CASE("DeviceManager repeated Detach is safe", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;

    ZDeviceManager Manager(Backend);
    Manager.Attach();
    Manager.Detach();
    Manager.Detach();

    REQUIRE_FALSE(Manager.IsAttached());
}

TEST_CASE("DeviceManager destructor clears callbacks safely", "[Runtime][DeviceManager]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    {
        ZDeviceManager Manager(Backend);
        Manager.Attach();
        Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    }
    // manager 已析构，后端继续触发回调不应崩溃
    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));
    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});
}
