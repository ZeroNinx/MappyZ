// ZApplicationBootstrap 实现。
// 组合后端工厂、ProfileManager 和 RuntimeHost，管理应用层运行时生命周期。
// 所有非预期路径都输出日志。

#include "App/ApplicationBootstrap.h"

#include "Backends/Output/NullOutputBackend.h"
#include "Runtime/ProfileManager.h"

#ifdef MAPPYZ_HAS_SDL3_INPUT
#include "Backends/Input/SdlInputBackend.h"
#endif

#ifdef MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT
#include "Backends/Output/WindowsSendInputBackend.h"
#endif

#include <cassert>
#include <cstdio>
#include <utility>

namespace ZeroMapper
{

// ── 生产默认工厂 ──

static TResult<TUniquePtr<IInputBackend>> DefaultInputBackendFactory()
{
#ifdef MAPPYZ_HAS_SDL3_INPUT
    return TResult<TUniquePtr<IInputBackend>>::Ok(
        std::make_unique<ZSdlInputBackend>());
#else
    return TResult<TUniquePtr<IInputBackend>>::Err(
        MakeError(EErrorCode::Unknown,
            "当前构建没有可用的真实输入后端（未启用 SDL3）"));
#endif
}

static TResult<TUniquePtr<IOutputBackend>> DefaultOutputBackendFactory()
{
#ifdef MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT
    return TResult<TUniquePtr<IOutputBackend>>::Ok(
        std::make_unique<ZWindowsSendInputBackend>());
#else
    // 无真实输出后端可用，回退到 NullOutputBackend
    std::fprintf(stderr,
        "[ApplicationBootstrap] 提示: 无真实输出后端可用，回退到 NullOutputBackend\n");
    return TResult<TUniquePtr<IOutputBackend>>::Ok(
        std::make_unique<ZNullOutputBackend>());
#endif
}

// ── 构造与析构 ──

ZApplicationBootstrap::ZApplicationBootstrap()
    : InputFactory(DefaultInputBackendFactory)
    , OutputFactory(DefaultOutputBackendFactory)
{
}

ZApplicationBootstrap::ZApplicationBootstrap(
    TInputBackendFactory InputFactory,
    TOutputBackendFactory OutputFactory)
    : InputFactory(std::move(InputFactory))
    , OutputFactory(std::move(OutputFactory))
{
}

ZApplicationBootstrap::~ZApplicationBootstrap()
{
    StopRuntime();
}

// ── Initialize ──

TResult<void> ZApplicationBootstrap::Initialize(SApplicationBootstrapOptions Options)
{
    // Ready/Running 状态下幂等返回，不重复创建
    if (BootstrapState == EApplicationBootstrapState::Ready
        || BootstrapState == EApplicationBootstrapState::Running)
    {
        std::fprintf(stderr,
            "[ApplicationBootstrap] 调试: 已初始化，跳过重复 Initialize\n");
        return TResult<void>::Ok();
    }

    // Created 或 Error 状态下执行完整 setup

    // 1. 创建输入后端
    auto InputResult = InputFactory();
    if (!InputResult)
    {
        BootstrapState = EApplicationBootstrapState::Error;
        BootstrapMessage = InputResult.Failure().Message;
        std::fprintf(stderr,
            "[ApplicationBootstrap] 错误: 创建输入后端失败: \"%s\"\n",
            BootstrapMessage.c_str());
        return TResult<void>::Err(std::move(InputResult).TakeFailure());
    }

    // 2. 创建输出后端
    auto OutputResult = CreateOutputBackend(Options.bUseNullOutput);
    if (!OutputResult)
    {
        BootstrapState = EApplicationBootstrapState::Error;
        BootstrapMessage = OutputResult.Failure().Message;
        std::fprintf(stderr,
            "[ApplicationBootstrap] 错误: 创建输出后端失败: \"%s\"\n",
            BootstrapMessage.c_str());
        return TResult<void>::Err(std::move(OutputResult).TakeFailure());
    }

    // 3. 按安全顺序清理旧对象：host 必须在 backend 之前析构，
    //    否则 host 内部引用会指向已析构的 backend
    if (RuntimeHost)
    {
        RuntimeHost->Stop();
        RuntimeHost.reset();
    }
    InputBackend.reset();
    OutputBackend.reset();

    // 4. 转移新后端所有权
    InputBackend = std::move(InputResult).TakeValue();
    OutputBackend = std::move(OutputResult).TakeValue();

    // 5. 构造 RuntimeHost（stopped 状态）
    RuntimeHost = std::make_unique<ZRuntimeHost>(*InputBackend, *OutputBackend);

    // 5. 加载 profile
    if (Options.ProfilePath.empty())
    {
        // 使用默认空 profile
        SMappingProfile DefaultProfile;
        DefaultProfile.Name = "Default";
        RuntimeHost->ReplaceProfile(std::move(DefaultProfile));
    }
    else
    {
        ZProfileManager ProfileManager;
        auto ProfileResult = ProfileManager.LoadProfile(Options.ProfilePath);
        if (!ProfileResult)
        {
            // profile 加载失败，清理已构造的对象
            RuntimeHost.reset();
            InputBackend.reset();
            OutputBackend.reset();

            BootstrapState = EApplicationBootstrapState::Error;
            BootstrapMessage = ProfileResult.Failure().Message;
            std::fprintf(stderr,
                "[ApplicationBootstrap] 错误: 加载 profile 失败: \"%s\" (路径: \"%s\")\n",
                BootstrapMessage.c_str(),
                Options.ProfilePath.string().c_str());
            return TResult<void>::Err(std::move(ProfileResult).TakeFailure());
        }

        RuntimeHost->ReplaceProfile(std::move(ProfileResult).TakeValue());
    }

    // 6. 缓存选项，更新状态
    CachedOptions = std::move(Options);
    BootstrapState = EApplicationBootstrapState::Ready;
    BootstrapMessage = "ready";
    return TResult<void>::Ok();
}

// ── StartRuntime ──

TResult<void> ZApplicationBootstrap::StartRuntime()
{
    if (!RuntimeHost)
    {
        std::fprintf(stderr,
            "[ApplicationBootstrap] 错误: 未初始化就调用 StartRuntime\n");
        return TResult<void>::Err(
            MakeError(EErrorCode::InvalidArgument,
                "必须先调用 Initialize() 再调用 StartRuntime()"));
    }

    auto Result = RuntimeHost->Start({
        .bAttachEventQueue = true,
        .bStartInputBackend = CachedOptions.bStartInputBackend,
        .bEnableMapping = CachedOptions.bEnableMapping,
    });

    if (!Result)
    {
        BootstrapState = EApplicationBootstrapState::Error;
        BootstrapMessage = Result.Failure().Message;
        std::fprintf(stderr,
            "[ApplicationBootstrap] 错误: 启动运行时失败: \"%s\"\n",
            BootstrapMessage.c_str());
        return TResult<void>::Err(std::move(Result).TakeFailure());
    }

    BootstrapState = EApplicationBootstrapState::Running;
    BootstrapMessage = "running";
    return TResult<void>::Ok();
}

// ── StopRuntime ──

void ZApplicationBootstrap::StopRuntime()
{
    if (!RuntimeHost)
    {
        // Created 或 Error 状态且 host 未构造，no-op
        return;
    }

    RuntimeHost->Stop();

    // Running → Ready；Ready 保持 Ready
    if (BootstrapState == EApplicationBootstrapState::Running)
    {
        BootstrapState = EApplicationBootstrapState::Ready;
        BootstrapMessage = "ready";
    }
}

// ── PumpOnce ──

SRuntimeEventPumpSummary ZApplicationBootstrap::PumpOnce()
{
    if (BootstrapState != EApplicationBootstrapState::Running)
    {
        return {};
    }

    return RuntimeHost->PumpOnce();
}

// ── 状态查询 ──

SApplicationBootstrapStatus ZApplicationBootstrap::GetStatus() const
{
    SApplicationBootstrapStatus Status;
    Status.State = BootstrapState;
    Status.Message = BootstrapMessage;

    if (RuntimeHost)
    {
        Status.RuntimeStatus = RuntimeHost->GetStatus();
    }

    return Status;
}

// ── 设备列表 ──

TVector<SDeviceInfo> ZApplicationBootstrap::ListInputDevices() const
{
    if (!InputBackend)
    {
        return {};
    }
    return InputBackend->ListDevices();
}

// ── RuntimeHost 访问器 ──

ZRuntimeHost& ZApplicationBootstrap::GetRuntimeHost()
{
    assert(RuntimeHost && "GetRuntimeHost() 要求 Initialize() 已成功");
    return *RuntimeHost;
}

const ZRuntimeHost& ZApplicationBootstrap::GetRuntimeHost() const
{
    assert(RuntimeHost && "GetRuntimeHost() 要求 Initialize() 已成功");
    return *RuntimeHost;
}

// ── 内部工具 ──

TResult<TUniquePtr<IOutputBackend>> ZApplicationBootstrap::CreateOutputBackend(
    bool bUseNullOutput)
{
    if (bUseNullOutput)
    {
        return TResult<TUniquePtr<IOutputBackend>>::Ok(
            std::make_unique<ZNullOutputBackend>());
    }

    return OutputFactory();
}

}  // namespace ZeroMapper
