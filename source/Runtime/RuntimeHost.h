// 运行时宿主。
// 组合 ZBackendEventQueue、ZActionDispatcher、ZMappingSession 和 ZRuntimeEventPump，
// 通过传入的 IInputBackend 和 IOutputBackend 引用完成运行时生命周期管理。
// 对外提供 Start/Stop/PumpOnce/ReplaceProfile 等统一入口。
//
// 本类不拥有输入/输出后端，通过引用接收。
// Runtime 层，不依赖 Qt、QML、SDL、Win32 或 nlohmann_json。

#pragma once

#include "Backends/Input/InputBackend.h"
#include "Backends/Output/OutputBackend.h"
#include "Core/MappingProfile.h"
#include "Core/ProjectCore.h"
#include "Runtime/ActionDispatcher.h"
#include "Runtime/BackendEventQueue.h"
#include "Runtime/MappingSession.h"
#include "Runtime/RuntimeEventPump.h"

namespace MappyZ
{

// 运行时宿主状态
enum class ERuntimeHostState
{
    Stopped,
    Running,
    Error,
};

// 运行时宿主状态快照
struct SRuntimeHostStatus
{
    ERuntimeHostState State = ERuntimeHostState::Stopped;
    StdString Message;
    SOutputBackendStatus OutputStatus;
};

// Start() 选项，控制启动行为
struct SRuntimeHostStartOptions
{
    bool bAttachEventQueue = true;
    bool bStartInputBackend = true;
    bool bEnableMapping = true;
};

class ZRuntimeHost final
{
public:
    ZRuntimeHost(IInputBackend& InputBackend, IOutputBackend& OutputBackend);
    ~ZRuntimeHost();

    // 禁止拷贝和移动
    ZRuntimeHost(const ZRuntimeHost&) = delete;
    ZRuntimeHost& operator=(const ZRuntimeHost&) = delete;
    ZRuntimeHost(ZRuntimeHost&&) = delete;
    ZRuntimeHost& operator=(ZRuntimeHost&&) = delete;

    // 启动运行时：按选项 attach event queue、启动 input backend、设置 mapping enabled
    NODISCARD TResult<void> Start(SRuntimeHostStartOptions Options = {});

    // 停止运行时：停止 input backend、detach event queue。幂等操作。
    void Stop();

    // 查询是否正在运行
    NODISCARD bool IsRunning() const noexcept;

    // 返回当前状态快照
    NODISCARD SRuntimeHostStatus GetStatus() const;

    // 从 event queue drain 事件并分发。仅 Running 状态有效。
    NODISCARD SRuntimeEventPumpSummary PumpOnce();

    // 替换 active profile 快照
    void ReplaceProfile(SMappingProfile Profile);

    // 返回当前 profile 快照拷贝
    NODISCARD SMappingProfile GetProfileSnapshot() const;

    // 设置是否启用映射
    void SetMappingEnabled(bool bEnabled);

    // 查询是否启用映射
    NODISCARD bool IsMappingEnabled() const noexcept;

    // ── 内部组件访问器（测试和 UI bridge 使用，不转移所有权） ──

    NODISCARD ZBackendEventQueue& GetEventQueue();
    NODISCARD ZRuntimeEventPump& GetEventPump();
    NODISCARD ZMappingSession& GetMappingSession();
    NODISCARD ZActionDispatcher& GetActionDispatcher();

private:
    IInputBackend& InputBackend;
    IOutputBackend& OutputBackend;

    // 成员构造顺序固定：event queue → dispatcher → session → pump
    ZBackendEventQueue EventQueue;
    ZActionDispatcher Dispatcher;
    ZMappingSession Session;
    ZRuntimeEventPump Pump;

    ERuntimeHostState HostState = ERuntimeHostState::Stopped;
    StdString HostMessage;
    bool bDidStartInputBackend = false;
};

}  // namespace MappyZ
