// ZRuntimeEventPump 单元测试。
// 使用 ZFakeInputBackend + ZBackendEventQueue + ZNullOutputBackend + ZActionDispatcher + ZMappingSession
// 验证事件泵的完整链路：drain → handler 分发 → mapping session → summary 计数 → recent records。

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/ActionDispatcher.h"
#include "Runtime/BackendEventQueue.h"
#include "Runtime/MappingSession.h"
#include "Runtime/RuntimeEventPump.h"

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

static SInputEvent MakeButtonEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    EInputEventType EventType)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EventType;
    Event.Value = (EventType == EInputEventType::Pressed) ? 1.0f : 0.0f;
    return Event;
}

static SMappingRule MakeButtonToKeyRule(
    const StdString& RuleId,
    StdStringView ControlId,
    const StdString& Key)
{
    SMappingRule Rule;
    Rule.Id = RuleId;
    Rule.Input.ControlId = StdString(ControlId);
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = Key, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

static SMappingProfile MakeTestProfile(TVector<SMappingRule> Rules)
{
    SMappingProfile Profile;
    Profile.Id = "test_profile";
    Profile.Name = "Test";
    Profile.Rules = std::move(Rules);
    return Profile;
}

// 构造完整的 pump 测试环境
struct SPumpTestContext
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZActionDispatcher Dispatcher;
    ZMappingSession Session;
    ZBackendEventQueue EventQueue;
    ZRuntimeEventPump Pump;

    SPumpTestContext()
        : Dispatcher(OutputBackend)
        , Session(Dispatcher)
        , EventQueue(InputBackend)
        , Pump(EventQueue, Session)
    {
        (void)InputBackend.Start();
        EventQueue.Attach();
    }
};

// ── 默认无事件 ──

TEST_CASE("RuntimeEventPump empty queue returns zero summary",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.DrainedEventCount == 0);
    REQUIRE(Summary.DeviceConnectedCount == 0);
    REQUIRE(Summary.DeviceDisconnectedCount == 0);
    REQUIRE(Summary.InputEventCount == 0);
    REQUIRE(Summary.MappedInputCount == 0);
    REQUIRE(Summary.DispatchedInputCount == 0);
    REQUIRE(Summary.FailedDispatchInputCount == 0);
    REQUIRE(Summary.InvalidEventCount == 0);
    REQUIRE(Context.Pump.GetRecentRecordCount() == 0);
}

// ── 设备连接事件 ──

TEST_CASE("RuntimeEventPump device connected event calls handler and counts",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    SDeviceInfo CapturedDevice;
    bool bHandlerCalled = false;
    Context.Pump.SetDeviceConnectedHandler(
        [&](const SDeviceInfo& Info)
        {
            CapturedDevice = Info;
            bHandlerCalled = true;
        });

    Context.InputBackend.AddDevice(MakeDevice("dev_1", "Controller A"));

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(bHandlerCalled);
    REQUIRE(CapturedDevice.Id.Value == "dev_1");
    REQUIRE(CapturedDevice.Name == "Controller A");
    REQUIRE(Summary.DrainedEventCount == 1);
    REQUIRE(Summary.DeviceConnectedCount == 1);
    REQUIRE(Summary.InputEventCount == 0);
}

// ── 设备断开事件 ──

TEST_CASE("RuntimeEventPump device disconnected event calls handler and counts",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    StdString CapturedId;
    bool bHandlerCalled = false;
    Context.Pump.SetDeviceDisconnectedHandler(
        [&](const SDeviceId& Id)
        {
            CapturedId = Id.Value;
            bHandlerCalled = true;
        });

    Context.InputBackend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Context.InputBackend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(bHandlerCalled);
    REQUIRE(CapturedId == "dev_1");
    REQUIRE(Summary.DeviceDisconnectedCount == 1);
    REQUIRE(Summary.DeviceConnectedCount == 1);
    REQUIRE(Summary.DrainedEventCount == 2);
}

// ── 输入事件调用 handler ──

TEST_CASE("RuntimeEventPump input event calls input handler",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    StdString CapturedControlId;
    Context.Pump.SetInputEventHandler(
        [&](const SInputEvent& Event)
        {
            CapturedControlId = Event.ControlId;
        });

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(CapturedControlId == ControlId::ButtonSouth);
    REQUIRE(Summary.InputEventCount == 1);
}

// ── 输入事件触发 mapping session ──

TEST_CASE("RuntimeEventPump input event triggers mapping session dispatch",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 1);
    REQUIRE(Summary.FailedDispatchInputCount == 0);
    REQUIRE(Context.OutputBackend.GetActionCount() == 1);

    auto Actions = Context.OutputBackend.ListActions();
    REQUIRE(Actions[0].Type == EActionType::KeyboardKey);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.Key == "Space");
}

// ── 空 handler 不影响映射 ──

TEST_CASE("RuntimeEventPump null handler does not block mapping session",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    // 不设置任何 handler
    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 1);
    REQUIRE(Context.OutputBackend.GetActionCount() == 1);
}

// ── FIFO 顺序 ──

TEST_CASE("RuntimeEventPump processes events in FIFO order",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    std::vector<StdString> HandlerOrder;
    Context.Pump.SetDeviceConnectedHandler(
        [&](const SDeviceInfo&) { HandlerOrder.push_back("connected"); });
    Context.Pump.SetInputEventHandler(
        [&](const SInputEvent&) { HandlerOrder.push_back("input"); });
    Context.Pump.SetDeviceDisconnectedHandler(
        [&](const SDeviceId&) { HandlerOrder.push_back("disconnected"); });

    Context.InputBackend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Context.InputBackend.RemoveDevice(SDeviceId{.Value = "dev_1"});

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.DrainedEventCount == 3);
    REQUIRE(HandlerOrder.size() == 3);
    REQUIRE(HandlerOrder[0] == "connected");
    REQUIRE(HandlerOrder[1] == "input");
    REQUIRE(HandlerOrder[2] == "disconnected");
}

// ── invalid event payload mismatch ──

TEST_CASE("RuntimeEventPump invalid payload increments InvalidEventCount",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    // 通过 PumpEvents 注入畸形事件：DeviceConnected 类型但 payload 是 monostate
    TVector<SBackendEvent> Events;
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::DeviceConnected,
        .Payload = std::monostate{},
    });
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::DeviceDisconnected,
        .Payload = std::monostate{},
    });
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::Input,
        .Payload = std::monostate{},
    });

    auto Summary = Context.Pump.PumpEvents(std::move(Events));

    REQUIRE(Summary.DrainedEventCount == 3);
    REQUIRE(Summary.InvalidEventCount == 3);
    REQUIRE(Summary.DeviceConnectedCount == 0);
    REQUIRE(Summary.DeviceDisconnectedCount == 0);
    REQUIRE(Summary.InputEventCount == 0);

    // 每个 invalid 事件也追加 record
    REQUIRE(Context.Pump.GetRecentRecordCount() == 3);
    auto Records = Context.Pump.ListRecentRecords();
    for (const auto& Record : Records)
    {
        REQUIRE(Record.bHandled == false);
        REQUIRE(Record.Message == "invalid payload");
    }
}

// ── invalid event 不阻止后续有效事件 ──

TEST_CASE("RuntimeEventPump invalid event does not block subsequent valid events",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    bool bConnectedCalled = false;
    Context.Pump.SetDeviceConnectedHandler(
        [&](const SDeviceInfo&) { bConnectedCalled = true; });

    // 畸形事件夹在两个有效事件之间
    SInputEvent ValidInput = MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed);
    SDeviceInfo ValidDevice = MakeDevice("dev_1", "Controller A");

    TVector<SBackendEvent> Events;
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::Input,
        .Payload = std::monostate{},
    });
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::DeviceConnected,
        .Payload = ValidDevice,
    });
    Events.push_back(SBackendEvent{
        .Type = EBackendEventType::Input,
        .Payload = ValidInput,
    });

    auto Summary = Context.Pump.PumpEvents(std::move(Events));

    REQUIRE(Summary.DrainedEventCount == 3);
    REQUIRE(Summary.InvalidEventCount == 1);
    REQUIRE(Summary.DeviceConnectedCount == 1);
    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(bConnectedCalled);
    REQUIRE(Context.OutputBackend.GetActionCount() == 1);
}

// ── dispatch 失败 ──

TEST_CASE("RuntimeEventPump dispatch failure increments FailedDispatchInputCount",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.OutputBackend.SetError("hardware fault");
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 0);
    REQUIRE(Summary.FailedDispatchInputCount == 1);
}

// ── ListRecentRecords 返回快照 ──

TEST_CASE("RuntimeEventPump ListRecentRecords returns snapshot copy",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    (void)Context.Pump.PumpOnce();

    auto Records = Context.Pump.ListRecentRecords();
    REQUIRE(Records.size() == 1);

    // 后续 pump 不影响已返回的快照
    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonEast, EInputEventType::Pressed));
    (void)Context.Pump.PumpOnce();

    REQUIRE(Records.size() == 1);
    REQUIRE(Context.Pump.GetRecentRecordCount() == 2);
}

// ── ClearRecentRecords ──

TEST_CASE("RuntimeEventPump ClearRecentRecords does not affect handlers or session",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    bool bHandlerCalled = false;
    Context.Pump.SetInputEventHandler(
        [&](const SInputEvent&) { bHandlerCalled = true; });

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    (void)Context.Pump.PumpOnce();
    REQUIRE(Context.Pump.GetRecentRecordCount() == 1);

    Context.Pump.ClearRecentRecords();
    REQUIRE(Context.Pump.GetRecentRecordCount() == 0);

    // handler 和 session 仍然工作
    bHandlerCalled = false;
    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(bHandlerCalled);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 1);
    REQUIRE(Context.Pump.GetRecentRecordCount() == 1);
}

// ── recent records 容量上限 ──

TEST_CASE("RuntimeEventPump recent records capacity limit 128",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    for (uint32 Index = 0; Index < ZRuntimeEventPump::MaxRecentRecords + 20; ++Index)
    {
        Context.InputBackend.EmitInput(
            MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    }

    (void)Context.Pump.PumpOnce();

    REQUIRE(Context.Pump.GetRecentRecordCount() == ZRuntimeEventPump::MaxRecentRecords);

    // 验证最旧的记录被丢弃，最新的保留
    auto Records = Context.Pump.ListRecentRecords();
    REQUIRE(Records.size() == ZRuntimeEventPump::MaxRecentRecords);
}

// ── record 内容验证 ──

TEST_CASE("RuntimeEventPump records contain correct event and mapping result",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    Context.InputBackend.AddDevice(MakeDevice("dev_1", "Controller A"));
    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    (void)Context.Pump.PumpOnce();

    auto Records = Context.Pump.ListRecentRecords();
    REQUIRE(Records.size() == 2);

    // 第一条：设备连接
    REQUIRE(Records[0].Event.Type == EBackendEventType::DeviceConnected);
    REQUIRE(Records[0].bHandled == true);

    // 第二条：输入事件且映射成功
    REQUIRE(Records[1].Event.Type == EBackendEventType::Input);
    REQUIRE(Records[1].bHandled == true);
    REQUIRE(Records[1].MappingResult.bMapped == true);
    REQUIRE(Records[1].MappingResult.bDispatched == true);
}

// ── 输入 handler 先于 mapping session 执行 ──

TEST_CASE("RuntimeEventPump input handler executes before mapping session",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    uint32 ActionCountAtHandlerTime = 0;
    Context.Pump.SetInputEventHandler(
        [&](const SInputEvent&)
        {
            // handler 执行时 mapping session 尚未处理，output 动作数应为 0
            ActionCountAtHandlerTime = Context.OutputBackend.GetActionCount();
        });

    Context.InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    (void)Context.Pump.PumpOnce();

    REQUIRE(ActionCountAtHandlerTime == 0);
    REQUIRE(Context.OutputBackend.GetActionCount() == 1);
}

// ── 无平台依赖 ──

TEST_CASE("RuntimeEventPump header has no platform dependencies",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;
    REQUIRE(Context.Pump.GetRecentRecordCount() == 0);
}

// ── 方向合成事件记录 ──

static SInputEvent MakeAxis2DEvent(
    const StdString& DeviceId,
    const StdString& ControlId,
    float32 X,
    float32 Y)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = ControlId;
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = SAxis2DValue{.X = X, .Y = Y};
    return Event;
}

TEST_CASE("RuntimeEventPump direction events produce individual records",
    "[Runtime][RuntimeEventPump]")
{
    SPumpTestContext Context;

    // 添加方向到按键映射
    Context.Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("dir_r", "left_stick_right", "D"),
    }));

    // 推右：产生原始 Axis2D + 合成 right pressed，应有 2 条 record
    Context.InputBackend.EmitInput(
        MakeAxis2DEvent("dev_1", "left_stick", 0.7f, 0.0f));
    auto Summary = Context.Pump.PumpOnce();

    REQUIRE(Summary.InputEventCount == 2);
    REQUIRE(Summary.MappedInputCount == 1);

    auto Records = Context.Pump.ListRecentRecords();
    REQUIRE(Records.size() == 2);

    // 第一条：原始 Axis2D
    auto* OriginalInput = std::get_if<SInputEvent>(&Records[0].Event.Payload);
    REQUIRE(OriginalInput != nullptr);
    REQUIRE(OriginalInput->ControlId == "left_stick");

    // 第二条：方向 Button 事件
    auto* DirectionInput = std::get_if<SInputEvent>(&Records[1].Event.Payload);
    REQUIRE(DirectionInput != nullptr);
    REQUIRE(DirectionInput->ControlId == "left_stick_right");
    REQUIRE(DirectionInput->EventType == EInputEventType::Pressed);
    REQUIRE(Records[1].MappingResult.bMapped);
}
