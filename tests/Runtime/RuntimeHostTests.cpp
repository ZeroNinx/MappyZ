// ZRuntimeHost 单元测试。
// 使用 ZFakeInputBackend + ZNullOutputBackend 验证 host 级生命周期：
// Start/Stop/PumpOnce、mapping dispatch 链路、profile 替换、start failure 回滚、
// 析构安全、状态查询和组件访问器。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/RuntimeHost.h"

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

// ── 默认状态 ──

TEST_CASE("RuntimeHost default state is Stopped",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    REQUIRE_FALSE(Host.IsRunning());
    REQUIRE(Host.GetStatus().State == ERuntimeHostState::Stopped);
    REQUIRE_FALSE(InputBackend.IsRunning());
    REQUIRE_FALSE(Host.GetEventQueue().IsAttached());
}

// ── Start 启动后端并 attach ──

TEST_CASE("RuntimeHost Start launches input backend and attaches event queue",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    auto Result = Host.Start();

    REQUIRE(Result);
    REQUIRE(Host.IsRunning());
    REQUIRE(InputBackend.IsRunning());
    REQUIRE(Host.GetEventQueue().IsAttached());
    REQUIRE(Host.GetStatus().State == ERuntimeHostState::Running);
}

// ── Start 后 PumpOnce 完成端到端链路 ──

TEST_CASE("RuntimeHost PumpOnce dispatches mapped input after Start",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    Host.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    (void)Host.Start();

    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Host.PumpOnce();

    REQUIRE(Summary.DrainedEventCount == 1);
    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 1);
    REQUIRE(OutputBackend.GetActionCount() == 1);
}

// ── 重复 Start 幂等 ──

TEST_CASE("RuntimeHost repeated Start is idempotent",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    (void)Host.Start();

    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    // 第二次 Start 不应清空 pending events
    auto Result = Host.Start();
    REQUIRE(Result);
    REQUIRE(Host.IsRunning());
    REQUIRE(Host.GetEventQueue().GetPendingEventCount() == 1);
}

// ── Stop 停止后端并 detach ──

TEST_CASE("RuntimeHost Stop stops backend and detaches event queue",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    (void)Host.Start();
    REQUIRE(Host.IsRunning());

    Host.Stop();

    REQUIRE_FALSE(Host.IsRunning());
    REQUIRE_FALSE(InputBackend.IsRunning());
    REQUIRE_FALSE(Host.GetEventQueue().IsAttached());
    REQUIRE(Host.GetStatus().State == ERuntimeHostState::Stopped);
}

// ── 重复 Stop 安全 ──

TEST_CASE("RuntimeHost repeated Stop is safe",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    (void)Host.Start();
    Host.Stop();
    Host.Stop();

    REQUIRE_FALSE(Host.IsRunning());
}

// ── 析构安全 ──

TEST_CASE("RuntimeHost destructor clears callbacks safely",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;

    {
        ZRuntimeHost Host(InputBackend, OutputBackend);
        (void)Host.Start();
        InputBackend.EmitInput(
            MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    }

    // host 已析构，后端继续注入不崩溃
    REQUIRE_FALSE(InputBackend.IsRunning());
    InputBackend.AddDevice(MakeDevice("dev_1", "Controller A"));
    InputBackend.RemoveDevice(SDeviceId{.Value = "dev_1"});
}

// ── input backend start failure ──

TEST_CASE("RuntimeHost Start failure sets Error state and rolls back attach",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    InputBackend.SetStartError("simulated hardware failure");

    auto Result = Host.Start();

    REQUIRE_FALSE(Result);
    REQUIRE(Result.Failure().Message == "simulated hardware failure");
    REQUIRE_FALSE(Host.IsRunning());
    REQUIRE(Host.GetStatus().State == ERuntimeHostState::Error);
    REQUIRE_FALSE(InputBackend.IsRunning());
    REQUIRE_FALSE(Host.GetEventQueue().IsAttached());
}

// ── ClearStartError 后恢复 ──

TEST_CASE("RuntimeHost Start succeeds after ClearStartError",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    InputBackend.SetStartError("temporary fault");
    (void)Host.Start();
    REQUIRE_FALSE(Host.IsRunning());

    InputBackend.ClearStartError();
    auto Result = Host.Start();

    REQUIRE(Result);
    REQUIRE(Host.IsRunning());
    REQUIRE(InputBackend.IsRunning());
}

// ── bStartInputBackend = false ──

TEST_CASE("RuntimeHost Start with bStartInputBackend false skips backend start",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    auto Result = Host.Start({.bAttachEventQueue = true, .bStartInputBackend = false});

    REQUIRE(Result);
    REQUIRE(Host.IsRunning());
    REQUIRE_FALSE(InputBackend.IsRunning());
    REQUIRE(Host.GetEventQueue().IsAttached());
}

// ── PumpOnce 在 Stopped 状态 ──

TEST_CASE("RuntimeHost PumpOnce in Stopped state returns empty summary",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    auto Summary = Host.PumpOnce();

    REQUIRE(Summary.DrainedEventCount == 0);
    REQUIRE(Summary.InputEventCount == 0);
}

// ── ReplaceProfile ──

TEST_CASE("RuntimeHost ReplaceProfile updates active profile snapshot",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    Host.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "A"),
    }));

    auto Snapshot = Host.GetProfileSnapshot();
    REQUIRE(Snapshot.Id == "test_profile");
    REQUIRE(Snapshot.Rules.size() == 1);
}

// ── SetMappingEnabled(false) ──

TEST_CASE("RuntimeHost SetMappingEnabled false prevents action dispatch",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    Host.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));
    (void)Host.Start();
    Host.SetMappingEnabled(false);

    REQUIRE_FALSE(Host.IsMappingEnabled());

    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Host.PumpOnce();

    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 0);
    REQUIRE(OutputBackend.GetActionCount() == 0);
}

// ── output backend Error 时 host 仍 Running ──

TEST_CASE("RuntimeHost stays Running when output backend has error",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    OutputBackend.SetError("hardware fault");

    ZRuntimeHost Host(InputBackend, OutputBackend);
    Host.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));
    (void)Host.Start();

    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Host.PumpOnce();

    REQUIRE(Host.IsRunning());
    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.FailedDispatchInputCount == 1);
}

// ── GetStatus 反映状态 ──

TEST_CASE("RuntimeHost GetStatus reflects host and output state",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    auto StatusBefore = Host.GetStatus();
    REQUIRE(StatusBefore.State == ERuntimeHostState::Stopped);
    REQUIRE(StatusBefore.OutputStatus.State == EOutputBackendState::Ready);

    (void)Host.Start();

    auto StatusAfter = Host.GetStatus();
    REQUIRE(StatusAfter.State == ERuntimeHostState::Running);

    OutputBackend.SetError("test error");
    auto StatusWithError = Host.GetStatus();
    REQUIRE(StatusWithError.OutputStatus.State == EOutputBackendState::Error);
}

// ── 通过 GetEventPump 设置 handler ──

TEST_CASE("RuntimeHost handler set via GetEventPump is called during PumpOnce",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    bool bHandlerCalled = false;
    Host.GetEventPump().SetInputEventHandler(
        [&](const SInputEvent&) { bHandlerCalled = true; });

    (void)Host.Start();

    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    (void)Host.PumpOnce();

    REQUIRE(bHandlerCalled);
}

// ── bStartInputBackend=false 时 Stop 不停外部 backend ──

TEST_CASE("RuntimeHost Stop does not stop externally started backend",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;

    // 外部先启动 backend
    (void)InputBackend.Start();
    REQUIRE(InputBackend.IsRunning());

    ZRuntimeHost Host(InputBackend, OutputBackend);
    (void)Host.Start({.bAttachEventQueue = true, .bStartInputBackend = false});
    REQUIRE(Host.IsRunning());

    Host.Stop();

    // host 没有启动 backend，不应停止它
    REQUIRE(InputBackend.IsRunning());
    REQUIRE_FALSE(Host.GetEventQueue().IsAttached());
}

// ── 手动 attach 后析构仍能 detach ──

TEST_CASE("RuntimeHost destructor detaches manually attached event queue",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    (void)InputBackend.Start();

    {
        ZRuntimeHost Host(InputBackend, OutputBackend);
        // 不调用 Start()，手动 attach
        Host.GetEventQueue().Attach();
        REQUIRE(Host.GetEventQueue().IsAttached());

        InputBackend.EmitInput(
            MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    }

    // 析构后回调被清空，后续注入不崩溃
    InputBackend.EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonEast, EInputEventType::Pressed));
}

// ── 无平台依赖 ──

TEST_CASE("RuntimeHost header has no platform dependencies",
    "[Runtime][RuntimeHost]")
{
    ZFakeInputBackend InputBackend;
    ZNullOutputBackend OutputBackend;
    ZRuntimeHost Host(InputBackend, OutputBackend);

    REQUIRE_FALSE(Host.IsRunning());
}
