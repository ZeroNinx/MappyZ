// ZActionDispatcher 单元测试。
// 验证单次/批量派发、enabled/disabled 状态、记录管理和容量上限。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Output/NullOutputBackend.h"
#include "Runtime/ActionDispatcher.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SAction MakeKeyAction(const StdString& Key, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SKeyboardAction{.Key = Key, .bPressed = bPressed};
    return Action;
}

static SAction MakeMouseAction(int32 Button, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::MouseButton;
    Action.Payload = SMouseButtonAction{.Button = Button, .bPressed = bPressed};
    return Action;
}

// ── 默认状态 ──

TEST_CASE("ActionDispatcher default is enabled", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    REQUIRE(Dispatcher.IsEnabled() == true);
    REQUIRE(Dispatcher.GetRecentRecordCount() == 0);
}

// ── 单次派发成功 ──

TEST_CASE("ActionDispatcher dispatches keyboard action to backend", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    auto Result = Dispatcher.DispatchAction(MakeKeyAction("Space", true));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);
    REQUIRE(Dispatcher.GetRecentRecordCount() == 1);

    auto Records = Dispatcher.ListRecentRecords();
    REQUIRE(Records[0].bSucceeded == true);
    REQUIRE(Records[0].Action.Type == EActionType::KeyboardKey);
}

TEST_CASE("ActionDispatcher dispatches mouse button action to backend", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    auto Result = Dispatcher.DispatchAction(MakeMouseAction(0, true));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);

    auto Records = Dispatcher.ListRecentRecords();
    REQUIRE(Records[0].bSucceeded == true);
    REQUIRE(Records[0].Action.Type == EActionType::MouseButton);
}

// ── 后端失败 ──

TEST_CASE("ActionDispatcher records failure when backend is in Error state", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    Backend.SetError("hardware fault");
    ZActionDispatcher Dispatcher(Backend);

    auto Result = Dispatcher.DispatchAction(MakeKeyAction("A", true));

    REQUIRE(Result.IsErr());
    REQUIRE(Backend.GetActionCount() == 0);
    REQUIRE(Dispatcher.GetRecentRecordCount() == 1);

    auto Records = Dispatcher.ListRecentRecords();
    REQUIRE(Records[0].bSucceeded == false);
    REQUIRE(Records[0].Message == "hardware fault");
}

// ── disabled 状态 ──

TEST_CASE("ActionDispatcher disabled does not call backend", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    Dispatcher.SetEnabled(false);

    REQUIRE(Dispatcher.IsEnabled() == false);

    auto Result = Dispatcher.DispatchAction(MakeKeyAction("A", true));

    REQUIRE(Result.IsErr());
    REQUIRE(Backend.GetActionCount() == 0);

    auto Records = Dispatcher.ListRecentRecords();
    REQUIRE(Records[0].bSucceeded == false);
    REQUIRE(Records[0].Message.find("disabled") != StdString::npos);
}

TEST_CASE("ActionDispatcher re-enable restores dispatch", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    Dispatcher.SetEnabled(false);
    (void)Dispatcher.DispatchAction(MakeKeyAction("A", true));
    REQUIRE(Backend.GetActionCount() == 0);

    Dispatcher.SetEnabled(true);
    auto Result = Dispatcher.DispatchAction(MakeKeyAction("B", true));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);
}

// ── 批量派发 ──

TEST_CASE("ActionDispatcher empty batch returns success summary", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    auto Summary = Dispatcher.DispatchActions({});

    REQUIRE(Summary.RequestedCount == 0);
    REQUIRE(Summary.SucceededCount == 0);
    REQUIRE(Summary.FailedCount == 0);
    REQUIRE(Summary.bSucceeded == true);
}

TEST_CASE("ActionDispatcher batch dispatches actions in order", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    TVector<SAction> Actions = {
        MakeKeyAction("A", true),
        MakeKeyAction("B", true),
        MakeMouseAction(1, false),
    };

    auto Summary = Dispatcher.DispatchActions(Actions);

    REQUIRE(Summary.RequestedCount == 3);
    REQUIRE(Summary.SucceededCount == 3);
    REQUIRE(Summary.FailedCount == 0);
    REQUIRE(Summary.bSucceeded == true);
    REQUIRE(Backend.GetActionCount() == 3);

    auto BackendActions = Backend.ListActions();
    REQUIRE(std::get<SKeyboardAction>(BackendActions[0].Payload).Key == "A");
    REQUIRE(std::get<SKeyboardAction>(BackendActions[1].Payload).Key == "B");
    REQUIRE(std::get<SMouseButtonAction>(BackendActions[2].Payload).Button == 1);
}

TEST_CASE("ActionDispatcher batch continues after failure", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    // None 动作会被后端拒绝，前后的有效动作应继续派发
    TVector<SAction> Actions = {
        MakeKeyAction("A", true),
        SAction{},  // EActionType::None，会失败
        MakeKeyAction("C", true),
    };

    auto Summary = Dispatcher.DispatchActions(Actions);

    REQUIRE(Summary.RequestedCount == 3);
    REQUIRE(Summary.SucceededCount == 2);
    REQUIRE(Summary.FailedCount == 1);
    REQUIRE(Summary.bSucceeded == false);
    REQUIRE(Backend.GetActionCount() == 2);
}

TEST_CASE("ActionDispatcher batch summary counts are correct", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    TVector<SAction> Actions = {
        MakeKeyAction("A", true),
        SAction{},  // 失败
        SAction{},  // 失败
        MakeKeyAction("D", true),
    };

    auto Summary = Dispatcher.DispatchActions(Actions);

    REQUIRE(Summary.RequestedCount == 4);
    REQUIRE(Summary.SucceededCount == 2);
    REQUIRE(Summary.FailedCount == 2);
    REQUIRE(Summary.bSucceeded == false);
    REQUIRE(Summary.Message.find("2 of 4 failed") != StdString::npos);
}

// ── GetOutputStatus ──

TEST_CASE("ActionDispatcher GetOutputStatus forwards backend status", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    auto Status = Dispatcher.GetOutputStatus();
    REQUIRE(Status.State == EOutputBackendState::Ready);
    REQUIRE(Status.Message == "ready");

    Backend.SetError("broken");
    Status = Dispatcher.GetOutputStatus();
    REQUIRE(Status.State == EOutputBackendState::Error);
    REQUIRE(Status.Message == "broken");
}

// ── 记录管理 ──

TEST_CASE("ActionDispatcher ListRecentRecords returns snapshot copy", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    (void)Dispatcher.DispatchAction(MakeKeyAction("A", true));
    auto Snapshot = Dispatcher.ListRecentRecords();
    REQUIRE(Snapshot.size() == 1);

    (void)Dispatcher.DispatchAction(MakeKeyAction("B", true));
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Dispatcher.GetRecentRecordCount() == 2);
}

TEST_CASE("ActionDispatcher ClearRecentRecords clears but keeps enabled", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    (void)Dispatcher.DispatchAction(MakeKeyAction("A", true));
    (void)Dispatcher.DispatchAction(MakeKeyAction("B", true));
    REQUIRE(Dispatcher.GetRecentRecordCount() == 2);

    Dispatcher.ClearRecentRecords();

    REQUIRE(Dispatcher.GetRecentRecordCount() == 0);
    REQUIRE(Dispatcher.ListRecentRecords().empty());
    REQUIRE(Dispatcher.IsEnabled() == true);
}

TEST_CASE("ActionDispatcher recent records capacity limit 128", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);

    for (uint32 Index = 0; Index < ZActionDispatcher::MaxRecentRecords + 10; ++Index)
    {
        (void)Dispatcher.DispatchAction(MakeKeyAction("K", true));
    }

    REQUIRE(Dispatcher.GetRecentRecordCount() == ZActionDispatcher::MaxRecentRecords);
}

// ── 无平台依赖 ──

TEST_CASE("ActionDispatcher has no platform dependencies", "[Runtime][ActionDispatcher]")
{
    ZNullOutputBackend Backend;
    ZActionDispatcher Dispatcher(Backend);
    auto Status = Dispatcher.GetOutputStatus();
    REQUIRE(Status.State == EOutputBackendState::Ready);
}
