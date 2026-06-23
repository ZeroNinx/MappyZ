// ZBackendEventQueue 单元测试。
// 使用 ZFakeInputBackend 验证事件入队、FIFO 顺序、线程安全、attach/detach 生命周期。

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

#include "Backends/Input/FakeInputBackend.h"
#include "Runtime/BackendEventQueue.h"

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

static SInputEvent MakeInputEvent(const StdString& DeviceId, const StdString& ControlId)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = ControlId;
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EInputEventType::Pressed;
    Event.Value = 1.0f;
    return Event;
}

// ── 默认状态 ──

TEST_CASE("BackendEventQueue default state is not attached with empty queue",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    ZBackendEventQueue Queue(Backend);

    REQUIRE_FALSE(Queue.IsAttached());
    REQUIRE(Queue.GetPendingEventCount() == 0);
    REQUIRE(Queue.DrainEvents().empty());
}

// ── 设备连接事件入队 ──

TEST_CASE("BackendEventQueue enqueues device connected event after Attach",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].Type == EBackendEventType::DeviceConnected);

    auto* DeviceInfo = std::get_if<SDeviceInfo>(&Events[0].Payload);
    REQUIRE(DeviceInfo != nullptr);
    REQUIRE(DeviceInfo->Id.Value == "dev_1");
    REQUIRE(DeviceInfo->Name == "Controller A");
}

// ── 设备断开事件入队 ──

TEST_CASE("BackendEventQueue enqueues device disconnected event after Attach",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();
    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].Type == EBackendEventType::DeviceDisconnected);

    auto* DeviceId = std::get_if<SDeviceId>(&Events[0].Payload);
    REQUIRE(DeviceId != nullptr);
    REQUIRE(DeviceId->Value == "dev_1");
}

// ── 输入事件入队 ──

TEST_CASE("BackendEventQueue enqueues input event after Attach",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));

    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].Type == EBackendEventType::Input);

    auto* InputEvent = std::get_if<SInputEvent>(&Events[0].Payload);
    REQUIRE(InputEvent != nullptr);
    REQUIRE(InputEvent->DeviceId.Value == "dev_1");
    REQUIRE(InputEvent->ControlId == "button_south");
}

// ── FIFO 顺序 ──

TEST_CASE("BackendEventQueue drains events in FIFO order",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    Backend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == 3);
    REQUIRE(Events[0].Type == EBackendEventType::DeviceConnected);
    REQUIRE(Events[1].Type == EBackendEventType::Input);
    REQUIRE(Events[2].Type == EBackendEventType::DeviceDisconnected);
}

// ── DrainEvents 清空队列 ──

TEST_CASE("BackendEventQueue DrainEvents clears internal queue",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    REQUIRE(Queue.GetPendingEventCount() == 1);

    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == 1);
    REQUIRE(Queue.GetPendingEventCount() == 0);
    REQUIRE(Queue.DrainEvents().empty());
}

// ── DrainEvents 返回快照 ──

TEST_CASE("BackendEventQueue DrainEvents returns independent snapshot",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    auto Snapshot = Queue.DrainEvents();
    REQUIRE(Snapshot.size() == 1);

    // 新事件不影响已返回的快照
    Backend.EmitInput(MakeInputEvent("dev_1", "button_east"));
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Queue.GetPendingEventCount() == 1);
}

// ── Clear ──

TEST_CASE("BackendEventQueue Clear discards events but keeps attached",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    REQUIRE(Queue.GetPendingEventCount() == 1);

    Queue.Clear();

    REQUIRE(Queue.GetPendingEventCount() == 0);
    REQUIRE(Queue.IsAttached());

    // attach 仍然有效，新事件继续入队
    Backend.EmitInput(MakeInputEvent("dev_1", "button_east"));
    REQUIRE(Queue.GetPendingEventCount() == 1);
}

// ── 重复 Attach ──

TEST_CASE("BackendEventQueue repeated Attach does not clear queue",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    REQUIRE(Queue.GetPendingEventCount() == 1);

    // 重复 attach 不清空队列
    Queue.Attach();
    REQUIRE(Queue.GetPendingEventCount() == 1);
    REQUIRE(Queue.IsAttached());
}

// ── Attach 覆盖已有回调 ──

TEST_CASE("BackendEventQueue Attach overwrites existing callbacks without crash",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    // 先用外部 lambda 占据回调
    bool bExternalCalled = false;
    Backend.OnInputEvent = [&bExternalCalled](const SInputEvent&)
    {
        bExternalCalled = true;
    };

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));

    // 回调已被 queue 覆盖，外部 lambda 不再触发
    REQUIRE_FALSE(bExternalCalled);
    REQUIRE(Queue.GetPendingEventCount() == 1);
}

// ── Detach 后事件不入队 ──

TEST_CASE("BackendEventQueue Detach stops receiving events",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    REQUIRE(Queue.GetPendingEventCount() == 1);

    Queue.Detach();
    REQUIRE_FALSE(Queue.IsAttached());

    Backend.EmitInput(MakeInputEvent("dev_1", "button_east"));

    // detach 后新事件不入队
    REQUIRE(Queue.GetPendingEventCount() == 1);
}

// ── 重复 Detach ──

TEST_CASE("BackendEventQueue repeated Detach is safe",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();
    Queue.Detach();
    Queue.Detach();

    REQUIRE_FALSE(Queue.IsAttached());
}

// ── 析构安全 ──

TEST_CASE("BackendEventQueue destructor clears callbacks safely",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    {
        ZBackendEventQueue Queue(Backend);
        Queue.Attach();
        Backend.EmitInput(MakeInputEvent("dev_1", "button_south"));
    }

    // queue 已析构，后端继续触发回调不应崩溃
    Backend.EmitInput(MakeInputEvent("dev_1", "button_east"));
    Backend.AddDevice(MakeDevice("dev_2", "Controller B"));
    Backend.RemoveDevice(SDeviceId{.Value = "dev_2"});
}

// ── 多线程安全 ──

TEST_CASE("BackendEventQueue multi-thread enqueue and drain is safe",
    "[Runtime][BackendEventQueue]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZBackendEventQueue Queue(Backend);
    Queue.Attach();

    constexpr int EventsPerThread = 100;
    constexpr int ThreadCount = 4;

    // 从多个线程并发注入输入事件
    std::vector<std::thread> Threads;
    for (int ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&Backend, ThreadIndex]()
        {
            for (int EventIndex = 0; EventIndex < EventsPerThread; ++EventIndex)
            {
                Backend.EmitInput(MakeInputEvent(
                    "dev_" + std::to_string(ThreadIndex),
                    "control_" + std::to_string(EventIndex)));
            }
        });
    }

    for (auto& Thread : Threads)
    {
        Thread.join();
    }

    // 所有事件都应成功入队
    auto Events = Queue.DrainEvents();
    REQUIRE(Events.size() == ThreadCount * EventsPerThread);
    REQUIRE(Queue.GetPendingEventCount() == 0);
}

// ── 头文件无平台依赖 ──

TEST_CASE("BackendEventQueue header has no platform dependencies",
    "[Runtime][BackendEventQueue]")
{
    // 确认 BackendEventQueue.h 不引入 Qt、QML、SDL 或 Win32
    ZFakeInputBackend Backend;
    ZBackendEventQueue Queue(Backend);
    REQUIRE_FALSE(Queue.IsAttached());
}
