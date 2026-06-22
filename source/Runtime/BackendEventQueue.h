// 运行时后端事件队列。
// 订阅 IInputBackend 的三个回调（设备连接、设备断开、输入事件），
// 将回调转换为线程安全的待处理事件队列。
// 调用方在合适线程调用 DrainEvents() 批量取走事件，实现线程切换。
//
// 本类不拥有输入后端，通过引用接收。
// 入队操作（回调触发）线程安全，Attach/Detach/DrainEvents 由调用方保证单线程调用。

#pragma once

#include "Backends/Input/InputBackend.h"
#include "Core/DeviceId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

#include <mutex>
#include <variant>

namespace ZeroMapper
{

// 后端事件类型
enum class EBackendEventType
{
    DeviceConnected,     // 设备连接
    DeviceDisconnected,  // 设备断开
    Input,               // 输入事件
};

// 后端事件载荷，使用 variant 实现类型安全的多态
using TBackendEventPayload = std::variant<
    std::monostate,
    SDeviceInfo,
    SDeviceId,
    SInputEvent>;

// 后端事件，队列中的最小单元
struct SBackendEvent
{
    EBackendEventType Type = EBackendEventType::Input;
    TBackendEventPayload Payload;
};

class ZBackendEventQueue
{
public:
    // 不拥有后端，后端生命周期必须长于队列
    explicit ZBackendEventQueue(IInputBackend& Backend);
    ~ZBackendEventQueue();

    // 禁止拷贝和移动，队列持有后端引用和回调绑定
    ZBackendEventQueue(const ZBackendEventQueue&) = delete;
    ZBackendEventQueue& operator=(const ZBackendEventQueue&) = delete;
    ZBackendEventQueue(ZBackendEventQueue&&) = delete;
    ZBackendEventQueue& operator=(ZBackendEventQueue&&) = delete;

    // 订阅后端三个回调。重复调用不重复绑定、不清空队列。
    // 如果后端已有非空回调，输出警告后覆盖。
    void Attach();

    // 取消后端回调订阅。重复调用安全。
    void Detach();

    // 查询是否已订阅后端回调
    ZERO_NODISCARD bool IsAttached() const noexcept;

    // 按 FIFO 返回所有待处理事件并清空内部队列。线程安全。
    ZERO_NODISCARD TVector<SBackendEvent> DrainEvents();

    // 返回当前待处理事件数量。线程安全。
    ZERO_NODISCARD uint32 GetPendingEventCount() const;

    // 丢弃所有待处理事件，不改变 attach 状态。线程安全。
    void Clear();

private:
    // 回调入队方法，由后端回调在任意线程调用
    void EnqueueDeviceConnected(const SDeviceInfo& DeviceInfo);
    void EnqueueDeviceDisconnected(const SDeviceId& DeviceId);
    void EnqueueInputEvent(const SInputEvent& Event);

    IInputBackend& Backend;
    bool bAttached = false;

    mutable std::mutex Mutex;
    TVector<SBackendEvent> PendingEvents;
};

}  // namespace ZeroMapper
