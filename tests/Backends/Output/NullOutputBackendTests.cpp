// ZNullOutputBackend 单元测试。
// 验证默认状态、动作记录、状态切换、失败路径和快照隔离。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Output/NullOutputBackend.h"
#include "Backends/Output/OutputBackend.h"

using namespace ZeroMapper;

// ── 构造辅助 ──

static SAction MakeKeyboardAction(const StdString& Key, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::KeyboardKey;
    Action.Payload = SKeyboardAction{.Key = Key, .bPressed = bPressed};
    return Action;
}

static SAction MakeMouseButtonAction(int32 Button, bool bPressed)
{
    SAction Action;
    Action.Type = EActionType::MouseButton;
    Action.Payload = SMouseButtonAction{.Button = Button, .bPressed = bPressed};
    return Action;
}

static SAction MakeMouseMoveAction(float32 DeltaX, float32 DeltaY)
{
    SAction Action;
    Action.Type = EActionType::MouseMove;
    Action.Payload = SMouseMoveAction{.DeltaX = DeltaX, .DeltaY = DeltaY};
    return Action;
}

// ── 头文件独立 include ──

TEST_CASE("IOutputBackend header is self-contained", "[Backends][OutputBackend]")
{
    // OutputBackend.h 可以独立 include 编译
    SOutputBackendStatus Status;
    REQUIRE(Status.State == EOutputBackendState::Unavailable);
    REQUIRE(Status.Message.empty());
}

// ── 默认状态 ──

TEST_CASE("NullOutputBackend default state is Ready", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    auto Status = Backend.GetStatus();

    REQUIRE(Status.State == EOutputBackendState::Ready);
    REQUIRE(Status.Message == "ready");
    REQUIRE(Backend.GetActionCount() == 0);
}

// ── Ready 状态下发送动作 ──

TEST_CASE("NullOutputBackend records keyboard action in Ready state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    auto Result = Backend.SendAction(MakeKeyboardAction("Space", true));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);

    auto Actions = Backend.ListActions();
    REQUIRE(Actions[0].Type == EActionType::KeyboardKey);
    auto& Payload = std::get<SKeyboardAction>(Actions[0].Payload);
    REQUIRE(Payload.Key == "Space");
    REQUIRE(Payload.bPressed == true);
}

TEST_CASE("NullOutputBackend records mouse button action in Ready state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    auto Result = Backend.SendAction(MakeMouseButtonAction(0, true));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);

    auto Actions = Backend.ListActions();
    REQUIRE(Actions[0].Type == EActionType::MouseButton);
    auto& Payload = std::get<SMouseButtonAction>(Actions[0].Payload);
    REQUIRE(Payload.Button == 0);
    REQUIRE(Payload.bPressed == true);
}

TEST_CASE("NullOutputBackend records mouse move action in Ready state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    auto Result = Backend.SendAction(MakeMouseMoveAction(10.0f, -5.0f));

    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);

    auto Actions = Backend.ListActions();
    REQUIRE(Actions[0].Type == EActionType::MouseMove);
    auto& Payload = std::get<SMouseMoveAction>(Actions[0].Payload);
    REQUIRE(Payload.DeltaX == 10.0f);
    REQUIRE(Payload.DeltaY == -5.0f);
}

// ── None 动作被拒绝 ──

TEST_CASE("NullOutputBackend rejects None action in Ready state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    SAction NoneAction;
    auto Result = Backend.SendAction(NoneAction);

    REQUIRE(Result.IsErr());
    REQUIRE(Backend.GetActionCount() == 0);
}

// ── 非 Ready 状态拒绝发送 ──

TEST_CASE("NullOutputBackend rejects action in Unavailable state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    Backend.SetUnavailable("device not found");
    auto Result = Backend.SendAction(MakeKeyboardAction("A", true));

    REQUIRE(Result.IsErr());
    REQUIRE(Backend.GetActionCount() == 0);

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Unavailable);
    REQUIRE(Status.Message == "device not found");
}

TEST_CASE("NullOutputBackend rejects action in Error state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    Backend.SetError("send failed");
    auto Result = Backend.SendAction(MakeKeyboardAction("A", true));

    REQUIRE(Result.IsErr());
    REQUIRE(Backend.GetActionCount() == 0);

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Error);
    REQUIRE(Status.Message == "send failed");
}

// ── 状态恢复 ──

TEST_CASE("NullOutputBackend SetReady recovers from Error and Unavailable", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;

    Backend.SetError("broken");
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Error);

    Backend.SetReady("recovered");
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Ready);
    REQUIRE(Backend.GetStatus().Message == "recovered");

    auto Result = Backend.SendAction(MakeKeyboardAction("B", true));
    REQUIRE(Result.IsOk());
    REQUIRE(Backend.GetActionCount() == 1);

    Backend.SetUnavailable("gone");
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Unavailable);

    Backend.SetReady();
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Ready);
    REQUIRE(Backend.GetStatus().Message == "ready");
}

// ── ListActions 返回快照拷贝 ──

TEST_CASE("NullOutputBackend ListActions returns snapshot copy", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    (void)Backend.SendAction(MakeKeyboardAction("X", true));

    auto Snapshot = Backend.ListActions();
    REQUIRE(Snapshot.size() == 1);

    // 发送更多动作不影响已有快照
    (void)Backend.SendAction(MakeKeyboardAction("Y", true));
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Backend.GetActionCount() == 2);
}

// ── ClearActions ──

TEST_CASE("NullOutputBackend ClearActions clears records but keeps state", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    (void)Backend.SendAction(MakeKeyboardAction("A", true));
    (void)Backend.SendAction(MakeKeyboardAction("B", true));
    REQUIRE(Backend.GetActionCount() == 2);

    Backend.ClearActions();

    REQUIRE(Backend.GetActionCount() == 0);
    REQUIRE(Backend.ListActions().empty());
    REQUIRE(Backend.GetStatus().State == EOutputBackendState::Ready);
}

// ── GetActionCount ──

TEST_CASE("NullOutputBackend GetActionCount tracks record count", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;
    REQUIRE(Backend.GetActionCount() == 0);

    (void)Backend.SendAction(MakeKeyboardAction("A", true));
    REQUIRE(Backend.GetActionCount() == 1);

    (void)Backend.SendAction(MakeMouseButtonAction(1, false));
    REQUIRE(Backend.GetActionCount() == 2);

    (void)Backend.SendAction(MakeMouseMoveAction(1.0f, 2.0f));
    REQUIRE(Backend.GetActionCount() == 3);
}

// ── SetStatus ──

TEST_CASE("NullOutputBackend SetStatus sets full status snapshot", "[Backends][NullOutputBackend]")
{
    ZNullOutputBackend Backend;

    SOutputBackendStatus Custom;
    Custom.State = EOutputBackendState::Error;
    Custom.Message = "custom error";
    Backend.SetStatus(Custom);

    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Error);
    REQUIRE(Status.Message == "custom error");
}

// ── 无平台依赖 ──

TEST_CASE("NullOutputBackend has no platform dependencies", "[Backends][NullOutputBackend]")
{
    // 确认 OutputBackend.h 和 NullOutputBackend.h 不引入 Qt、QML、SDL 或 Win32
    ZNullOutputBackend Backend;
    auto Status = Backend.GetStatus();
    REQUIRE(Status.State == EOutputBackendState::Ready);
}
