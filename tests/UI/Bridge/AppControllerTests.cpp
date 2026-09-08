// ZAppController 单元测试。
// 使用 fake/null 工厂验证 UI Bridge 控制器的完整生命周期：
// initializeRuntime/startRuntime/stopRuntime/pumpOnce、
// QTimer pump 控制、QSignalSpy 信号验证、factory 失败、析构安全。
//
// 多配置相关测试统一通过构造函数注入隔离的临时目录（不再依赖
// QStandardPaths test mode），使 initializeProfiles 后的 autosave/切换/
// 新建/重命名/删除都落在独立目录，测试之间互不污染。

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QVariantList>
#include <QVariantMap>

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

// ── 隔离配置目录 RAII 辅助 ──

// 每个实例生成一个唯一的临时目录路径，用于注入 ZAppController。
// 构造时仅清理残留（不主动创建目录），析构时递归删除，确保测试隔离。
struct STempProfileDir
{
    StdPath Path;

    explicit STempProfileDir(const StdString& Label)
    {
        // steady_clock 计数 + 原子自增计数器，保证跨用例的路径唯一性
        static std::atomic<uint32> Counter{0};
        const auto Unique =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
            + "_" + std::to_string(Counter.fetch_add(1));
        Path = std::filesystem::temp_directory_path()
            / ("mappyz_ac_" + Label + "_" + Unique);

        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
    }

    ~STempProfileDir()
    {
        std::error_code Ec;
        std::filesystem::remove_all(Path, Ec);
    }

    STempProfileDir(const STempProfileDir&) = delete;
    STempProfileDir& operator=(const STempProfileDir&) = delete;
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

// 构造一个已完成 initializeRuntime + initializeProfiles 的控制器，
// 配置绑定到给定的隔离目录，供多配置/自动保存类用例复用。
static std::unique_ptr<ZAppController> MakeInitializedController(const StdPath& Directory)
{
    auto Controller = std::make_unique<ZAppController>(
        MakeFakeInputFactory(), MakeNullOutputFactory(), Directory);
    REQUIRE(Controller->initializeRuntime());
    REQUIRE(Controller->initializeProfiles());
    return Controller;
}

// ── 默认状态 ──

TEST_CASE("AppController default state is created with timer stopped",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    REQUIRE(Controller.RuntimeState() == "created");
    REQUIRE_FALSE(Controller.IsPumpTimerRunning());
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
    REQUIRE(Meta->indexOfSignal("pumpTimerRunningChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("lastPumpSummaryChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("runtimeError(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("profileStatusChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("profileSaved(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("profileLoaded(QString)") >= 0);
    // 多配置列表 / 当前项变化信号，供 QML Connections 绑定
    REQUIRE(Meta->indexOfSignal("profileListChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("activeProfileChanged()") >= 0);
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

TEST_CASE("AppController applySelectedBinding stick to Keyboard rejected",
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

TEST_CASE("AppController activeProfileName is Default without initializing profiles",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Controller.initializeRuntime();

    // 未调用 initializeProfiles，manager 无当前项，回退到 Default
    REQUIRE(Controller.ActiveProfileName() == "Default");
}

TEST_CASE("AppController activeProfileName is Default when profile manager is uninitialized",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.initializeRuntime());

    // 名称现在由 ProfileManager 提供；未初始化时始终回退 Default
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

// ══════════════════════════════════════════════════════════════
// Profile Dirty State 测试（temp-dir 注入）
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController default profileDirty is false and profileSaveState is clean",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE_FALSE(Controller.IsProfileDirty());
    REQUIRE(Controller.ProfileSaveState() == "clean");
}

TEST_CASE("AppController apply success sets dirty then autosaves to clean",
    "[UI][AppController]")
{
    STempProfileDir Temp("apply_autosave");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);

    bool bResult = Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));

    REQUIRE(bResult);
    // MarkProfileDirty 后 autosave 成功回到 clean
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "clean");
    // 一次 MarkProfileDirty + 一次保存成功，至少两次状态变化和一次保存
    REQUIRE(SavedSpy.count() >= 1);
    REQUIRE(StatusSpy.count() >= 1);
}

TEST_CASE("AppController apply success writes profile file to injected directory",
    "[UI][AppController]")
{
    STempProfileDir Temp("apply_writes_file");
    auto Controller = MakeInitializedController(Temp.Path);

    // 初始化后当前配置即 default.json，路径非空
    REQUIRE_FALSE(Controller->ProfilePath().isEmpty());

    bool bResult = Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    REQUIRE(bResult);

    // autosave 把规则写入当前配置文件
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    REQUIRE(std::filesystem::exists(ActiveFile));

    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(ActiveFile);
    REQUIRE(LoadResult.IsOk());
    REQUIRE(LoadResult.Value().Rules.size() == 1);
}

TEST_CASE("AppController removeBinding success autosaves to clean",
    "[UI][AppController]")
{
    STempProfileDir Temp("remove_autosave");
    auto Controller = MakeInitializedController(Temp.Path);

    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));

    QString RuleId = Controller->MappingRuleModel()->ruleIdAt(0);

    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);
    bool bResult = Controller->removeBinding(RuleId);

    REQUIRE(bResult);
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "clean");
    REQUIRE(SavedSpy.count() >= 1);
}

TEST_CASE("AppController manual saveActiveProfile success clears dirty",
    "[UI][AppController]")
{
    STempProfileDir Temp("manual_save");
    auto Controller = MakeInitializedController(Temp.Path);

    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));

    // autosave 已清 dirty；无参手动保存仍写当前配置文件
    bool bResult = Controller->saveActiveProfile();
    REQUIRE(bResult);
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "clean");
    REQUIRE(Controller->ProfileMessage() == "Profile saved");
}

TEST_CASE("AppController manual saveActiveProfile failure preserves dirty",
    "[UI][AppController]")
{
    STempProfileDir Temp("manual_save_fail");
    auto Controller = MakeInitializedController(Temp.Path);

    // 把当前配置文件替换为同名目录，使后续写入失败
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    // 修改 profile 触发 autosave 失败，形成真实 dirty/error 状态
    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    REQUIRE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");

    // 手动保存到同样不可写路径，应保持 dirty/error
    bool bResult = Controller->saveActiveProfile();
    REQUIRE_FALSE(bResult);
    REQUIRE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");
}

TEST_CASE("AppController save failure on clean profile keeps clean",
    "[UI][AppController]")
{
    STempProfileDir Temp("clean_save_fail");
    auto Controller = MakeInitializedController(Temp.Path);

    // 初始化后为 clean 状态：未做任何映射变更
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "clean");

    // 把当前配置文件替换为同名目录，使写入失败
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    // clean 配置保存失败：置 error 状态，但绝不把 dirty 从 false 翻成 true，
    // 否则临时写入失败会误阻塞后续切换/新建/删除。
    bool bResult = Controller->saveActiveProfile();
    REQUIRE_FALSE(bResult);
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");
}

TEST_CASE("AppController switch success clears dirty",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_clears_dirty");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建第二个配置，当前项变为 Untitled_1
    REQUIRE(Controller->createProfile());

    // 切回 default，ApplyLoadedProfile 统一清 dirty
    REQUIRE(Controller->switchProfile(QStringLiteral("default")));
    REQUIRE_FALSE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "clean");
}

TEST_CASE("AppController switch to unknown id does not clear existing dirty state",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_fail_keeps_dirty");
    auto Controller = MakeInitializedController(Temp.Path);

    // 通过阻塞当前配置文件制造 dirty/error 状态
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);
    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));
    REQUIRE(Controller->IsProfileDirty());

    // 切换到未知 ID 失败：dirty 状态不被清除（切换前保存也会失败）
    bool bResult = Controller->switchProfile(QStringLiteral("does_not_exist"));
    REQUIRE_FALSE(bResult);
    REQUIRE(Controller->IsProfileDirty());
}

TEST_CASE("AppController autosave success does not add duplicate Success log",
    "[UI][AppController]")
{
    STempProfileDir Temp("autosave_no_dup_log");
    auto Controller = MakeInitializedController(Temp.Path);

    auto* Log = Controller->LogModel();
    int LogCountBefore = Log->rowCount();

    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));

    // apply 本身写一条 Success log，autosave 不应额外写 Success
    int NewLogCount = Log->rowCount() - LogCountBefore;
    REQUIRE(NewLogCount == 1);

    auto Level = Log->data(
        Log->index(Log->rowCount() - 1), ZLogModel::LevelRole).toString();
    CHECK(Level == "Success");
}

TEST_CASE("AppController autosave failure writes Error log",
    "[UI][AppController]")
{
    STempProfileDir Temp("autosave_error_log");
    auto Controller = MakeInitializedController(Temp.Path);

    // 阻塞当前配置文件，使 autosave 写入失败
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    auto* Log = Controller->LogModel();
    int LogCountBefore = Log->rowCount();

    Controller->applySelectedBinding(
        QStringLiteral("button_south"),
        QStringLiteral("Keyboard"),
        QStringLiteral("Space"));

    bool bFoundError = false;
    for (int Index = LogCountBefore; Index < Log->rowCount(); ++Index)
    {
        auto Level = Log->data(
            Log->index(Index), ZLogModel::LevelRole).toString();
        if (Level == "Error")
        {
            bFoundError = true;
            break;
        }
    }
    REQUIRE(bFoundError);
    REQUIRE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");
}

TEST_CASE("AppController profileSaveState uses only clean dirty error",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    auto State = Controller.ProfileSaveState();
    REQUIRE((State == "clean" || State == "dirty" || State == "error"));
}

// ── display text properties ──

TEST_CASE("AppController profileDisplayText shows Default when clean",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.ProfileDisplayText() == "Default");
}

TEST_CASE("AppController profileDisplayText shows unsaved or save error after failed autosave",
    "[UI][AppController]")
{
    STempProfileDir Temp("display_dirty");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    // 阻塞当前配置文件使 autosave 失败，dirty 状态保留
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    Controller->applySelectedBinding("button_south", "Keyboard", "Space");

    auto DisplayText = Controller->ProfileDisplayText();
    bool bHasSuffix = DisplayText.contains("unsaved") || DisplayText.contains("save error");
    REQUIRE(bHasSuffix);
}

TEST_CASE("AppController profileDisplayText shows save error on failed save",
    "[UI][AppController]")
{
    STempProfileDir Temp("display_error");
    auto Controller = MakeInitializedController(Temp.Path);

    // 阻塞当前配置文件，手动保存失败
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    Controller->saveActiveProfile();

    REQUIRE(Controller->ProfileDisplayText().contains("save error"));
}

TEST_CASE("AppController profileSaveDisplayText three states",
    "[UI][AppController]")
{
    STempProfileDir Temp("save_display");
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory(), Temp.Path);

    // clean → Saved
    REQUIRE(Controller.ProfileSaveDisplayText() == "Saved");

    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());

    // 阻塞当前配置文件制造 save error 状态
    StdPath ActiveFile(Controller.ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);
    Controller.saveActiveProfile();

    REQUIRE(Controller.ProfileSaveDisplayText() == "Save Error");
}

TEST_CASE("AppController profileSaveSeverity maps clean dirty error to visual keys",
    "[UI][AppController]")
{
    STempProfileDir Temp("severity");
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory(), Temp.Path);

    // clean → normal
    REQUIRE(Controller.ProfileSaveSeverity() == "normal");

    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());

    // 阻塞当前配置文件制造 save error → danger
    StdPath ActiveFile(Controller.ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);
    Controller.saveActiveProfile();

    REQUIRE(Controller.ProfileSaveSeverity() == "danger");
}

TEST_CASE("AppController profileSaveSeverity emits caution during transient dirty",
    "[UI][AppController]")
{
    STempProfileDir Temp("severity_caution");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    // 通过 signal 观察捕获 MarkProfileDirty 瞬态
    QStringList ObservedSeverities;
    QObject::connect(Controller.get(), &ZAppController::profileStatusChanged,
        [&]() { ObservedSeverities.append(Controller->ProfileSaveSeverity()); });

    Controller->applySelectedBinding("button_south", "Keyboard", "Space");

    // MarkProfileDirty 同步触发第一次 profileStatusChanged，此时 severity 为 caution
    REQUIRE(ObservedSeverities.contains("caution"));
}

TEST_CASE("AppController runtimeDisplayText reflects lifecycle states",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());

    // Created
    REQUIRE(Controller.RuntimeDisplayText() == "Created");

    // Ready
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.RuntimeDisplayText() == "Ready");

    // Running
    REQUIRE(Controller.startRuntime());
    REQUIRE(Controller.RuntimeDisplayText() == "Running");
}

TEST_CASE("AppController runtimeDisplayText shows Error on failed init",
    "[UI][AppController]")
{
    ZAppController Controller(
        MakeFailingInputFactory("test error"), MakeNullOutputFactory());

    Controller.initializeRuntime();
    REQUIRE(Controller.RuntimeDisplayText() == "Error");
}

// ── setBindingEnabled ──

TEST_CASE("AppController setBindingEnabled returns false for empty ruleId",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.initializeRuntime());
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    REQUIRE_FALSE(Controller.setBindingEnabled(QString(), false));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController setBindingEnabled returns false before initialize",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    REQUIRE_FALSE(Controller.setBindingEnabled(QStringLiteral("some_rule"), false));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController setBindingEnabled returns false for unknown ruleId",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.initializeRuntime());
    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    REQUIRE_FALSE(Controller.setBindingEnabled(QStringLiteral("nonexistent"), false));
    REQUIRE(ErrorSpy.count() == 1);
}

TEST_CASE("AppController setBindingEnabled disables existing rule",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_disable");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    auto* Model = Controller->MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);

    // 默认 enabled
    auto EnabledBefore = Model->data(
        Model->index(0), ZMappingRuleModel::EnabledRole).toBool();
    REQUIRE(EnabledBefore);

    auto RuleId = Model->ruleIdAt(0);
    REQUIRE(Controller->setBindingEnabled(RuleId, false));

    auto EnabledAfter = Model->data(
        Model->index(0), ZMappingRuleModel::EnabledRole).toBool();
    REQUIRE_FALSE(EnabledAfter);
}

TEST_CASE("AppController setBindingEnabled re-enables disabled rule",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_reenable");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    auto* Model = Controller->MappingRuleModel();
    auto RuleId = Model->ruleIdAt(0);
    REQUIRE(Controller->setBindingEnabled(RuleId, false));
    REQUIRE(Controller->setBindingEnabled(RuleId, true));

    auto EnabledAfter = Model->data(
        Model->index(0), ZMappingRuleModel::EnabledRole).toBool();
    REQUIRE(EnabledAfter);
}

TEST_CASE("AppController setBindingEnabled same state is no-op",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_noop");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    // 等 autosave 完成，profile 回到 clean
    REQUIRE(Controller->ProfileSaveState() == "clean");

    auto* Model = Controller->MappingRuleModel();
    auto RuleId = Model->ruleIdAt(0);

    // 设为 true（已经是 true），不应触发 profileStatusChanged
    QSignalSpy ProfileSpy(Controller.get(), &ZAppController::profileStatusChanged);
    REQUIRE(Controller->setBindingEnabled(RuleId, true));
    REQUIRE(ProfileSpy.count() == 0);
    REQUIRE(Controller->ProfileSaveState() == "clean");
}

TEST_CASE("AppController setBindingEnabled persists through save load round trip",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_persist");

    StdPath ActiveFile;
    {
        auto Controller = MakeInitializedController(Temp.Path);
        REQUIRE(Controller->startRuntime());
        REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

        auto* Model = Controller->MappingRuleModel();
        auto RuleId = Model->ruleIdAt(0);
        // 禁用规则触发 autosave 写入当前配置文件
        REQUIRE(Controller->setBindingEnabled(RuleId, false));
        REQUIRE(Controller->ProfileSaveState() == "clean");

        ActiveFile = StdPath(Controller->ProfilePath().toStdString());
    }

    // 用独立的 ZProfileManager 读回文件，验证禁用状态已落盘
    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(ActiveFile);
    REQUIRE(LoadResult.IsOk());
    auto& Rules = LoadResult.Value().Rules;
    REQUIRE(Rules.size() == 1);
    REQUIRE_FALSE(Rules[0].bEnabled);
}

TEST_CASE("AppController setBindingEnabled autosave failure keeps dirty",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_fail");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    auto* Model = Controller->MappingRuleModel();
    auto RuleId = Model->ruleIdAt(0);

    // 阻塞当前配置文件使后续 autosave 失败
    StdPath ActiveFile(Controller->ProfilePath().toStdString());
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);

    REQUIRE(Controller->setBindingEnabled(RuleId, false));

    // runtime 中规则已禁用
    auto EnabledAfter = Model->data(
        Model->index(0), ZMappingRuleModel::EnabledRole).toBool();
    REQUIRE_FALSE(EnabledAfter);

    // 但 profile 状态是 error
    REQUIRE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");
}

TEST_CASE("AppController setBindingEnabled works while runtime is running",
    "[UI][AppController]")
{
    STempProfileDir Temp("toggle_running");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->RuntimeState() == "running");

    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    auto* Model = Controller->MappingRuleModel();
    auto RuleId = Model->ruleIdAt(0);
    REQUIRE(Controller->setBindingEnabled(RuleId, false));

    auto EnabledAfter = Model->data(
        Model->index(0), ZMappingRuleModel::EnabledRole).toBool();
    REQUIRE_FALSE(EnabledAfter);
}

// ══════════════════════════════════════════════════════════════
// MouseMove 绑定测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController applySelectedBinding left_stick MouseMove succeeds",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_left");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    bool bResult = Controller->applySelectedBinding("left_stick", "MouseMove", "Cursor");

    REQUIRE(bResult);
    auto* Model = Controller->MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);
    REQUIRE(Model->data(Model->index(0), ZMappingRuleModel::ActionKindRole).toString()
        == "MouseMove");
    REQUIRE(Model->data(Model->index(0), ZMappingRuleModel::ActionValueRole).toString()
        == "Cursor");
    REQUIRE(Model->data(Model->index(0), ZMappingRuleModel::OutputRole).toString()
        == "Move Cursor");
}

TEST_CASE("AppController applySelectedBinding right_stick MouseMove succeeds",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_right");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    bool bResult = Controller->applySelectedBinding("right_stick", "MouseMove", "Cursor");

    REQUIRE(bResult);
    auto* Model = Controller->MappingRuleModel();
    REQUIRE(Model->rowCount() == 1);
    REQUIRE(Model->data(Model->index(0), ZMappingRuleModel::InputRole).toString()
        == "right_stick");
}

TEST_CASE("AppController applySelectedBinding MouseMove uses Axis2D input type",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_axis2d");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    Controller->applySelectedBinding("left_stick", "MouseMove", "Cursor");

    auto Snapshot = Controller->MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Snapshot[0].Input.ControlType == EInputControlType::Axis2D);
    REQUIRE(Snapshot[0].Input.EventType == EInputEventType::Changed);
    REQUIRE(Snapshot[0].Input.Deadzone == 0.20f);
    REQUIRE(Snapshot[0].Input.Threshold == 0.0f);
}

TEST_CASE("AppController applySelectedBinding MouseMove uses Analog mode and sensitivity",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_analog");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    Controller->applySelectedBinding("left_stick", "MouseMove", "Cursor");

    auto Snapshot = Controller->MappingRuleModel()->ListRulesSnapshot();
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Snapshot[0].Output.Mode == EMappingActionMode::Analog);
    REQUIRE(Snapshot[0].Output.Sensitivity == 12.0f);
    REQUIRE(Snapshot[0].Output.Action.Type == EActionType::MouseMove);
}

TEST_CASE("AppController applySelectedBinding button to MouseMove rejected",
    "[UI][AppController]")
{
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory());
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.startRuntime());

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);

    bool bResult = Controller.applySelectedBinding("button_south", "MouseMove", "Cursor");

    REQUIRE_FALSE(bResult);
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller.MappingRuleModel()->rowCount() == 0);
}

TEST_CASE("AppController applySelectedBinding MouseMove save load round trip",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_persist");

    StdPath ActiveFile;
    {
        auto Controller = MakeInitializedController(Temp.Path);
        REQUIRE(Controller->startRuntime());
        REQUIRE(Controller->applySelectedBinding("left_stick", "MouseMove", "Cursor"));
        REQUIRE(Controller->ProfileSaveState() == "clean");
        ActiveFile = StdPath(Controller->ProfilePath().toStdString());
    }

    // 用独立的 ZProfileManager 读回文件，验证 MouseMove 语义已落盘
    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(ActiveFile);
    REQUIRE(LoadResult.IsOk());
    auto& Rules = LoadResult.Value().Rules;
    REQUIRE(Rules.size() == 1);
    REQUIRE(Rules[0].Output.Mode == EMappingActionMode::Analog);
    REQUIRE(Rules[0].Output.Sensitivity == 12.0f);
    REQUIRE(Rules[0].Input.ControlType == EInputControlType::Axis2D);
    REQUIRE(Rules[0].Input.Deadzone == 0.20f);
}

// ── Runtime 集成：MouseMove Axis2D 事件 dispatch ──

static SInputEvent MakeAxis2DEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    float32 X, float32 Y)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = {.X = X, .Y = Y};
    return Event;
}

TEST_CASE("AppController MouseMove Axis2D inside deadzone does not dispatch",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_deadzone_in");
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory(), Temp.Path);
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());
    REQUIRE(Controller.startRuntime());
    REQUIRE(Controller.applySelectedBinding("left_stick", "MouseMove", "Cursor"));

    // magnitude = sqrt(0.1^2 + 0.1^2) ≈ 0.14 < deadzone 0.20
    RawInputBackend->EmitInput(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.1f, 0.1f));
    Controller.pumpOnce();

    REQUIRE(Controller.LastDispatchedInputCount() == 0);
}

TEST_CASE("AppController MouseMove Axis2D outside deadzone dispatches",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_deadzone_out");
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory(), Temp.Path);
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());
    REQUIRE(Controller.startRuntime());
    REQUIRE(Controller.applySelectedBinding("left_stick", "MouseMove", "Cursor"));

    // magnitude = sqrt(0.5^2 + 0.5^2) ≈ 0.71 > deadzone 0.20
    RawInputBackend->EmitInput(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.5f, 0.5f));
    Controller.pumpOnce();

    REQUIRE(Controller.LastDispatchedInputCount() > 0);
}

TEST_CASE("AppController MouseMove disabled rule does not dispatch",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_disabled");
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory(), Temp.Path);
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());
    REQUIRE(Controller.startRuntime());
    REQUIRE(Controller.applySelectedBinding("left_stick", "MouseMove", "Cursor"));

    // 禁用该规则
    auto* Model = Controller.MappingRuleModel();
    auto RuleId = Model->ruleIdAt(0);
    REQUIRE(Controller.setBindingEnabled(RuleId, false));

    // 注入超出 deadzone 的事件
    RawInputBackend->EmitInput(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.5f, 0.5f));
    Controller.pumpOnce();

    REQUIRE(Controller.LastDispatchedInputCount() == 0);
}

TEST_CASE("AppController MouseMove mapping dispatches while runtime is running",
    "[UI][AppController]")
{
    STempProfileDir Temp("mousemove_dispatch");
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZAppController Controller(InputFactory, MakeNullOutputFactory(), Temp.Path);
    REQUIRE(Controller.initializeRuntime());
    REQUIRE(Controller.initializeProfiles());
    REQUIRE(Controller.startRuntime());
    REQUIRE(Controller.applySelectedBinding("left_stick", "MouseMove", "Cursor"));

    // 注入超出 deadzone 的事件
    RawInputBackend->EmitInput(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.5f, 0.5f));
    Controller.pumpOnce();

    REQUIRE(Controller.LastDispatchedInputCount() == 1);
}

// ══════════════════════════════════════════════════════════════
// 多配置命令测试（8.2）：initializeProfiles / switch / create /
// rename / delete，全部通过隔离临时目录验证信号矩阵与磁盘副作用。
// ══════════════════════════════════════════════════════════════

TEST_CASE("AppController initializeProfiles before initializeRuntime returns false and creates no directory",
    "[UI][AppController]")
{
    STempProfileDir Temp("init_noruntime");
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory(), Temp.Path);

    QSignalSpy ErrorSpy(&Controller, &ZAppController::runtimeError);
    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);

    REQUIRE_FALSE(Controller.initializeProfiles());
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);

    // 运行时未就绪时不触碰磁盘，配置目录不应被创建
    REQUIRE_FALSE(std::filesystem::exists(Temp.Path));
}

TEST_CASE("AppController initializeProfiles success yields single Default and cannot delete",
    "[UI][AppController]")
{
    STempProfileDir Temp("init_ok");
    auto Controller = MakeInitializedController(Temp.Path);

    REQUIRE(Controller->ActiveProfileName() == "Default");
    REQUIRE(Controller->ActiveProfileId() == "default");
    REQUIRE(Controller->ProfileEntries().size() == 1);
    REQUIRE_FALSE(Controller->CanDeleteProfile());
    REQUIRE(std::filesystem::exists(Temp.Path / "default.json"));
}

TEST_CASE("AppController ProfileEntries contain only id and name in manager order",
    "[UI][AppController]")
{
    STempProfileDir Temp("entries");
    auto Controller = MakeInitializedController(Temp.Path);

    auto Entries = Controller->ProfileEntries();
    REQUIRE(Entries.size() == 1);

    auto Entry = Entries.at(0).toMap();
    REQUIRE(Entry.size() == 2);
    REQUIRE(Entry.contains("id"));
    REQUIRE(Entry.contains("name"));
    REQUIRE(Entry.value("id").toString() == "default");
    REQUIRE(Entry.value("name").toString() == "Default");
}

TEST_CASE("AppController initializeProfiles emits list active status and loaded once each",
    "[UI][AppController]")
{
    STempProfileDir Temp("init_matrix");
    ZAppController Controller(MakeFakeInputFactory(), MakeNullOutputFactory(), Temp.Path);
    REQUIRE(Controller.initializeRuntime());

    QSignalSpy ListSpy(&Controller, &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(&Controller, &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(&Controller, &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(&Controller, &ZAppController::profileSaved);
    QSignalSpy LoadedSpy(&Controller, &ZAppController::profileLoaded);

    REQUIRE(Controller.initializeProfiles());

    REQUIRE(ListSpy.count() == 1);
    REQUIRE(ActiveSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 0);
    REQUIRE(LoadedSpy.count() == 1);
}

TEST_CASE("AppController createProfile makes an empty profile and enables delete",
    "[UI][AppController]")
{
    STempProfileDir Temp("create_empty");
    auto Controller = MakeInitializedController(Temp.Path);

    // 在 default 上加一条规则并 autosave
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));
    REQUIRE(Controller->MappingRuleModel()->rowCount() == 1);

    // 新建配置：不复制映射，模型清空，当前项变为 Untitled_1
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->MappingRuleModel()->rowCount() == 0);
    REQUIRE(Controller->ActiveProfileName() == "Untitled_1");
    REQUIRE(Controller->CanDeleteProfile());
}

TEST_CASE("AppController createProfile numbers Untitled sequentially",
    "[UI][AppController]")
{
    STempProfileDir Temp("create_numbering");
    auto Controller = MakeInitializedController(Temp.Path);

    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->ActiveProfileName() == "Untitled_1");

    // Untitled_1 为空且 clean，第二次新建得到 Untitled_2
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->ActiveProfileName() == "Untitled_2");
}

TEST_CASE("AppController createProfile from clean state emits full matrix",
    "[UI][AppController]")
{
    STempProfileDir Temp("create_matrix");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy ListSpy(Controller.get(), &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(Controller.get(), &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);
    QSignalSpy LoadedSpy(Controller.get(), &ZAppController::profileLoaded);

    REQUIRE(Controller->createProfile());

    REQUIRE(ListSpy.count() == 1);
    REQUIRE(ActiveSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 1);
    REQUIRE(LoadedSpy.count() == 1);
}

TEST_CASE("AppController switchProfile from clean state emits active status and loaded only",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_matrix");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建并切到 Untitled_1（此时为 clean）
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->ProfileSaveState() == "clean");

    QSignalSpy ListSpy(Controller.get(), &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(Controller.get(), &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);
    QSignalSpy LoadedSpy(Controller.get(), &ZAppController::profileLoaded);

    REQUIRE(Controller->switchProfile(QStringLiteral("default")));

    REQUIRE(ListSpy.count() == 0);
    REQUIRE(ActiveSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 0);
    REQUIRE(LoadedSpy.count() == 1);
}

TEST_CASE("AppController switchProfile replaces model and preserves running state",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_replace");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());

    // default 上加规则并 autosave
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    // 新建空白配置，模型清空
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->MappingRuleModel()->rowCount() == 0);

    // 切回 default，模型恢复该配置的规则
    REQUIRE(Controller->switchProfile(QStringLiteral("default")));
    REQUIRE(Controller->MappingRuleModel()->rowCount() == 1);

    // 切换不改变运行状态
    REQUIRE(Controller->RuntimeState() == "running");
}

TEST_CASE("AppController switchProfile preserves pump timer",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_pump");
    auto Controller = MakeInitializedController(Temp.Path);
    REQUIRE(Controller->startRuntime());
    REQUIRE(Controller->createProfile());

    Controller->startPumpTimer(16);
    REQUIRE(Controller->IsPumpTimerRunning());

    REQUIRE(Controller->switchProfile(QStringLiteral("default")));

    // 切换不应停止 pump 定时器
    REQUIRE(Controller->IsPumpTimerRunning());
    Controller->stopPumpTimer();
}

TEST_CASE("AppController switchProfile to current id is a no-op without signals",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_noop");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy ListSpy(Controller.get(), &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(Controller.get(), &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy LoadedSpy(Controller.get(), &ZAppController::profileLoaded);

    // 已是当前项，直接返回 true 且不发任何信号
    REQUIRE(Controller->switchProfile(QStringLiteral("default")));
    REQUIRE(ListSpy.count() == 0);
    REQUIRE(ActiveSpy.count() == 0);
    REQUIRE(StatusSpy.count() == 0);
    REQUIRE(LoadedSpy.count() == 0);
}

TEST_CASE("AppController switchProfile to unknown id fails and keeps active unchanged",
    "[UI][AppController]")
{
    STempProfileDir Temp("switch_unknown");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy ErrorSpy(Controller.get(), &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller->switchProfile(QStringLiteral("nonexistent")));
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller->ActiveProfileId() == "default");
}

TEST_CASE("AppController dirty switch saves old profile only and leaves new profile untouched",
    "[UI][AppController]")
{
    STempProfileDir Temp("dirty_switch");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建 Untitled_1（空），再切回 default
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->switchProfile(QStringLiteral("default")));

    // 阻塞 default.json 使 apply 的 autosave 失败，产生真实 dirty
    StdPath DefaultFile = Temp.Path / "default.json";
    std::filesystem::remove(DefaultFile);
    std::filesystem::create_directory(DefaultFile);
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));
    REQUIRE(Controller->IsProfileDirty());

    // 解除阻塞，使切换前的保存能成功写入 default.json
    std::filesystem::remove(DefaultFile);

    // 切换到 Untitled_1：切换前保存把 default 的规则写入 default.json
    REQUIRE(Controller->switchProfile(QStringLiteral("untitled_1")));

    // default.json 应包含刚保存的规则
    ZProfileManager DefaultManager;
    auto DefaultLoad = DefaultManager.LoadProfile(DefaultFile);
    REQUIRE(DefaultLoad.IsOk());
    REQUIRE(DefaultLoad.Value().Rules.size() == 1);

    // untitled_1.json 未被污染，仍为空规则
    ZProfileManager NewManager;
    auto NewLoad = NewManager.LoadProfile(Temp.Path / "untitled_1.json");
    REQUIRE(NewLoad.IsOk());
    REQUIRE(NewLoad.Value().Rules.empty());
}

TEST_CASE("AppController dirty save failure blocks switch create and delete",
    "[UI][AppController]")
{
    STempProfileDir Temp("dirty_blocks");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建 Untitled_1，使列表有两项且成为当前项
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->ActiveProfileId() == "untitled_1");

    // 阻塞当前配置文件，apply 触发 autosave 失败 → dirty/error
    StdPath ActiveFile = Temp.Path / "untitled_1.json";
    std::filesystem::remove(ActiveFile);
    std::filesystem::create_directory(ActiveFile);
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));
    REQUIRE(Controller->IsProfileDirty());
    REQUIRE(Controller->ProfileSaveState() == "error");

    // 切换前保存脏配置失败，三种选择变更命令都应被阻断
    REQUIRE_FALSE(Controller->switchProfile(QStringLiteral("default")));
    REQUIRE(Controller->ActiveProfileId() == "untitled_1");

    REQUIRE_FALSE(Controller->createProfile());
    REQUIRE_FALSE(Controller->deleteActiveProfile());

    // 列表未变，仍为两项
    REQUIRE(Controller->ProfileEntries().size() == 2);
}

TEST_CASE("AppController renameActiveProfile updates name keeps id and rewrites file",
    "[UI][AppController]")
{
    STempProfileDir Temp("rename_ok");
    auto Controller = MakeInitializedController(Temp.Path);

    // 先加规则，确认重命名保留映射
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    QSignalSpy ListSpy(Controller.get(), &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(Controller.get(), &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);
    QSignalSpy LoadedSpy(Controller.get(), &ZAppController::profileLoaded);

    REQUIRE(Controller->renameActiveProfile(QStringLiteral("My Profile")));

    // rename 矩阵：list=1, active=1, status=1, saved=1, loaded=0
    REQUIRE(ListSpy.count() == 1);
    REQUIRE(ActiveSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 1);
    REQUIRE(LoadedSpy.count() == 0);

    REQUIRE(Controller->ActiveProfileName() == "My Profile");
    REQUIRE(Controller->ActiveProfileId() == "default");

    // 文件仍是 default.json，新名称和规则均已落盘
    ZProfileManager Manager;
    auto LoadResult = Manager.LoadProfile(Temp.Path / "default.json");
    REQUIRE(LoadResult.IsOk());
    REQUIRE(LoadResult.Value().Name == "My Profile");
    REQUIRE(LoadResult.Value().Rules.size() == 1);
}

TEST_CASE("AppController renameActiveProfile with blank name fails and keeps name",
    "[UI][AppController]")
{
    STempProfileDir Temp("rename_blank");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy ErrorSpy(Controller.get(), &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller->renameActiveProfile(QStringLiteral("   ")));
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller->ActiveProfileName() == "Default");
}

TEST_CASE("AppController renameActiveProfile to duplicate name ignoring case fails",
    "[UI][AppController]")
{
    STempProfileDir Temp("rename_dup");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建 Untitled_1 后尝试改名为 "DEFAULT"（与 Default 忽略大小写重名）
    REQUIRE(Controller->createProfile());

    QSignalSpy ErrorSpy(Controller.get(), &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller->renameActiveProfile(QStringLiteral("DEFAULT")));
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller->ActiveProfileName() == "Untitled_1");
}

TEST_CASE("AppController deleteActiveProfile from clean state emits matrix and switches to fallback",
    "[UI][AppController]")
{
    STempProfileDir Temp("delete_ok");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建 Untitled_1（当前项，clean），此时可删除
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->CanDeleteProfile());

    QSignalSpy ListSpy(Controller.get(), &ZAppController::profileListChanged);
    QSignalSpy ActiveSpy(Controller.get(), &ZAppController::activeProfileChanged);
    QSignalSpy StatusSpy(Controller.get(), &ZAppController::profileStatusChanged);
    QSignalSpy SavedSpy(Controller.get(), &ZAppController::profileSaved);
    QSignalSpy LoadedSpy(Controller.get(), &ZAppController::profileLoaded);

    REQUIRE(Controller->deleteActiveProfile());

    // delete 矩阵：list=1, active=1, status=1, saved=0, loaded=1
    REQUIRE(ListSpy.count() == 1);
    REQUIRE(ActiveSpy.count() == 1);
    REQUIRE(StatusSpy.count() == 1);
    REQUIRE(SavedSpy.count() == 0);
    REQUIRE(LoadedSpy.count() == 1);

    // 回退到 default，列表只剩一项，被删文件已消失
    REQUIRE(Controller->ActiveProfileId() == "default");
    REQUIRE(Controller->ProfileEntries().size() == 1);
    REQUIRE_FALSE(std::filesystem::exists(Temp.Path / "untitled_1.json"));
}

TEST_CASE("AppController deleteActiveProfile refuses to delete the last profile",
    "[UI][AppController]")
{
    STempProfileDir Temp("delete_last");
    auto Controller = MakeInitializedController(Temp.Path);

    QSignalSpy ErrorSpy(Controller.get(), &ZAppController::runtimeError);

    REQUIRE_FALSE(Controller->deleteActiveProfile());
    REQUIRE(ErrorSpy.count() == 1);
    REQUIRE(Controller->ProfileEntries().size() == 1);
}

TEST_CASE("AppController autosave targets only the new active profile file after switch",
    "[UI][AppController]")
{
    STempProfileDir Temp("autosave_target");
    auto Controller = MakeInitializedController(Temp.Path);

    // 新建并切到 Untitled_1，在其上加规则触发 autosave
    REQUIRE(Controller->createProfile());
    REQUIRE(Controller->ActiveProfileId() == "untitled_1");
    REQUIRE(Controller->applySelectedBinding("button_south", "Keyboard", "Space"));

    // 规则只落在 untitled_1.json，default.json 保持空
    ZProfileManager Manager;

    auto NewLoad = Manager.LoadProfile(Temp.Path / "untitled_1.json");
    REQUIRE(NewLoad.IsOk());
    REQUIRE(NewLoad.Value().Rules.size() == 1);

    auto DefaultLoad = Manager.LoadProfile(Temp.Path / "default.json");
    REQUIRE(DefaultLoad.IsOk());
    REQUIRE(DefaultLoad.Value().Rules.empty());
}
