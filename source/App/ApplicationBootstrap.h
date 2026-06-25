// 应用层运行时装配入口。
// 根据编译期开关选择输入/输出后端，加载启动 profile，构造 ZRuntimeHost，
// 向 Main.cpp 或后续 UI Bridge 暴露启动、停止和 pump 接口。
//
// App 层，依赖 Runtime、InputBackends、OutputBackends。
// 不依赖 QML、SDL、Win32 public 类型。

#pragma once

#include "Backends/Input/InputBackend.h"
#include "Backends/Output/OutputBackend.h"
#include "Core/ProjectCore.h"
#include "Runtime/RuntimeHost.h"

#include <functional>

namespace MappyZ
{

// 输入后端工厂函数类型
using TInputBackendFactory = std::function<TResult<TUniquePtr<IInputBackend>>()>;

// 输出后端工厂函数类型
using TOutputBackendFactory = std::function<TResult<TUniquePtr<IOutputBackend>>()>;

// Initialize() 选项
struct SApplicationBootstrapOptions
{
    // profile 文件路径，为空时使用默认空 profile
    StdPath ProfilePath;

    // 是否在 StartRuntime 时启动输入后端
    bool bStartInputBackend = true;

    // 是否在 StartRuntime 时启用映射
    bool bEnableMapping = true;
};

// 应用层 bootstrap 状态
enum class EApplicationBootstrapState
{
    Created,   // 默认构造后，未创建后端和 host
    Ready,     // Initialize 成功，后端和 host 已创建（host 处于 Stopped）
    Running,   // StartRuntime 成功，host 正在运行
    Error,     // Initialize 或 StartRuntime 失败
};

// 应用层 bootstrap 状态快照
struct SApplicationBootstrapStatus
{
    EApplicationBootstrapState State = EApplicationBootstrapState::Created;
    StdString Message;
    SRuntimeHostStatus RuntimeStatus;
};

class ZApplicationBootstrap final
{
public:
    // 生产构造：使用编译期开关选择真实后端
    ZApplicationBootstrap();

    // 测试构造：注入自定义后端工厂
    ZApplicationBootstrap(TInputBackendFactory InputFactory, TOutputBackendFactory OutputFactory);

    ~ZApplicationBootstrap();

    // 禁止拷贝和移动
    ZApplicationBootstrap(const ZApplicationBootstrap&) = delete;
    ZApplicationBootstrap& operator=(const ZApplicationBootstrap&) = delete;
    ZApplicationBootstrap(ZApplicationBootstrap&&) = delete;
    ZApplicationBootstrap& operator=(ZApplicationBootstrap&&) = delete;

    // 初始化：创建后端、加载 profile、构造 stopped 状态的 RuntimeHost。
    // Created/Error 状态下执行完整 setup；Ready/Running 状态下幂等返回 Ok。
    NODISCARD TResult<void> Initialize(SApplicationBootstrapOptions Options = {});

    // 启动运行时：调用 RuntimeHost::Start()，透传 options。
    // 要求已 Initialize()，否则返回错误。
    NODISCARD TResult<void> StartRuntime();

    // 停止运行时。幂等安全。
    // Created/Error 且 host 未构造时 no-op；Running 时停止后回到 Ready。
    void StopRuntime();

    // 从 event queue drain 事件并分发。仅 Running 状态委托给 host。
    NODISCARD SRuntimeEventPumpSummary PumpOnce();

    // 返回当前状态快照
    NODISCARD SApplicationBootstrapStatus GetStatus() const;

    // 返回当前输入后端已知的设备快照。
    // 未 initialize 或 InputBackend 为空时返回空 vector，不修改后端状态。
    NODISCARD TVector<SDeviceInfo> ListInputDevices() const;

    // 访问内部 RuntimeHost（供 UI Bridge 绑定和测试使用）。
    // 前置条件：Initialize() 已成功（状态为 Ready 或 Running）。
    NODISCARD ZRuntimeHost& GetRuntimeHost();
    NODISCARD const ZRuntimeHost& GetRuntimeHost() const;

private:

    TInputBackendFactory InputFactory;
    TOutputBackendFactory OutputFactory;

    // 成员声明顺序：后端先于 host，确保析构时 host 先释放
    TUniquePtr<IInputBackend> InputBackend;
    TUniquePtr<IOutputBackend> OutputBackend;
    TUniquePtr<ZRuntimeHost> RuntimeHost;

    EApplicationBootstrapState BootstrapState = EApplicationBootstrapState::Created;
    StdString BootstrapMessage;

    // 缓存 Initialize 时的选项，供 StartRuntime 使用
    SApplicationBootstrapOptions CachedOptions;
};

}  // namespace MappyZ
