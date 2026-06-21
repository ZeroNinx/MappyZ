// ZInputRuntime 单元测试。
// 使用 ZFakeInputBackend 验证输入事件记录、控件状态快照、容量上限和回调生命周期安全。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/FakeInputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/InputRuntime.h"

using namespace ZeroMapper;

// ── 构造辅助 ──

static SInputEvent MakeButtonEvent(const StdString& DeviceId, StdStringView ControlId, float32 Value)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = (Value > 0.0f) ? EInputEventType::Pressed : EInputEventType::Released;
    Event.Value = Value;
    return Event;
}

static SInputEvent MakeTriggerEvent(const StdString& DeviceId, StdStringView ControlId, float32 Value)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Trigger;
    Event.EventType = EInputEventType::Changed;
    Event.Value = Value;
    return Event;
}

// ── 构造与生命周期 ──

TEST_CASE("InputRuntime default state is not attached with zero counts", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    ZInputRuntime Runtime(Backend);

    REQUIRE_FALSE(Runtime.IsAttached());
    REQUIRE(Runtime.GetRecentEventCount() == 0);
    REQUIRE(Runtime.GetTrackedControlCount() == 0);
    REQUIRE(Runtime.ListRecentEvents().empty());
}

TEST_CASE("InputRuntime Attach receives input events", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));

    REQUIRE(Runtime.IsAttached());
    REQUIRE(Runtime.GetRecentEventCount() == 1);
    REQUIRE(Runtime.ListRecentEvents()[0].ControlId == ControlId::ButtonSouth);
}

TEST_CASE("InputRuntime repeated Attach does not clear existing state", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 1);

    // 重复 Attach 不应清空已有事件
    Runtime.Attach();
    REQUIRE(Runtime.GetRecentEventCount() == 1);
}

// ── 控件状态 ──

TEST_CASE("InputRuntime same control state overwritten by subsequent event", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.3f));
    Backend.EmitInput(MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.8f));

    // 最近事件列表记录了两次
    REQUIRE(Runtime.GetRecentEventCount() == 2);

    // 当前状态只保留最后一次
    REQUIRE(Runtime.GetTrackedControlCount() == 1);
    auto State = Runtime.FindControlState(SDeviceId{.Value = "dev_1"}, ControlId::LeftTrigger);
    REQUIRE(State.has_value());
    REQUIRE(State->Value == 0.8f);
}

TEST_CASE("InputRuntime different devices same control stored separately", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    Backend.EmitInput(MakeButtonEvent("dev_2", ControlId::ButtonSouth, 0.0f));

    REQUIRE(Runtime.GetTrackedControlCount() == 2);

    auto State1 = Runtime.FindControlState(SDeviceId{.Value = "dev_1"}, ControlId::ButtonSouth);
    auto State2 = Runtime.FindControlState(SDeviceId{.Value = "dev_2"}, ControlId::ButtonSouth);

    REQUIRE(State1.has_value());
    REQUIRE(State1->Value == 1.0f);
    REQUIRE(State2.has_value());
    REQUIRE(State2->Value == 0.0f);
}

// ── 查询 ──

TEST_CASE("InputRuntime ListRecentEvents returns snapshot copy", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));

    auto Snapshot = Runtime.ListRecentEvents();
    Snapshot.clear();

    REQUIRE(Runtime.GetRecentEventCount() == 1);
}

TEST_CASE("InputRuntime FindControlState returns empty for unknown control", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    auto State = Runtime.FindControlState(SDeviceId{.Value = "nonexistent"}, ControlId::ButtonSouth);
    REQUIRE_FALSE(State.has_value());
}

// ── Clear ──

TEST_CASE("InputRuntime Clear clears events and state but stays attached", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 1);
    REQUIRE(Runtime.GetTrackedControlCount() == 1);

    Runtime.Clear();

    REQUIRE(Runtime.GetRecentEventCount() == 0);
    REQUIRE(Runtime.GetTrackedControlCount() == 0);
    REQUIRE(Runtime.IsAttached());

    // Clear 后仍然可以接收新事件
    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonNorth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 1);
}

// ── 回调生命周期 ──

TEST_CASE("InputRuntime Detach stops receiving input events", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 1);

    Runtime.Detach();
    REQUIRE_FALSE(Runtime.IsAttached());

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonNorth, 1.0f));

    // detach 后新事件不应进入 runtime
    REQUIRE(Runtime.GetRecentEventCount() == 1);
    REQUIRE(Runtime.GetTrackedControlCount() == 1);
}

TEST_CASE("InputRuntime repeated Detach is safe", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();
    Runtime.Detach();
    Runtime.Detach();

    REQUIRE_FALSE(Runtime.IsAttached());
}

TEST_CASE("InputRuntime re-Attach after Detach preserves state and resumes", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 1);

    Runtime.Detach();
    Runtime.Attach();

    // re-attach 后旧状态应保留
    REQUIRE(Runtime.GetRecentEventCount() == 1);
    REQUIRE(Runtime.GetTrackedControlCount() == 1);

    // re-attach 后可以继续接收新事件
    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonNorth, 1.0f));
    REQUIRE(Runtime.GetRecentEventCount() == 2);
    REQUIRE(Runtime.GetTrackedControlCount() == 2);
}

TEST_CASE("InputRuntime destructor clears callbacks safely", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    {
        ZInputRuntime Runtime(Backend);
        Runtime.Attach();
        Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonSouth, 1.0f));
    }
    // runtime 已析构，后端继续触发输入回调不应崩溃
    Backend.EmitInput(MakeButtonEvent("dev_1", ControlId::ButtonNorth, 1.0f));
}

// ── 容量上限 ──

TEST_CASE("InputRuntime recent events capacity limit discards oldest", "[Runtime][InputRuntime]")
{
    ZFakeInputBackend Backend;
    (void)Backend.Start();

    ZInputRuntime Runtime(Backend);
    Runtime.Attach();

    // 注入超过容量上限的事件，使用整数值避免浮点精度问题
    for (uint32 Index = 0; Index < ZInputRuntime::MaxRecentEvents + 10; ++Index)
    {
        Backend.EmitInput(MakeTriggerEvent("dev_1", ControlId::LeftTrigger, static_cast<float32>(Index)));
    }

    // 最近事件数量应被限制在容量上限
    REQUIRE(Runtime.GetRecentEventCount() == ZInputRuntime::MaxRecentEvents);

    // 最旧的 10 个事件应被丢弃，第一个保留的事件对应 Index=10
    auto Events = Runtime.ListRecentEvents();
    REQUIRE(Events.front().Value == 10.0f);
    REQUIRE(Events.back().Value == static_cast<float32>(ZInputRuntime::MaxRecentEvents + 9));
}
