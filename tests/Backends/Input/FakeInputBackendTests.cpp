// ZFakeInputBackend 单元测试。
// 覆盖生命周期管理、设备增删、回调触发、空回调安全和设备快照隔离。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/FakeInputBackend.h"
#include "Core/ControlId.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SDeviceInfo MakeDevice(const StdString& Id, const StdString& Name)
{
    SDeviceInfo Info;
    Info.Id = SDeviceId{.Value = Id};
    Info.Name = Name;
    Info.Backend = "fake";
    return Info;
}

static SInputEvent MakeButtonPress(const StdString& DeviceId, StdStringView ControlId)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EInputEventType::Pressed;
    Event.Value = 1.0f;
    Event.Timestamp = std::chrono::steady_clock::now();
    return Event;
}

// ── 生命周期 ──

TEST_CASE("FakeBackend default state is stopped with empty devices", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    REQUIRE_FALSE(Backend.IsRunning());
    REQUIRE(Backend.ListDevices().empty());
}

TEST_CASE("FakeBackend Start and Stop lifecycle", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    auto StartResult = Backend.Start();
    REQUIRE(StartResult.IsOk());
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
    REQUIRE_FALSE(Backend.IsRunning());
}

TEST_CASE("FakeBackend repeated Start returns success", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    REQUIRE(Backend.Start().IsOk());
    REQUIRE(Backend.Start().IsOk());
    REQUIRE(Backend.IsRunning());
}

TEST_CASE("FakeBackend repeated Stop is safe", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    (void)Backend.Start();
    Backend.Stop();
    Backend.Stop();
    REQUIRE_FALSE(Backend.IsRunning());
}

// ── 设备管理 ──

TEST_CASE("FakeBackend AddDevice updates ListDevices", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));

    auto Devices = Backend.ListDevices();
    REQUIRE(Devices.size() == 2);
    REQUIRE(Devices[0].Id.Value == "dev_1");
    REQUIRE(Devices[1].Id.Value == "dev_2");
}

TEST_CASE("FakeBackend AddDevice while running triggers OnDeviceConnected", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    int ConnectedCount = 0;
    StdString LastConnectedId;
    Backend.OnDeviceConnected = [&](const SDeviceInfo& DeviceInfo)
    {
        ConnectedCount++;
        LastConnectedId = DeviceInfo.Id.Value;
    };

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    REQUIRE(ConnectedCount == 1);
    REQUIRE(LastConnectedId == "dev_1");
}

TEST_CASE("FakeBackend AddDevice while stopped does not trigger callback", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    int ConnectedCount = 0;
    Backend.OnDeviceConnected = [&](const SDeviceInfo&)
    {
        ConnectedCount++;
    };

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    REQUIRE(ConnectedCount == 0);
    REQUIRE(Backend.ListDevices().size() == 1);
}

TEST_CASE("FakeBackend duplicate DeviceId does not add or trigger callback", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    int ConnectedCount = 0;
    Backend.OnDeviceConnected = [&](const SDeviceInfo&)
    {
        ConnectedCount++;
    };

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Backend.AddDevice(MakeDevice("dev_1", "Controller A Duplicate"));

    REQUIRE(ConnectedCount == 1);
    REQUIRE(Backend.ListDevices().size() == 1);
    REQUIRE(Backend.ListDevices()[0].Name == "Controller A");
}

TEST_CASE("FakeBackend AddDevice with null OnDeviceConnected does not crash", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    // OnDeviceConnected 未设置，不应崩溃
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    REQUIRE(Backend.ListDevices().size() == 1);
}

// ── 设备移除 ──

TEST_CASE("FakeBackend RemoveDevice while running triggers OnDeviceDisconnected", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    int DisconnectedCount = 0;
    StdString LastDisconnectedId;
    Backend.OnDeviceDisconnected = [&](const SDeviceId& DeviceId)
    {
        DisconnectedCount++;
        LastDisconnectedId = DeviceId.Value;
    };

    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(DisconnectedCount == 1);
    REQUIRE(LastDisconnectedId == "dev_1");
    REQUIRE(Backend.ListDevices().empty());
}

TEST_CASE("FakeBackend RemoveDevice nonexistent does not trigger callback", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    int DisconnectedCount = 0;
    Backend.OnDeviceDisconnected = [&](const SDeviceId&)
    {
        DisconnectedCount++;
    };

    Backend.RemoveDevice(SDeviceId{.Value = "nonexistent"});

    REQUIRE(DisconnectedCount == 0);
}

TEST_CASE("FakeBackend RemoveDevice with null OnDeviceDisconnected does not crash", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    // OnDeviceDisconnected 未设置，不应崩溃
    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(Backend.ListDevices().empty());
}

// ── 输入事件 ──

TEST_CASE("FakeBackend EmitInput while running triggers OnInputEvent", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    int EventCount = 0;
    StdString LastControlId;
    float LastValue = 0.0f;
    Backend.OnInputEvent = [&](const SInputEvent& Event)
    {
        EventCount++;
        LastControlId = Event.ControlId;
        LastValue = Event.Value;
    };

    Backend.EmitInput(MakeButtonPress("dev_1", ControlId::ButtonSouth));

    REQUIRE(EventCount == 1);
    REQUIRE(LastControlId == "button_south");
    REQUIRE(LastValue == 1.0f);
}

TEST_CASE("FakeBackend EmitInput while stopped does not trigger callback", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    int EventCount = 0;
    Backend.OnInputEvent = [&](const SInputEvent&)
    {
        EventCount++;
    };

    Backend.EmitInput(MakeButtonPress("dev_1", ControlId::ButtonSouth));

    REQUIRE(EventCount == 0);
}

TEST_CASE("FakeBackend EmitInput with null OnInputEvent does not crash", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    // OnInputEvent 未设置，不应崩溃
    Backend.EmitInput(MakeButtonPress("dev_1", ControlId::ButtonSouth));
}

// ── 快照隔离 ──

TEST_CASE("FakeBackend ListDevices returns snapshot not internal reference", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    auto Snapshot = Backend.ListDevices();
    Snapshot.clear();

    // 清空返回值不应影响后端内部设备列表
    REQUIRE(Backend.ListDevices().size() == 1);
}

// ── 启动失败注入 ──

TEST_CASE("FakeBackend SetStartError makes Start return failure", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    Backend.SetStartError("simulated failure");
    auto Result = Backend.Start();

    REQUIRE_FALSE(Result);
    REQUIRE(Result.Failure().Message == "simulated failure");
    REQUIRE_FALSE(Backend.IsRunning());
}

TEST_CASE("FakeBackend ClearStartError restores Start success", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    Backend.SetStartError("temporary");
    (void)Backend.Start();
    REQUIRE_FALSE(Backend.IsRunning());

    Backend.ClearStartError();
    auto Result = Backend.Start();

    REQUIRE(Result);
    REQUIRE(Backend.IsRunning());
}

TEST_CASE("FakeBackend SetStartError does not affect already running backend", "[Backend][Fake]")
{
    ZFakeInputBackend Backend;

    (void)Backend.Start();
    REQUIRE(Backend.IsRunning());

    // 已 running 时设置 error 不影响当前状态
    Backend.SetStartError("late error");

    // 重复 Start 在 running 状态直接返回 Ok，不检查 injected error
    auto Result = Backend.Start();
    REQUIRE(Result);
    REQUIRE(Backend.IsRunning());
}
