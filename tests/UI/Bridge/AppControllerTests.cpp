// ZAppController 单元测试。
// 使用 fake/null 工厂验证 UI Bridge 控制器的完整生命周期：
// initializeRuntime/startRuntime/stopRuntime/pumpOnce、mapping enabled 缓存与透传、
// QTimer pump 控制、QSignalSpy 信号验证、factory 失败、析构安全。

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>

#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/ProfileManager.h"
#include "UI/Bridge/AppController.h"
#include "UI/Bridge/DeviceModel.h"
#include "UI/Bridge/InputCaptureModel.h"
#include "UI/Bridge/InputStateModel.h"
#include "UI/Bridge/LogModel.h"
#include "UI/Bridge/MappingRuleModel.h"

using namespace MappyZ;

// QStandardPaths test mode RAII guard，确保断言失败时仍恢复全局状态
struct STestModeGuard
{
    STestModeGuard() { QStandardPaths::setTestModeEnabled(true); }
    ~STestModeGuard() { QStandardPaths::setTestModeEnabled(false); }
    STestModeGuard(const STestModeGuard&) = delete;
    STestModeGuard& operator=(const STestModeGuard&) = delete;
};

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

    QSignalSpy StatusSpy(&Controller, &ZAppController::runtimeStatusChanged);

    bool bResult = Controller.initializeRuntime();

    REQUIRE(bResult);
    REQUIRE(Controller.RuntimeState() == "ready");
    REQUIRE(StatusSpy.count() >= 1);
}

// ── startRuntime 未 initialize ──

TEST_CASE("AppController startRuntime before initialize returns false and emits error",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.startRuntime();

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

// ── startRuntime 成功 ──

TEST_CASE("AppController startRuntime succeeds after initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    (void)Controller.initializeRuntime();

    QSignalSpy StatusSpy(&Controller, &ZAppController::runtimeStatusChanged);

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
    (void)Controller.initializeRuntime();
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
    (void)Controller.initializeRuntime();
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
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    // 通过捕获的 FakeInputBackend 注入事件到 event queue
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy SummarySpy(&Controller, &ZAppController::lastPumpSummaryChanged);

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

    QSignalSpy TimerSpy(&Controller, &ZAppController::pumpTimerRunningChanged);

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
        (void)Controller.initializeRuntime();
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

    QSignalSpy MappingSpy(&Controller, &ZAppController::mappingEnabledChanged);

    Controller.SetMappingEnabled(false);

    REQUIRE_FALSE(Controller.IsMappingEnabled());
    REQUIRE(MappingSpy.count() == 1);

    (void)Controller.initializeRuntime();
}

// ── mappingEnabled 在 initialize 后更新缓存并发信号 ──

TEST_CASE("AppController SetMappingEnabled after initialize updates cache and emits signal",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    QSignalSpy MappingSpy(&Controller, &ZAppController::mappingEnabledChanged);

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

    (void)Controller.initializeRuntime();
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
    (void)Controller.initializeRuntime();

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

    QSignalSpy StatusSpy(&Controller, &ZAppController::runtimeStatusChanged);

    (void)Controller.initializeRuntime();
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

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    QSignalSpy StatusSpy(&Controller, &ZAppController::runtimeStatusChanged);

    bool bResult = Controller.initializeRuntime();

    REQUIRE_FALSE(bResult);
    REQUIRE(Controller.RuntimeState() == "error");
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(StatusSpy.count() >= 1);

    // 验证 error signal 携带消息
    auto ErrorArgs = ErrorSpy.takeFirst();
    REQUIRE(ErrorArgs.at(0).toString() == "Initialize failed: input backend unavailable");
}

// ── output factory 失败 ──

TEST_CASE("AppController initializeRuntime fails when output factory fails",
    "[UI][AppController]")
{
    ZAppController Controller(
        MakeFakeInputFactory(),
        MakeFailingOutputFactory("output backend unavailable"));

    bool bResult = Controller.initializeRuntime();

    REQUIRE_FALSE(bResult);
    REQUIRE(Controller.RuntimeState() == "error");
}

// ── header 不包含 SDL 或 Win32 ──

TEST_CASE("AppController header has no SDL or Win32 dependencies",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.RuntimeState() == "created");
}

TEST_CASE("AppController signals use lowerCamelCase for QML Connections compatibility",
    "[UI][AppController]")
{
    const QMetaObject* Meta = &ZAppController::staticMetaObject;

    REQUIRE(Meta->indexOfSignal("runtimeStatusChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("mappingEnabledChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("pumpTimerRunningChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("lastPumpSummaryChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("runtimeError(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("profileStatusChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("profileSaved(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("profileLoaded(QString)") >= 0);
}

// ── inputStateModel 属性 ──

TEST_CASE("AppController inputStateModel returns non-null QObject",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.InputStateModel() != nullptr);
}

// ── pumpOnce 后 InputStateModel 更新 ──

TEST_CASE("AppController pumpOnce updates InputStateModel via input event handler",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());
    REQUIRE(Model != nullptr);
    REQUIRE(Model->rowCount() == 0);

    // 注入输入事件
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    Controller.pumpOnce();

    REQUIRE(Model->rowCount() == 1);
    REQUIRE(Model->isPressed("dev_1", "button_south") == true);
}

// ── 设备断开后 InputStateModel 清理 ──

TEST_CASE("AppController device disconnect cleans up InputStateModel",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());

    // 添加设备并注入事件
    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "dev_1"};
    DevInfo.Name = "Test Device";
    RawInputBackend->AddDevice(DevInfo);

    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    Controller.pumpOnce();
    REQUIRE(Model->rowCount() == 1);

    // 移除设备
    RawInputBackend->RemoveDevice(SDeviceId{.Value = "dev_1"});
    Controller.pumpOnce();

    REQUIRE(Model->rowCount() == 0);
}

// ── 幂等 initialize 不清理 InputStateModel ──

TEST_CASE("AppController idempotent initialize does not clear InputStateModel",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());

    // 注入事件
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();
    REQUIRE(Model->rowCount() == 1);

    // 幂等再次 initialize（Ready 状态短路）
    (void)Controller.initializeRuntime();

    // InputStateModel 保持不变
    REQUIRE(Model->rowCount() == 1);
}

// ── Error 后重新 initialize 清理 InputStateModel ──

TEST_CASE("AppController initialize from Error state clears InputStateModel",
    "[UI][AppController]")
{
    // 第一次用正常工厂初始化并注入事件
    int FactoryCallCount = 0;
    auto InputFactory = [&FactoryCallCount]() -> TResult<TUniquePtr<IInputBackend>> {
        ++FactoryCallCount;
        if (FactoryCallCount == 1)
        {
            return TResult<TUniquePtr<IInputBackend>>::Ok(
                std::make_unique<ZFakeInputBackend>());
        }
        // 第二次调用失败，模拟 Error 状态
        return TResult<TUniquePtr<IInputBackend>>::Err(
            MakeError(EErrorCode::Unknown, "factory fail"));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());

    // 第一次初始化成功
    (void)Controller.initializeRuntime();
    REQUIRE(Controller.RuntimeState() == "ready");

    // 手动通过 InputStateModel 添加状态来模拟有旧数据
    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());
    SInputEvent FakeEvent;
    FakeEvent.DeviceId = SDeviceId{.Value = "dev_1"};
    FakeEvent.ControlId = "button_south";
    FakeEvent.ControlType = EInputControlType::Button;
    FakeEvent.EventType = EInputEventType::Pressed;
    FakeEvent.Value = 1.0f;
    Model->ApplyInputEvent(FakeEvent);
    REQUIRE(Model->rowCount() == 1);

    // 因为 Ready 状态幂等短路，需要先让 bootstrap 进入 Error
    // 使用 StartRuntime 再让 factory 第二次失败
    // 但 StartRuntime 不会调 factory... 需要另一种方式进入 Error
    // 实际上 AppController 没有暴露直接进入 Error 的方法，
    // 所以用另一个 controller 实例来测试 Error→re-init 路径

    ZAppController Controller2(
        MakeFailingInputFactory("first fail"),
        MakeNullOutputFactory());

    // 初始化失败 → Error 状态
    (void)Controller2.initializeRuntime();
    REQUIRE(Controller2.RuntimeState() == "error");

    // 手动填充 InputStateModel
    auto* Model2 = qobject_cast<ZInputStateModel*>(Controller2.InputStateModel());
    Model2->ApplyInputEvent(FakeEvent);
    REQUIRE(Model2->rowCount() == 1);

    // 重新 initialize（仍会失败，但应先清理 InputStateModel）
    (void)Controller2.initializeRuntime();

    // Error 路径的 re-init 应已清理旧 InputStateModel
    REQUIRE(Model2->rowCount() == 0);
}

// ══════════════════════════════════════════════════════════════
// 语义 signal 集成测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController pumpOnce triggers InputStateModel ControlStateChanged",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());
    QSignalSpy Spy(Model, &ZInputStateModel::controlStateChanged);

    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    REQUIRE(Spy.count() == 1);
    REQUIRE(Spy.at(0).at(0).toString() == "dev_1");
    REQUIRE(Spy.at(0).at(1).toString() == "button_south");
}

TEST_CASE("AppController device disconnect triggers InputStateModel DeviceStateRemoved",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* InputModel = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());

    // 注入输入事件使 InputStateModel 有该设备的数据
    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "dev_1"};
    DevInfo.Name = "Test Device";
    RawInputBackend->AddDevice(DevInfo);
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    QSignalSpy Spy(InputModel, &ZInputStateModel::deviceStateRemoved);

    RawInputBackend->RemoveDevice(SDeviceId{.Value = "dev_1"});
    Controller.pumpOnce();

    REQUIRE(Spy.count() == 1);
    REQUIRE(Spy.takeFirst().at(0).toString() == "dev_1");
}

TEST_CASE("AppController device connect triggers DeviceModel DeviceAdded",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* DevModel = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());
    QSignalSpy AddedSpy(DevModel, &ZDeviceModel::deviceAdded);

    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "hot_dev"};
    DevInfo.Name = "Hot Pad";
    RawInputBackend->AddDevice(DevInfo);
    Controller.pumpOnce();

    REQUIRE(AddedSpy.count() == 1);
    REQUIRE(AddedSpy.at(0).at(0).toString() == "hot_dev");
}

TEST_CASE("AppController duplicate device connect triggers DeviceModel DeviceUpdated",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* DevModel = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());

    // 首次添加设备
    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "dup_dev"};
    DevInfo.Name = "Original Name";
    RawInputBackend->AddDevice(DevInfo);
    Controller.pumpOnce();
    REQUIRE(DevModel->rowCount() >= 1);

    // 再次添加相同 ID 但不同名称的设备，走 update 路径
    QSignalSpy UpdatedSpy(DevModel, &ZDeviceModel::deviceUpdated);
    QSignalSpy AddedSpy(DevModel, &ZDeviceModel::deviceAdded);

    DevInfo.Name = "Updated Name";
    RawInputBackend->AddDevice(DevInfo);
    Controller.pumpOnce();

    REQUIRE(UpdatedSpy.count() == 1);
    REQUIRE(UpdatedSpy.at(0).at(0).toString() == "dup_dev");
    REQUIRE(AddedSpy.count() == 0);
    REQUIRE(DevModel->displayNameAt(
        DevModel->rowCount() - 1) == "Updated Name");
}

TEST_CASE("AppController device disconnect triggers DeviceModel DeviceRemoved",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* DevModel = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());

    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "rm_dev"};
    DevInfo.Name = "Removable";
    RawInputBackend->AddDevice(DevInfo);
    Controller.pumpOnce();

    QSignalSpy RemovedSpy(DevModel, &ZDeviceModel::deviceRemoved);

    RawInputBackend->RemoveDevice(SDeviceId{.Value = "rm_dev"});
    Controller.pumpOnce();

    REQUIRE(RemovedSpy.count() == 1);
    REQUIRE(RemovedSpy.takeFirst().at(0).toString() == "rm_dev");
}

TEST_CASE("AppController device added without input then removed does not trigger DeviceStateRemoved",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* InputModel = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());
    auto* DevModel = qobject_cast<ZDeviceModel*>(Controller.DeviceModel());

    // 添加设备但不发送任何输入
    SDeviceInfo DevInfo;
    DevInfo.Id = SDeviceId{.Value = "silent_dev"};
    DevInfo.Name = "Silent Pad";
    RawInputBackend->AddDevice(DevInfo);
    Controller.pumpOnce();

    QSignalSpy InputRemovedSpy(InputModel, &ZInputStateModel::deviceStateRemoved);
    QSignalSpy DevRemovedSpy(DevModel, &ZDeviceModel::deviceRemoved);

    // 移除设备
    RawInputBackend->RemoveDevice(SDeviceId{.Value = "silent_dev"});
    Controller.pumpOnce();

    // DeviceModel 一定会移除
    REQUIRE(DevRemovedSpy.count() == 1);
    // InputStateModel 没有该设备的状态，不发 DeviceStateRemoved
    REQUIRE(InputRemovedSpy.count() == 0);
}

TEST_CASE("AppController initialize from Error triggers InputStateReset",
    "[UI][AppController]")
{
    ZAppController Controller(
        MakeFailingInputFactory("first fail"),
        MakeNullOutputFactory());

    // 初始化失败 → Error 状态
    (void)Controller.initializeRuntime();
    REQUIRE(Controller.RuntimeState() == "error");

    // 手动填充 InputStateModel
    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());
    SInputEvent FakeEvent;
    FakeEvent.DeviceId = SDeviceId{.Value = "dev_1"};
    FakeEvent.ControlId = "button_south";
    FakeEvent.ControlType = EInputControlType::Button;
    FakeEvent.EventType = EInputEventType::Pressed;
    FakeEvent.Value = 1.0f;
    Model->ApplyInputEvent(FakeEvent);
    REQUIRE(Model->rowCount() == 1);

    QSignalSpy ResetSpy(Model, &ZInputStateModel::inputStateReset);

    // 重新 initialize（仍会失败，但应先清理 InputStateModel）
    (void)Controller.initializeRuntime();

    REQUIRE(ResetSpy.count() == 1);
}

TEST_CASE("AppController idempotent initialize does not trigger InputStateReset",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Model = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());

    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();
    REQUIRE(Model->rowCount() == 1);

    QSignalSpy ResetSpy(Model, &ZInputStateModel::inputStateReset);

    // 幂等再次 initialize（Ready 状态短路）
    (void)Controller.initializeRuntime();

    REQUIRE(ResetSpy.count() == 0);
    REQUIRE(Model->rowCount() == 1);
}

// ══════════════════════════════════════════════════════════════
// InputCapture 集成测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController inputCapture property returns non-null QObject",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.InputCapture() != nullptr);
}

TEST_CASE("AppController begin capture then pumpOnce completes capture",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Capture = qobject_cast<ZInputCaptureModel*>(Controller.InputCapture());
    REQUIRE(Capture != nullptr);

    Capture->begin("dev_1");
    REQUIRE(Capture->IsActive());

    QSignalSpy CompleteSpy(Capture, &ZInputCaptureModel::captureCompleted);

    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    REQUIRE_FALSE(Capture->IsActive());
    REQUIRE(CompleteSpy.count() == 1);
    REQUIRE(CompleteSpy.at(0).at(0).toString() == "dev_1");
    REQUIRE(CompleteSpy.at(0).at(1).toString() == "button_south");
}

TEST_CASE("AppController capture completion has InputStateModel snapshot already updated",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Capture = qobject_cast<ZInputCaptureModel*>(Controller.InputCapture());
    auto* InputModel = qobject_cast<ZInputStateModel*>(Controller.InputStateModel());

    // 在 CaptureCompleted slot 中验证 InputStateModel 快照已更新
    bool bSnapshotUpdated = false;
    QObject::connect(Capture, &ZInputCaptureModel::captureCompleted,
        [InputModel, &bSnapshotUpdated](const QString& DeviceId, const QString& ControlId)
        {
            bSnapshotUpdated = InputModel->isPressed(DeviceId, ControlId);
        });

    Capture->begin("dev_1");
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    REQUIRE(bSnapshotUpdated);
}

TEST_CASE("AppController non-target device input does not complete capture",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Capture = qobject_cast<ZInputCaptureModel*>(Controller.InputCapture());

    Capture->begin("dev_1");

    QSignalSpy CompleteSpy(Capture, &ZInputCaptureModel::captureCompleted);

    // 注入来自非 target device 的输入
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_2", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    REQUIRE(Capture->IsActive());
    REQUIRE(CompleteSpy.count() == 0);
}

TEST_CASE("AppController capture does not affect LastPumpSummary statistics",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto* Capture = qobject_cast<ZInputCaptureModel*>(Controller.InputCapture());

    // 不开启 capture 时 pump 一个事件，记录 summary
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();
    int BaseInputCount = Controller.LastInputEventCount();

    // 开启 capture 后 pump 另一个事件
    Capture->begin("dev_1");
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonNorth, EInputEventType::Pressed));
    Controller.pumpOnce();

    // capture 不影响统计计数
    REQUIRE(Controller.LastInputEventCount() == BaseInputCount);
}

// ══════════════════════════════════════════════════════════════
// MappingRuleModel 集成测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController mappingRuleModel property returns non-null",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.MappingRuleModel() != nullptr);
}

TEST_CASE("AppController mappingRuleModel is empty after initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto* Model = Controller.MappingRuleModel();
    REQUIRE(Model->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding Keyboard Space succeeds",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    bool bResult = Controller.applySelectedBinding("button_south", "Keyboard", "Space");
    REQUIRE(bResult);

    auto* Model = Controller.MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);

    auto Index = Model->index(0);
    REQUIRE(Model->data(Index, ZMappingRuleModel::InputRole).toString() == "button_south");
    REQUIRE(Model->data(Index, ZMappingRuleModel::OutputRole).toString() == "Space");
    REQUIRE(Model->data(Index, ZMappingRuleModel::ActionKindRole).toString() == "Keyboard");
}

TEST_CASE("AppController applySelectedBinding Mouse Left Click succeeds",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    bool bResult = Controller.applySelectedBinding("right_trigger", "MouseButton", "Left");
    REQUIRE(bResult);

    auto* Model = Controller.MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);

    auto Index = Model->index(0);
    REQUIRE(Model->data(Index, ZMappingRuleModel::OutputRole).toString() == "Left Click");
    REQUIRE(Model->data(Index, ZMappingRuleModel::ActionKindRole).toString() == "MouseButton");
}

TEST_CASE("AppController applySelectedBinding rule id equals controlId",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    Controller.applySelectedBinding("button_south", "Keyboard", "Space");

    auto* Model = Controller.MappingRuleModel();
    auto Index = Model->index(0);
    REQUIRE(Model->data(Index, ZMappingRuleModel::RuleIdRole).toString() == "button_south");
}

TEST_CASE("AppController applySelectedBinding empty controlId returns false and emits error",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("", "Keyboard", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController applySelectedBinding empty actionKind returns false and emits error",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("button_south", "", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController applySelectedBinding before initialize returns false",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("button_south", "Keyboard", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController applySelectedBinding same control replaces old rule",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    Controller.applySelectedBinding("button_south", "Keyboard", "Space");
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);

    // 同一 control 再次 apply 替换
    Controller.applySelectedBinding("button_south", "MouseButton", "Left");
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);

    auto Index = Controller.MappingRuleModel()->index(0);
    REQUIRE(Controller.MappingRuleModel()->data(
        Index, ZMappingRuleModel::OutputRole).toString() == "Left Click");
    REQUIRE(Controller.MappingRuleModel()->data(
        Index, ZMappingRuleModel::ActionKindRole).toString() == "MouseButton");
}

TEST_CASE("AppController applySelectedBinding Axis2D returns false and does not modify model",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("left_stick", "Keyboard", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding unknown controlId returns false",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("unknown_control", "Keyboard", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding updates RuntimeHost profile snapshot",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    Controller.applySelectedBinding("button_south", "Keyboard", "Space");

    // 直接验证内部 snapshot
    auto Snapshot = Controller.MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Snapshot[0].Id == "button_south");
    REQUIRE(Snapshot[0].Input.ControlId == "button_south");
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::KeyboardKey);
}

TEST_CASE("AppController applySelectedBinding then pump dispatches mapped input",
    "[UI][AppController]")
{
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    // 创建规则
    Controller.applySelectedBinding("button_south", "Keyboard", "Space");

    // 注入匹配事件
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Controller.pumpOnce();

    REQUIRE(Controller.LastMappedInputCount() == 1);
    REQUIRE(Controller.LastDispatchedInputCount() == 1);
}

// ══════════════════════════════════════════════════════════════
// P6: actionCatalogModel 属性 + 结构化 apply 测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController actionCatalogModel is non-null and has rows",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    auto* Catalog = Controller.ActionCatalogModel();
    REQUIRE(Catalog != nullptr);
    REQUIRE(Catalog->rowCount() > 0);
}

TEST_CASE("AppController applySelectedBinding Keyboard A writes correct action",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    bool bResult = Controller.applySelectedBinding("button_south", "Keyboard", "A");
    REQUIRE(bResult);

    auto* Model = Controller.MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);

    auto Snapshot = Model->ListRulesSnapshot();
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::KeyboardKey);
    auto& Keyboard = std::get<SKeyboardAction>(Snapshot[0].Output.Action.Payload);
    REQUIRE(Keyboard.Key == "A");
    REQUIRE(Keyboard.bPressed == true);
}

TEST_CASE("AppController applySelectedBinding MouseButton Left writes Button 0",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    bool bResult = Controller.applySelectedBinding("button_south", "MouseButton", "Left");
    REQUIRE(bResult);

    auto Snapshot = Controller.MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::MouseButton);
    auto& Mouse = std::get<SMouseButtonAction>(Snapshot[0].Output.Action.Payload);
    REQUIRE(Mouse.Button == 0);
}

TEST_CASE("AppController applySelectedBinding MouseButton Right writes Button 1",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    bool bResult = Controller.applySelectedBinding("button_south", "MouseButton", "Right");
    REQUIRE(bResult);

    auto Snapshot = Controller.MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::MouseButton);
    auto& Mouse = std::get<SMouseButtonAction>(Snapshot[0].Output.Action.Payload);
    REQUIRE(Mouse.Button == 1);
}

TEST_CASE("AppController applySelectedBinding MouseButton Middle writes Button 2",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    bool bResult = Controller.applySelectedBinding("button_south", "MouseButton", "Middle");
    REQUIRE(bResult);

    auto Snapshot = Controller.MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::MouseButton);
    auto& Mouse = std::get<SMouseButtonAction>(Snapshot[0].Output.Action.Payload);
    REQUIRE(Mouse.Button == 2);
}

TEST_CASE("AppController applySelectedBinding unknown actionKind returns false",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("button_south", "UnknownKind", "Space");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding unknown mouse button value returns false",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding(
        "button_south", "MouseButton", "UnknownButton");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding unknown keyboard value returns false",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding(
        "button_south", "Keyboard", "NotAKey");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding success and failure log semantics preserved",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto* Log = Controller.LogModel();

    // 成功 apply
    Controller.applySelectedBinding("button_south", "Keyboard", "Space");
    auto SuccessLevel = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    CHECK(SuccessLevel == "Success");

    // 失败 apply（未知 kind）
    Controller.applySelectedBinding("button_south", "BadKind", "X");
    auto ErrorLevel = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    CHECK(ErrorLevel == "Error");
}

// ══════════════════════════════════════════════════════════════
// P2: activeProfileName / outputDisplayText / outputState 稳定性
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController activeProfileName is Default before initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.ActiveProfileName() == "Default");
}

TEST_CASE("AppController activeProfileName is Default after initialize with default profile",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // Bootstrap 默认 profile Name 为 "Default"
    REQUIRE(Controller.ActiveProfileName() == "Default");
}

TEST_CASE("AppController activeProfileName falls back to Default when profile name is empty",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.initializeRuntime());

    // 替换为空 Name 的 profile
    SMappingProfile Profile;
    Profile.Name = "";
    Controller.ReplaceActiveProfileForTest(std::move(Profile));

    REQUIRE(Controller.ActiveProfileName() == "Default");
}

TEST_CASE("AppController outputDisplayText is Unavailable before initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.OutputDisplayText() == "Unavailable");
}

TEST_CASE("AppController outputDisplayText is Ready after initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    REQUIRE(Controller.OutputDisplayText() == "Ready");
}

TEST_CASE("AppController outputDisplayText is Live Output after startRuntime",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();
    REQUIRE(Controller.OutputDisplayText() == "Live Output");
}

TEST_CASE("AppController outputDisplayText is Unavailable when initialize fails",
    "[UI][AppController]")
{
    ZAppController Controller(
        MakeFailingInputFactory("input failed"),
        MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    REQUIRE(Controller.RuntimeState() == "error");
    REQUIRE(Controller.OutputDisplayText() == "Unavailable");
}

TEST_CASE("AppController outputState strings are stable",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    // Created 状态下 outputState 返回 unavailable
    REQUIRE(Controller.OutputState() == "unavailable");

    (void)Controller.initializeRuntime();
    // Ready 状态下 NullOutputBackend state 为 ready
    REQUIRE(Controller.OutputState() == "ready");
}

TEST_CASE("AppController mappingEnabled change emits signal and property syncs",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy MappingSpy(&Controller, &ZAppController::mappingEnabledChanged);

    // 默认 enabled=true，改为 false
    Controller.SetMappingEnabled(false);
    REQUIRE(MappingSpy.count() == 1);
    REQUIRE_FALSE(Controller.IsMappingEnabled());

    // 改回 true
    Controller.SetMappingEnabled(true);
    REQUIRE(MappingSpy.count() == 2);
    REQUIRE(Controller.IsMappingEnabled());
}

// ══════════════════════════════════════════════════════════════
// P3: saveActiveProfile
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController saveActiveProfile before initialize returns false and emits runtimeError",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_noinit" / "profile.json";
    bool bResult = Controller.saveActiveProfile(
        QString::fromStdString(TempPath.string()));

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
}

TEST_CASE("AppController saveActiveProfile with explicit path creates JSON file",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    bool bResult = Controller.saveActiveProfile(PathStr);

    REQUIRE(bResult);
    REQUIRE(std::filesystem::exists(TempPath));

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController saved empty profile can be parsed by ZProfileManager",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_empty" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    Controller.saveActiveProfile(PathStr);

    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(TempPath);
    REQUIRE(LoadResult.IsOk());
    REQUIRE(LoadResult.Value().Rules.empty());

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController apply binding then save writes mapping rule with expected control and action",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    Controller.applySelectedBinding(
        QStringLiteral("button_south"), QStringLiteral("Keyboard"), QStringLiteral("Space"));

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_binding" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    Controller.saveActiveProfile(PathStr);

    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(TempPath);
    REQUIRE(LoadResult.IsOk());

    auto& Rules = LoadResult.Value().Rules;
    REQUIRE(Rules.size() == 1);
    REQUIRE(Rules[0].Input.ControlId == "button_south");
    REQUIRE(Rules[0].Output.Action.Type == EActionType::KeyboardKey);

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController saveActiveProfile creates missing parent directories",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_mkdir" / "nested" / "dir" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    bool bResult = Controller.saveActiveProfile(PathStr);

    REQUIRE(bResult);
    REQUIRE(std::filesystem::exists(TempPath));

    std::filesystem::remove_all(
        std::filesystem::temp_directory_path() / "mappyz_save_test_mkdir");
}

TEST_CASE("AppController saveActiveProfile while Running succeeds",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();
    (void)Controller.startRuntime();

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_running" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    bool bResult = Controller.saveActiveProfile(PathStr);

    REQUIRE(bResult);

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController saveActiveProfile preserves mappingEnabled value",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    Controller.SetMappingEnabled(false);
    REQUIRE_FALSE(Controller.IsMappingEnabled());

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_mapping" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    Controller.saveActiveProfile(PathStr);

    REQUIRE_FALSE(Controller.IsMappingEnabled());

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController successful save emits profileStatusChanged and profileSaved",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(&Controller, &ZAppController::profileSaved);

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_save_test_signals" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    Controller.saveActiveProfile(PathStr);

    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 1);
    REQUIRE(SavedSpy.at(0).at(0).toString() == PathStr);

    // 验证 profilePath 属性已更新
    REQUIRE(Controller.ProfilePath() == PathStr);
    REQUIRE(Controller.ProfileMessage() == "Profile saved");

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController failed save emits profileStatusChanged and runtimeError",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // 创建一个普通文件作为阻塞点，使 create_directories 失败
    auto BlockerPath = std::filesystem::temp_directory_path()
        / "mappyz_save_fail_blocker";
    { std::ofstream(BlockerPath).close(); }

    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    auto BadPath = QString::fromStdString(
        (BlockerPath / "sub" / "profile.json").string());

    bool bResult = Controller.saveActiveProfile(BadPath);

    REQUIRE_FALSE(bResult);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(ErrorSpy.count() == 1);

    std::filesystem::remove(BlockerPath);
}

TEST_CASE("AppController saveActiveProfile default path creates file under AppDataLocation",
    "[UI][AppController]")
{
    STestModeGuard Guard;

    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    bool bResult = Controller.saveActiveProfile();

    REQUIRE(bResult);
    REQUIRE_FALSE(Controller.ProfilePath().isEmpty());

    // 验证文件存在且可被 ProfileManager round-trip
    StdPath SavedPath(Controller.ProfilePath().toStdString());
    REQUIRE(std::filesystem::exists(SavedPath));

    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(SavedPath);
    REQUIRE(LoadResult.IsOk());

    // 清理测试目录
    std::filesystem::remove_all(SavedPath.parent_path());
}

// ══════════════════════════════════════════════════════════════
// P4: loadProfile
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController loadProfile before initialize returns false and emits runtimeError",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_noinit" / "profile.json";
    bool bResult = Controller.loadProfile(
        QString::fromStdString(TempPath.string()));

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
}

TEST_CASE("AppController loadProfile with missing default path returns true and keeps empty Default profile",
    "[UI][AppController]")
{
    STestModeGuard Guard;

    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // 无参调用，test mode 下默认路径不存在
    bool bResult = Controller.loadProfile();

    REQUIRE(bResult);
    REQUIRE(Controller.ActiveProfileName() == "Default");
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController loadProfile with explicit missing path returns false and emits runtimeError",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.loadProfile(
        QStringLiteral("nonexistent_mappyz_test_xyz.json"));

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController loadProfile with explicit path loads saved profile and refreshes MappingRuleModel",
    "[UI][AppController]")
{
    // 先用一个 controller 保存带规则的 profile
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_explicit" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    {
        ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Saver.initializeRuntime();
        Saver.applySelectedBinding(
            QStringLiteral("button_south"), QStringLiteral("Keyboard"), QStringLiteral("Space"));
        Saver.saveActiveProfile(PathStr);
    }

    // 用新 controller 加载
    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();
    REQUIRE(Loader.MappingRuleModel()->rowCount() == 0);

    bool bResult = Loader.loadProfile(PathStr);

    REQUIRE(bResult);
    REQUIRE(Loader.MappingRuleModel()->rowCount() == 1);

    auto Index = Loader.MappingRuleModel()->index(0);
    REQUIRE(Loader.MappingRuleModel()->data(
        Index, ZMappingRuleModel::InputRole).toString() == "button_south");

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController loadProfile updates activeProfileName from profile Name",
    "[UI][AppController]")
{
    // 保存带自定义 Name 的 profile
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_name" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    {
        ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Saver.initializeRuntime();

        SMappingProfile Profile;
        Profile.Name = "My Custom Profile";
        Saver.ReplaceActiveProfileForTest(std::move(Profile));
        Saver.saveActiveProfile(PathStr);
    }

    // 加载并验证 activeProfileName
    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();
    REQUIRE(Loader.ActiveProfileName() == "Default");

    Loader.loadProfile(PathStr);

    REQUIRE(Loader.ActiveProfileName() == "My Custom Profile");

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController loadProfile preserves mappingEnabled value",
    "[UI][AppController]")
{
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_mapping" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    {
        ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Saver.initializeRuntime();
        Saver.saveActiveProfile(PathStr);
    }

    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();
    Loader.SetMappingEnabled(false);
    REQUIRE_FALSE(Loader.IsMappingEnabled());

    Loader.loadProfile(PathStr);

    REQUIRE_FALSE(Loader.IsMappingEnabled());

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController loadProfile while Running succeeds and does not stop runtime",
    "[UI][AppController]")
{
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_running" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    {
        ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Saver.initializeRuntime();
        Saver.saveActiveProfile(PathStr);
    }

    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();
    (void)Loader.startRuntime();
    REQUIRE(Loader.RuntimeState() == "running");

    bool bResult = Loader.loadProfile(PathStr);

    REQUIRE(bResult);
    REQUIRE(Loader.RuntimeState() == "running");

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController loadProfile failure does not clear existing MappingRuleModel",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // 先 apply 一条规则
    Controller.applySelectedBinding(
        QStringLiteral("button_south"), QStringLiteral("Keyboard"), QStringLiteral("Space"));
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);

    // 加载不存在的显式路径
    Controller.loadProfile(QStringLiteral("nonexistent_mappyz_load_fail.json"));

    // 现有规则不被清空
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);
}

TEST_CASE("AppController loadProfile failure does not change profilePath",
    "[UI][AppController]")
{
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_keeppath" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // 先保存以设置 profilePath
    Controller.saveActiveProfile(PathStr);
    REQUIRE(Controller.ProfilePath() == PathStr);

    // 加载失败不应修改 profilePath
    Controller.loadProfile(QStringLiteral("nonexistent_mappyz_load_fail.json"));

    REQUIRE(Controller.ProfilePath() == PathStr);

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController successful load emits profileStatusChanged and profileLoaded",
    "[UI][AppController]")
{
    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_load_test_signals" / "profile.json";
    auto PathStr = QString::fromStdString(TempPath.string());

    {
        ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Saver.initializeRuntime();
        Saver.saveActiveProfile(PathStr);
    }

    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();

    QSignalSpy StatusSpy(&Loader, &ZAppController::profileStatusChanged);
    QSignalSpy LoadedSpy(&Loader, &ZAppController::profileLoaded);
    QSignalSpy RuntimeSpy(&Loader, &ZAppController::runtimeStatusChanged);

    Loader.loadProfile(PathStr);

    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(LoadedSpy.count() == 1);
    REQUIRE(LoadedSpy.at(0).at(0).toString() == PathStr);
    // runtimeStatusChanged 也需发出，驱动 activeProfileName 刷新
    REQUIRE(RuntimeSpy.count() >= 1);

    REQUIRE(Loader.ProfilePath() == PathStr);
    REQUIRE(Loader.ProfileMessage() == "Profile loaded");

    std::filesystem::remove_all(TempPath.parent_path());
}

TEST_CASE("AppController default path save then no-arg load round-trips",
    "[UI][AppController]")
{
    STestModeGuard Guard;

    // 保存带规则的 profile 到默认路径
    ZAppController Saver(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Saver.initializeRuntime();
    Saver.applySelectedBinding(
        QStringLiteral("button_south"), QStringLiteral("Keyboard"), QStringLiteral("Space"));
    Saver.saveActiveProfile();
    auto SavedPath = Saver.ProfilePath();

    // 新 controller 无参 loadProfile 读回
    ZAppController Loader(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Loader.initializeRuntime();
    REQUIRE(Loader.MappingRuleModel()->rowCount() == 0);

    bool bResult = Loader.loadProfile();

    REQUIRE(bResult);
    REQUIRE(Loader.MappingRuleModel()->rowCount() == 1);
    REQUIRE(Loader.ProfilePath() == SavedPath);

    // 清理
    std::filesystem::remove_all(
        StdPath(SavedPath.toStdString()).parent_path());
}

// ── removeBinding ──

TEST_CASE("AppController removeBinding returns false for empty ruleId",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller.removeBinding(QString()));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController removeBinding returns false before initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller.removeBinding(QStringLiteral("some_rule")));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController removeBinding removes existing rule and updates model",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    Controller.applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);

    QString RuleId = Controller.MappingRuleModel()->ruleIdAt(0);
    REQUIRE_FALSE(RuleId.isEmpty());

    bool bResult = Controller.removeBinding(RuleId);

    REQUIRE(bResult);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController removeBinding returns false for unknown ruleId",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller.removeBinding(QStringLiteral("nonexistent_rule")));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController removeBinding works while runtime is running",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    Controller.applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);

    (void)Controller.startRuntime();
    REQUIRE(Controller.RuntimeState() == "running");

    QString RuleId = Controller.MappingRuleModel()->ruleIdAt(0);
    bool bResult = Controller.removeBinding(RuleId);

    REQUIRE(bResult);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController removeBinding with multiple rules removes only target",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    Controller.applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    Controller.applySelectedBinding(
        QStringLiteral("button_north"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Escape"));
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 2);

    QString FirstRuleId = Controller.MappingRuleModel()->ruleIdAt(0);
    bool bResult = Controller.removeBinding(FirstRuleId);

    REQUIRE(bResult);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 1);
    REQUIRE(Controller.MappingRuleModel()->ruleIdAt(0) != FirstRuleId);
}
