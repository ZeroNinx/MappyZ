// ZApplicationBootstrap 单元测试。
// 使用 fake/null 工厂验证应用层 bootstrap 完整生命周期：
// Initialize/StartRuntime/StopRuntime/PumpOnce、profile 加载、
// 工厂失败、幂等行为、Error 恢复、析构安全。

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "App/ApplicationBootstrap.h"
#include "Backends/Input/FakeInputBackend.h"
#include "Backends/Output/NullOutputBackend.h"
#include "Core/ControlId.h"
#include "Runtime/ProfileManager.h"

using namespace MappyZ;

// ── 测试用 factory 辅助 ──

// 返回创建 FakeInputBackend 的工厂
static TInputBackendFactory MakeFakeInputFactory()
{
    return []() -> TResult<TUniquePtr<IInputBackend>> {
        return TResult<TUniquePtr<IInputBackend>>::Ok(
            std::make_unique<ZFakeInputBackend>());
    };
}

// 返回创建 NullOutputBackend 的工厂
static TOutputBackendFactory MakeNullOutputFactory()
{
    return []() -> TResult<TUniquePtr<IOutputBackend>> {
        return TResult<TUniquePtr<IOutputBackend>>::Ok(
            std::make_unique<ZNullOutputBackend>());
    };
}

// 返回始终失败的输入后端工厂
static TInputBackendFactory MakeFailingInputFactory(const StdString& Message)
{
    return [Message]() -> TResult<TUniquePtr<IInputBackend>> {
        return TResult<TUniquePtr<IInputBackend>>::Err(
            MakeError(EErrorCode::Unknown, Message));
    };
}

// 返回始终失败的输出后端工厂
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

// 创建包含一条规则的临时 profile 文件，返回路径
static StdPath CreateTempProfileFile()
{
    SMappingProfile Profile;
    Profile.Id = "temp_test";
    Profile.Name = "Temp Test";
    Profile.Rules.push_back(
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"));

    auto TempPath = std::filesystem::temp_directory_path()
        / "mappyz_bootstrap_test_profile.json";

    ZProfileManager Manager;
    auto Result = Manager.SaveProfile(Profile, TempPath);
    REQUIRE(Result.IsOk());

    return TempPath;
}

// ── 默认状态 ──

TEST_CASE("ApplicationBootstrap default state is Created",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    auto Status = Bootstrap.GetStatus();
    REQUIRE(Status.State == EApplicationBootstrapState::Created);
}

// ── Initialize 成功 ──

TEST_CASE("ApplicationBootstrap Initialize with fake factory succeeds to Ready",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    auto Result = Bootstrap.Initialize();

    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
}

// ── Initialize 创建 host 并加载默认空 profile ──

TEST_CASE("ApplicationBootstrap Initialize creates host with default empty profile",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Bootstrap.Initialize();

    auto& Host = Bootstrap.GetRuntimeHost();
    auto Profile = Host.GetProfileSnapshot();

    REQUIRE(Profile.Name == "Default");
    REQUIRE(Profile.Rules.empty());
    REQUIRE(Host.GetStatus().State == ERuntimeHostState::Stopped);
}

// ── 指定 profile 路径加载 ──

TEST_CASE("ApplicationBootstrap Initialize loads profile from path",
    "[App][ApplicationBootstrap]")
{
    auto TempPath = CreateTempProfileFile();

    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    auto Result = Bootstrap.Initialize({.ProfilePath = TempPath});

    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);

    auto Profile = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    REQUIRE(Profile.Id == "temp_test");
    REQUIRE(Profile.Rules.size() == 1);

    std::filesystem::remove(TempPath);
}

// ── profile 加载失败 ──

TEST_CASE("ApplicationBootstrap Initialize fails when profile path is invalid",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    auto Result = Bootstrap.Initialize(
        {.ProfilePath = StdPath("nonexistent_bootstrap_test_xyz.json")});

    REQUIRE_FALSE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);
}

// ── 重复 Initialize 幂等 ──

TEST_CASE("ApplicationBootstrap repeated Initialize is idempotent",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    (void)Bootstrap.Initialize();
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);

    auto Result = Bootstrap.Initialize();
    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
}

// ── Error 后重试 Initialize ──

TEST_CASE("ApplicationBootstrap Initialize retries after Error state",
    "[App][ApplicationBootstrap]")
{
    // 第一次用失败的 factory
    bool bShouldFail = true;

    auto InputFactory = [&bShouldFail]() -> TResult<TUniquePtr<IInputBackend>> {
        if (bShouldFail)
        {
            return TResult<TUniquePtr<IInputBackend>>::Err(
                MakeError(EErrorCode::Unknown, "temporary failure"));
        }
        return TResult<TUniquePtr<IInputBackend>>::Ok(
            std::make_unique<ZFakeInputBackend>());
    };

    ZApplicationBootstrap Bootstrap(InputFactory, MakeNullOutputFactory());

    auto FirstResult = Bootstrap.Initialize();
    REQUIRE_FALSE(FirstResult);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);

    // 修复条件后重试
    bShouldFail = false;
    auto SecondResult = Bootstrap.Initialize();
    REQUIRE(SecondResult);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
}

// ── StartRuntime 失败后 re-Initialize 不触发悬垂引用 ──

TEST_CASE("ApplicationBootstrap re-Initialize after StartRuntime failure safely replaces backends",
    "[App][ApplicationBootstrap]")
{
    // 使用可控失败的 FakeInputBackend
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZApplicationBootstrap Bootstrap(InputFactory, MakeNullOutputFactory());

    // Initialize 成功：此时 RuntimeHost 已构造，持有第一组 backend 引用
    (void)Bootstrap.Initialize();
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);

    // 注入 start error，让 StartRuntime 失败
    RawInputBackend->SetStartError("simulated hardware failure");
    auto StartResult = Bootstrap.StartRuntime();
    REQUIRE_FALSE(StartResult);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);

    // re-Initialize：必须先销毁旧 RuntimeHost 再销毁旧 backend，
    // 否则旧 host 析构时会访问已释放的 backend
    auto RetryResult = Bootstrap.Initialize();
    REQUIRE(RetryResult);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);

    // 新 host 可以正常启动
    auto SecondStart = Bootstrap.StartRuntime();
    REQUIRE(SecondStart);
    REQUIRE(Bootstrap.GetRuntimeHost().IsRunning());
}

// ── StartRuntime 未 Initialize ──

TEST_CASE("ApplicationBootstrap StartRuntime before Initialize returns error",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    auto Result = Bootstrap.StartRuntime();

    REQUIRE_FALSE(Result);
}

// ── StartRuntime 成功 ──

TEST_CASE("ApplicationBootstrap StartRuntime succeeds after Initialize",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Bootstrap.Initialize();

    auto Result = Bootstrap.StartRuntime();

    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Running);
    REQUIRE(Bootstrap.GetRuntimeHost().IsRunning());
}

// ── StopRuntime 停止 host ──

TEST_CASE("ApplicationBootstrap StopRuntime transitions Running to Ready",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Bootstrap.Initialize();
    (void)Bootstrap.StartRuntime();
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Running);

    Bootstrap.StopRuntime();

    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
    REQUIRE_FALSE(Bootstrap.GetRuntimeHost().IsRunning());
}

// ── StopRuntime 在 Created/Error 时 no-op ──

TEST_CASE("ApplicationBootstrap StopRuntime is no-op in Created and Error states",
    "[App][ApplicationBootstrap]")
{
    SECTION("Created state")
    {
        ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

        Bootstrap.StopRuntime();
        REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Created);
    }

    SECTION("Error state")
    {
        ZApplicationBootstrap Bootstrap(
            MakeFailingInputFactory("test error"), MakeNullOutputFactory());

        (void)Bootstrap.Initialize();
        REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);

        Bootstrap.StopRuntime();
        REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);
    }
}

// ── 重复 StopRuntime 安全 ──

TEST_CASE("ApplicationBootstrap repeated StopRuntime is safe",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Bootstrap.Initialize();
    (void)Bootstrap.StartRuntime();

    Bootstrap.StopRuntime();
    Bootstrap.StopRuntime();

    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
}

// ── PumpOnce 在 Running 时处理事件 ──

TEST_CASE("ApplicationBootstrap PumpOnce dispatches input events when Running",
    "[App][ApplicationBootstrap]")
{
    // 使用捕获指针的 factory，以便测试中直接注入事件
    ZFakeInputBackend* RawInputBackend = nullptr;
    auto InputFactory = [&RawInputBackend]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        RawInputBackend = Backend.get();
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZApplicationBootstrap Bootstrap(InputFactory, MakeNullOutputFactory());

    auto TempPath = CreateTempProfileFile();
    (void)Bootstrap.Initialize({.ProfilePath = TempPath});
    (void)Bootstrap.StartRuntime();

    // 通过捕获的 FakeInputBackend 指针注入事件到 event queue
    RawInputBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    auto Summary = Bootstrap.PumpOnce();

    REQUIRE(Summary.DrainedEventCount == 1);
    REQUIRE(Summary.InputEventCount == 1);
    REQUIRE(Summary.MappedInputCount == 1);
    REQUIRE(Summary.DispatchedInputCount == 1);

    std::filesystem::remove(TempPath);
}

// ── PumpOnce 在非 Running 状态返回空 summary ──

TEST_CASE("ApplicationBootstrap PumpOnce returns empty summary in non-Running states",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    // Created 状态
    auto Summary = Bootstrap.PumpOnce();
    REQUIRE(Summary.DrainedEventCount == 0);

    // Ready 状态
    (void)Bootstrap.Initialize();
    Summary = Bootstrap.PumpOnce();
    REQUIRE(Summary.DrainedEventCount == 0);
}

// ── 析构安全 ──

TEST_CASE("ApplicationBootstrap destructor stops host and cleans up safely",
    "[App][ApplicationBootstrap]")
{
    {
        ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
        (void)Bootstrap.Initialize();
        (void)Bootstrap.StartRuntime();
        REQUIRE(Bootstrap.GetRuntimeHost().IsRunning());
    }
    // 析构不应崩溃
    REQUIRE(true);
}

// ── 输入后端 factory 失败 ──

TEST_CASE("ApplicationBootstrap Initialize fails when input factory fails",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFailingInputFactory("input backend unavailable"),
        MakeNullOutputFactory());

    auto Result = Bootstrap.Initialize();

    REQUIRE_FALSE(Result);
    REQUIRE(Result.Failure().Message == "input backend unavailable");
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);
}

// ── 输出后端 factory 失败 ──

TEST_CASE("ApplicationBootstrap Initialize fails when output factory fails",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFakeInputFactory(),
        MakeFailingOutputFactory("output backend unavailable"));

    auto Result = Bootstrap.Initialize();

    REQUIRE_FALSE(Result);
    REQUIRE(Result.Failure().Message == "output backend unavailable");
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);
}

// ── IsUsingNullOutput ──

TEST_CASE("ApplicationBootstrap IsUsingNullOutput reflects last successful Initialize",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFakeInputFactory(),
        MakeNullOutputFactory());

    // 未 initialize 时默认 false
    REQUIRE_FALSE(Bootstrap.IsUsingNullOutput());

    // 使用 NullOutput 初始化
    auto Result = Bootstrap.Initialize({.bUseNullOutput = true});
    REQUIRE(Result);
    REQUIRE(Bootstrap.IsUsingNullOutput());
}

TEST_CASE("ApplicationBootstrap IsUsingNullOutput false when real output used",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFakeInputFactory(),
        MakeNullOutputFactory());

    // 使用真实输出后端（NullOutputFactory 充当真实 factory）
    auto Result = Bootstrap.Initialize({.bUseNullOutput = false});
    REQUIRE(Result);
    REQUIRE_FALSE(Bootstrap.IsUsingNullOutput());
}

TEST_CASE("ApplicationBootstrap empty profile Name verified via snapshot",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFakeInputFactory(),
        MakeNullOutputFactory());

    auto Result = Bootstrap.Initialize({.bUseNullOutput = true});
    REQUIRE(Result);

    // 替换为空 Name 的 profile，验证 snapshot 确实反映空 Name
    SMappingProfile EmptyNameProfile;
    EmptyNameProfile.Name = "";
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(EmptyNameProfile));

    auto Snapshot = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    REQUIRE(Snapshot.Name.empty());
}

// ── Reinitialize ──

TEST_CASE("ApplicationBootstrap Reinitialize in Ready state rebuilds output backend",
    "[App][ApplicationBootstrap]")
{
    int OutputFactoryCallCount = 0;
    auto CountingOutputFactory = [&OutputFactoryCallCount]()
        -> TResult<TUniquePtr<IOutputBackend>> {
        ++OutputFactoryCallCount;
        return TResult<TUniquePtr<IOutputBackend>>::Ok(
            std::make_unique<ZNullOutputBackend>());
    };

    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), CountingOutputFactory);

    // 第一次 Initialize 使用 NullOutput（不调用 factory）
    auto InitResult = Bootstrap.Initialize({.bUseNullOutput = true});
    REQUIRE(InitResult);
    REQUIRE(OutputFactoryCallCount == 0);

    // Reinitialize 请求真实输出，factory 被调用
    auto ReinitResult = Bootstrap.Reinitialize({.bUseNullOutput = false});
    REQUIRE(ReinitResult);
    REQUIRE(OutputFactoryCallCount == 1);
    REQUIRE_FALSE(Bootstrap.IsUsingNullOutput());
}

TEST_CASE("ApplicationBootstrap Reinitialize in Running state safely stops old host and rebuilds",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());

    (void)Bootstrap.Initialize({.bUseNullOutput = true});
    (void)Bootstrap.StartRuntime();
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Running);

    auto Result = Bootstrap.Reinitialize({.bUseNullOutput = true});

    REQUIRE(Result);
    // Reinitialize 成功后状态为 Ready（host 处于 stopped）
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
    REQUIRE_FALSE(Bootstrap.GetRuntimeHost().IsRunning());
}

TEST_CASE("ApplicationBootstrap Reinitialize in Error state executes full setup and recovers to Ready",
    "[App][ApplicationBootstrap]")
{
    bool bShouldFail = true;
    auto InputFactory = [&bShouldFail]() -> TResult<TUniquePtr<IInputBackend>> {
        if (bShouldFail)
        {
            return TResult<TUniquePtr<IInputBackend>>::Err(
                MakeError(EErrorCode::Unknown, "temporary"));
        }
        return TResult<TUniquePtr<IInputBackend>>::Ok(
            std::make_unique<ZFakeInputBackend>());
    };

    ZApplicationBootstrap Bootstrap(InputFactory, MakeNullOutputFactory());

    // 先进入 Error 状态
    (void)Bootstrap.Initialize();
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);

    // 修复后 Reinitialize 恢复到 Ready
    bShouldFail = false;
    auto Result = Bootstrap.Reinitialize({.bUseNullOutput = true});
    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);
}

TEST_CASE("ApplicationBootstrap Reinitialize replaces old host with new backend references",
    "[App][ApplicationBootstrap]")
{
    ZFakeInputBackend* LatestBackend = nullptr;
    int FactoryCallCount = 0;

    auto InputFactory = [&]() -> TResult<TUniquePtr<IInputBackend>> {
        auto Backend = std::make_unique<ZFakeInputBackend>();
        LatestBackend = Backend.get();
        ++FactoryCallCount;
        return TResult<TUniquePtr<IInputBackend>>::Ok(std::move(Backend));
    };

    ZApplicationBootstrap Bootstrap(InputFactory, MakeNullOutputFactory());
    (void)Bootstrap.Initialize({.bUseNullOutput = true});
    REQUIRE(FactoryCallCount == 1);

    (void)Bootstrap.Reinitialize({.bUseNullOutput = true});
    REQUIRE(FactoryCallCount == 2);

    // 新 host 使用最新创建的 backend，可以正常启动和 pump
    (void)Bootstrap.StartRuntime();
    LatestBackend->EmitInput(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    auto Summary = Bootstrap.PumpOnce();
    REQUIRE(Summary.DrainedEventCount == 1);
}

TEST_CASE("ApplicationBootstrap Reinitialize with bSkipProfileSetup skips profile creation",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), MakeNullOutputFactory());
    (void)Bootstrap.Initialize({.bUseNullOutput = true});

    // 设置一个自定义 profile
    SMappingProfile CustomProfile;
    CustomProfile.Name = "Custom";
    CustomProfile.Rules.push_back(
        MakeButtonToKeyRule("r1", ControlId::ButtonSouth, "Space"));
    Bootstrap.GetRuntimeHost().ReplaceProfile(CustomProfile);

    // Reinitialize 时跳过 profile setup
    auto Result = Bootstrap.Reinitialize({
        .bUseNullOutput = true,
        .bSkipProfileSetup = true,
    });
    REQUIRE(Result);
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Ready);

    // host 已重建但 profile 未被默认覆盖，由调用方负责恢复
    // 此时 host 的 profile 为默认构造的空 profile（无 Name）
    auto Snapshot = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    REQUIRE(Snapshot.Name.empty());
    REQUIRE(Snapshot.Rules.empty());

    // 调用方手动恢复 profile
    Bootstrap.GetRuntimeHost().ReplaceProfile(std::move(CustomProfile));
    auto Restored = Bootstrap.GetRuntimeHost().GetProfileSnapshot();
    REQUIRE(Restored.Name == "Custom");
    REQUIRE(Restored.Rules.size() == 1);
}

TEST_CASE("ApplicationBootstrap Initialize with bUseNullOutput false calls output factory",
    "[App][ApplicationBootstrap]")
{
    bool bFactoryCalled = false;
    auto OutputFactory = [&bFactoryCalled]() -> TResult<TUniquePtr<IOutputBackend>> {
        bFactoryCalled = true;
        return TResult<TUniquePtr<IOutputBackend>>::Ok(
            std::make_unique<ZNullOutputBackend>());
    };

    ZApplicationBootstrap Bootstrap(MakeFakeInputFactory(), OutputFactory);

    auto Result = Bootstrap.Initialize({.bUseNullOutput = false});
    REQUIRE(Result);
    REQUIRE(bFactoryCalled);
    REQUIRE_FALSE(Bootstrap.IsUsingNullOutput());
}

TEST_CASE("ApplicationBootstrap real output factory failure returns error not silent NullOutput",
    "[App][ApplicationBootstrap]")
{
    ZApplicationBootstrap Bootstrap(
        MakeFakeInputFactory(),
        MakeFailingOutputFactory("real output not available"));

    auto Result = Bootstrap.Initialize({.bUseNullOutput = false});

    REQUIRE_FALSE(Result);
    REQUIRE(Result.Failure().Message == "real output not available");
    REQUIRE(Bootstrap.GetStatus().State == EApplicationBootstrapState::Error);
}
