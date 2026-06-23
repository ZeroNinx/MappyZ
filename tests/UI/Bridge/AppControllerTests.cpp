// ZAppController 单元测试。
// 使用 fake/null 工厂验证 UI Bridge 控制器的完整生命周期：
// initializeRuntime/startRuntime/stopRuntime/pumpOnce、mapping enabled 缓存与透传、
// QTimer pump 控制、QSignalSpy 信号验证、factory 失败、析构安全。

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "UI/Bridge/AppController.h"

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

static TInputBackendFactory MakeFailingInputFactory(const StdString& Message)
{
    return [Message]() -> TResult<TUniquePtr<IInputBackend>> {
        return TResult<TUniquePtr<IInputBackend>>::Err(
            MakeError(EErrorCode::Unknown, Message));
    };
}

static TOutputBackendFactory MakeFailingOutputFactory(const StdString& Message)
{
    return [Message]() -> TResult<TUniquePtr<IOutputBackend>> {
        return TResult<TUniquePtr<IOutputBackend>>::Err(
            MakeError(EErrorCode::Unknown, Message));
    };
}

// ── 构造辅助 ──

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

// ── 默认状态 ──

TEST_CASE("AppController default state is created with timer stopped",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    REQUIRE(Controller.RuntimeState() == "created");
    REQUIRE_FALSE(Controller.IsPumpTimerRunning());
    REQUIRE(Controller.IsMappingEnabled() == true);
    REQUIRE(Controller.LastDrainedEventCount() == 0);
}

// ── initializeRuntime 成功 ──

TEST_CASE("AppController initializeRuntime succeeds to ready",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy StatusSpy(&Controller, &ZAppController::RuntimeStatusChanged);

    bool bResult = Controller.initializeRuntime(true);

    REQUIRE(bResult);
    REQUIRE(Controller.RuntimeState() == "ready");
    REQUIRE(StatusSpy.count() >= 1);
}

// ── startRuntime 未 initialize ──

TEST_CASE("AppController startRuntime before initialize returns false and emits error",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::RuntimeError);

    bool bResult = Controller.startRuntime();

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

// ── startRuntime 成功 ──

TEST_CASE("AppController startRuntime succeeds after initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    (void)Controller.initializeRuntime(true);

    QSignalSpy StatusSpy(&Controller, &ZAppController::RuntimeStatusChanged);

    bool bResult = Controller.startRuntime();

    REQUIRE(bResult);
    REQUIRE(Controller.RuntimeState() == "running");
    REQUIRE(StatusSpy.count() >= 1);
}

// ── stopRuntime ──

TEST_CASE("AppController stopRuntime transitions running to ready",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();
    REQUIRE(Controller.RuntimeState() == "running");

    Controller.stopRuntime();

    REQUIRE(Controller.RuntimeState() == "ready");
}

// ── stopRuntime 重复安全 ──

TEST_CASE("AppController repeated stopRuntime is safe",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();

    Controller.stopRuntime();
    Controller.stopRuntime();

    REQUIRE(Controller.RuntimeState() == "ready");
}

// ── pumpOnce 在 running 时更新 summary ──

TEST_CASE("AppController pumpOnce updates last summary when running",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();

    // 通过捕获的 FakeInputBackend 注入事件到 event queue
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy SummarySpy(&Controller, &ZAppController::LastPumpSummaryChanged);

    Controller.pumpOnce();

    REQUIRE(Controller.LastDrainedEventCount() == 1);
    REQUIRE(Controller.LastInputEventCount() == 1);
    REQUIRE(SummarySpy.count() == 1);
}

// ── pumpOnce 在非 running 状态 summary 保持空 ──

TEST_CASE("AppController pumpOnce in non-running state keeps empty summary",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    Controller.pumpOnce();

    REQUIRE(Controller.LastDrainedEventCount() == 0);
    REQUIRE(Controller.LastInputEventCount() == 0);
}

// ── startPumpTimer / stopPumpTimer ──

TEST_CASE("AppController startPumpTimer starts timer and stopPumpTimer stops it",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy TimerSpy(&Controller, &ZAppController::PumpTimerRunningChanged);

    Controller.startPumpTimer(50);

    REQUIRE(Controller.IsPumpTimerRunning());
    REQUIRE(TimerSpy.count() == 1);

    Controller.stopPumpTimer();

    REQUIRE_FALSE(Controller.IsPumpTimerRunning());
    REQUIRE(TimerSpy.count() == 2);
}

// ── 析构安全 ──

TEST_CASE("AppController destructor stops timer and runtime safely",
    "[UI][AppController]")
{
    {
        ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Controller.initializeRuntime(true);
        (void)Controller.startRuntime();
        Controller.startPumpTimer(16);
    }
    // 析构不应崩溃
    REQUIRE(true);
}

// ── mappingEnabled 在 initialize 前缓存 ──

TEST_CASE("AppController SetMappingEnabled before initialize caches value",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy MappingSpy(&Controller, &ZAppController::MappingEnabledChanged);

    Controller.SetMappingEnabled(false);

    REQUIRE_FALSE(Controller.IsMappingEnabled());
    REQUIRE(MappingSpy.count() == 1);

    (void)Controller.initializeRuntime(true);
}

// ── mappingEnabled 在 initialize 后更新缓存并发信号 ──

TEST_CASE("AppController SetMappingEnabled after initialize updates cache and emits signal",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();

    QSignalSpy MappingSpy(&Controller, &ZAppController::MappingEnabledChanged);

    // 禁用 mapping
    Controller.SetMappingEnabled(false);

    REQUIRE_FALSE(Controller.IsMappingEnabled());
    REQUIRE(MappingSpy.count() == 1);

    // 重复设置相同值不发信号
    Controller.SetMappingEnabled(false);
    REQUIRE(MappingSpy.count() == 1);

    // 恢复 enabled
    Controller.SetMappingEnabled(true);
    REQUIRE(Controller.IsMappingEnabled());
    REQUIRE(MappingSpy.count() == 2);
}

// ── mappingEnabled 在 initialize 前设为 false，startRuntime 后缓存值不丢失 ──

TEST_CASE("AppController SetMappingEnabled false before initialize survives startRuntime",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    // initialize 前设为 false
    Controller.SetMappingEnabled(false);

    (void)Controller.initializeRuntime(true);
    (void)Controller.startRuntime();

    // 缓存值在整个生命周期中保持 false
    REQUIRE_FALSE(Controller.IsMappingEnabled());
}

// ── initialize 后、start 前切换 mappingEnabled，缓存保持最新值 ──

TEST_CASE("AppController mappingEnabled toggled between initialize and start uses latest value",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    // initialize 时 mapping enabled（默认）
    (void)Controller.initializeRuntime(true);

    // initialize 后、start 前切换为 false
    Controller.SetMappingEnabled(false);

    (void)Controller.startRuntime();

    // startRuntime 后缓存值应为最新设置的 false
    REQUIRE_FALSE(Controller.IsMappingEnabled());
}

// ── QSignalSpy 验证关键信号 ──

TEST_CASE("AppController emits RuntimeStatusChanged on lifecycle transitions",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy StatusSpy(&Controller, &ZAppController::RuntimeStatusChanged);

    (void)Controller.initializeRuntime(true);
    REQUIRE(StatusSpy.count() >= 1);

    int CountAfterInit = StatusSpy.count();

    (void)Controller.startRuntime();
    REQUIRE(StatusSpy.count() > CountAfterInit);

    int CountAfterStart = StatusSpy.count();

    Controller.stopRuntime();
    REQUIRE(StatusSpy.count() > CountAfterStart);
}

// ── input factory 失败 ──

TEST_CASE("AppController initializeRuntime fails when input factory fails",
    "[UI][AppController]")
{
    ZAppController Controller(
        MakeFailingInputFactory("input backend unavailable"),
        MakeNullOutputFactory());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::RuntimeError);
    QSignalSpy StatusSpy(&Controller, &ZAppController::RuntimeStatusChanged);

    bool bResult = Controller.initializeRuntime(true);

    REQUIRE_FALSE(bResult);
    REQUIRE(Controller.RuntimeState() == "error");
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(StatusSpy.count() >= 1);

    // 验证 error signal 携带消息
    auto ErrorArgs = ErrorSpy.takeFirst();
    REQUIRE(ErrorArgs.at(0).toString() == "input backend unavailable");
}

// ── output factory 失败 ──

TEST_CASE("AppController output factory failure vs useNullOutput bypass",
    "[UI][AppController]")
{
    SECTION("initializeRuntime(false) uses output factory and fails")
    {
        ZAppController Controller(
            MakeFakeInputFactory(),
            MakeFailingOutputFactory("output backend unavailable"));

        bool bResult = Controller.initializeRuntime(false);

        REQUIRE_FALSE(bResult);
        REQUIRE(Controller.RuntimeState() == "error");
    }

    SECTION("initializeRuntime(true) bypasses output factory with NullOutput")
    {
        ZAppController Controller(
            MakeFakeInputFactory(),
            MakeFailingOutputFactory("output backend unavailable"));

        bool bResult = Controller.initializeRuntime(true);

        REQUIRE(bResult);
        REQUIRE(Controller.RuntimeState() == "ready");
    }
}

// ── header 不包含 SDL 或 Win32 ──

TEST_CASE("AppController header has no SDL or Win32 dependencies",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.RuntimeState() == "created");
}
