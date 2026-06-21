// ZMappingSession 单元测试。
// 验证映射会话的完整链路：输入事件 → MappingEngine → ActionDispatcher → NullOutputBackend，
// 以及 enabled/disabled、profile 快照隔离、记录管理和容量上限。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/ActionDispatcher.h"
#include "Runtime/MappingSession.h"

using namespace ZeroMapper;

// ── 构造辅助 ──

static SInputEvent MakeButtonEvent(StdStringView ControlId, EInputEventType EventType)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = "dev_1"};
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

TEST_CASE("MappingSession default is enabled", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    REQUIRE(Session.IsEnabled() == true);
    REQUIRE(Session.GetRecentRecordCount() == 0);
}

// ── 空 profile 无动作 ──

TEST_CASE("MappingSession empty profile produces no actions", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 0);
    REQUIRE(Result.bMapped == false);
    REQUIRE(Result.bDispatched == false);
    REQUIRE(Backend.GetActionCount() == 0);
}

// ── ReplaceProfile 后映射 ──

TEST_CASE("MappingSession maps and dispatches after ReplaceProfile", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 1);
    REQUIRE(Result.bMapped == true);
    REQUIRE(Result.bDispatched == true);
    REQUIRE(Backend.GetActionCount() == 1);

    auto Actions = Backend.ListActions();
    REQUIRE(Actions[0].Type == EActionType::KeyboardKey);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.Key == "Space");
    REQUIRE(Payload.bPressed == true);
}

// ── ReplaceProfile 快照隔离 ──

TEST_CASE("MappingSession ReplaceProfile stores snapshot", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    auto Profile = MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    });
    Session.ReplaceProfile(Profile);

    // 外部修改原 profile 不影响 session
    Profile.Rules.clear();

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 1);
    REQUIRE(Result.bMapped == true);
}

// ── GetProfileSnapshot 返回拷贝 ──

TEST_CASE("MappingSession GetProfileSnapshot returns copy", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "A"),
    }));

    auto Snapshot = Session.GetProfileSnapshot();
    REQUIRE(Snapshot.Rules.size() == 1);

    // 修改快照不影响 session 内部
    Snapshot.Rules.clear();

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));
    REQUIRE(Result.ActionCount == 1);
}

// ── disabled 状态 ──

TEST_CASE("MappingSession disabled does not map or dispatch", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));
    Session.SetEnabled(false);

    REQUIRE(Session.IsEnabled() == false);

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 0);
    REQUIRE(Result.bMapped == false);
    REQUIRE(Result.bDispatched == false);
    REQUIRE(Result.Message.find("disabled") != StdString::npos);
    REQUIRE(Backend.GetActionCount() == 0);
}

TEST_CASE("MappingSession re-enable restores mapping", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    Session.SetEnabled(false);
    (void)Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));
    REQUIRE(Backend.GetActionCount() == 0);

    Session.SetEnabled(true);
    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.bMapped == true);
    REQUIRE(Result.bDispatched == true);
    REQUIRE(Backend.GetActionCount() == 1);
}

// ── profile disabled ──

TEST_CASE("MappingSession profile disabled produces no actions", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    auto Profile = MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    });
    Profile.bEnabled = false;
    Session.ReplaceProfile(Profile);

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 0);
    REQUIRE(Result.bMapped == false);
    REQUIRE(Backend.GetActionCount() == 0);
}

// ── 多条规则命中 ──

TEST_CASE("MappingSession multiple rules dispatch multiple actions", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
        MakeButtonToKeyRule("r2", ControlId::ButtonSouth, "Enter"),
    }));

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 2);
    REQUIRE(Result.bMapped == true);
    REQUIRE(Result.bDispatched == true);
    REQUIRE(Backend.GetActionCount() == 2);
}

// ── 派发失败 ──

TEST_CASE("MappingSession marks dispatch failure from backend", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    Backend.SetError("hardware fault");
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));

    auto Result = Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Result.ActionCount == 1);
    REQUIRE(Result.bMapped == true);
    REQUIRE(Result.bDispatched == false);
    REQUIRE(Backend.GetActionCount() == 0);
}

// ── 记录管理 ──

TEST_CASE("MappingSession no match still appends record", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    (void)Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Session.GetRecentRecordCount() == 1);
    auto Records = Session.ListRecentRecords();
    REQUIRE(Records[0].Result.bMapped == false);
}

TEST_CASE("MappingSession disabled input still appends record", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);
    Session.SetEnabled(false);

    (void)Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Session.GetRecentRecordCount() == 1);
    auto Records = Session.ListRecentRecords();
    REQUIRE(Records[0].Result.Message.find("disabled") != StdString::npos);
}

TEST_CASE("MappingSession ClearRecentRecords preserves enabled and profile", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    Session.ReplaceProfile(MakeTestProfile({
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"),
    }));
    (void)Session.HandleInputEvent(
        MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));
    REQUIRE(Session.GetRecentRecordCount() == 1);

    Session.ClearRecentRecords();

    REQUIRE(Session.GetRecentRecordCount() == 0);
    REQUIRE(Session.IsEnabled() == true);
    REQUIRE(Session.GetProfileSnapshot().Rules.size() == 1);
}

TEST_CASE("MappingSession recent records capacity limit 128", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    for (uint32 Index = 0; Index < ZMappingSession::MaxRecentRecords + 10; ++Index)
    {
        (void)Session.HandleInputEvent(
            MakeButtonEvent(ControlId::ButtonSouth, EInputEventType::Pressed));
    }

    REQUIRE(Session.GetRecentRecordCount() == ZMappingSession::MaxRecentRecords);
}

// ── 无平台依赖 ──

TEST_CASE("MappingSession has no platform dependencies", "[Runtime][MappingSession]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    ZMappingSession Session(Dispatcher);

    REQUIRE(Session.IsEnabled() == true);
}
