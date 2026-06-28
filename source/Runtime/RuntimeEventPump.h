// 运行时事件泵。
// 从 ZBackendEventQueue 批量取走后端事件，在调用线程内按 FIFO 分发。
// 输入事件送入 ZMappingSession 执行映射和输出派发；
// 设备事件和输入事件也提供轻量回调，供 RuntimeHost 或 UI Bridge 更新状态。
//
// 本类不拥有 EventQueue 和 MappingSession，通过引用接收。
// PumpOnce() 在调用线程同步执行，不引入额外线程。
// Runtime 层，不依赖 Qt、QML、SDL、Win32 或 nlohmann_json。

#pragma once

#include "Core/DeviceId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"
#include "Runtime/BackendEventQueue.h"
#include "Runtime/MappingSession.h"
#include "Runtime/StickDirectionSynthesizer.h"

#include <functional>

namespace MappyZ
{

// 单次 PumpOnce 的汇总统计
struct SRuntimeEventPumpSummary
{
    uint32 DrainedEventCount = 0;
    uint32 DeviceConnectedCount = 0;
    uint32 DeviceDisconnectedCount = 0;
    uint32 InputEventCount = 0;
    uint32 MappedInputCount = 0;
    uint32 DispatchedInputCount = 0;
    uint32 FailedDispatchInputCount = 0;
    uint32 InvalidEventCount = 0;
    StdString Message;
};

// 单个事件的处理记录
// MappingResult 仅在 Event.Type == Input 且 payload 有效时有业务意义；
// 设备事件和 invalid 事件的该字段保持默认值，消费方不应读取。
struct SRuntimeEventPumpRecord
{
    SBackendEvent Event;
    bool bHandled = false;
    SMappingSessionResult MappingResult;
    StdString Message;
};

class ZRuntimeEventPump final
{
public:
    static constexpr uint32 MaxRecentRecords = 128;

    ZRuntimeEventPump(ZBackendEventQueue& EventQueue, ZMappingSession& MappingSession);

    // 禁止拷贝和移动
    ZRuntimeEventPump(const ZRuntimeEventPump&) = delete;
    ZRuntimeEventPump& operator=(const ZRuntimeEventPump&) = delete;
    ZRuntimeEventPump(ZRuntimeEventPump&&) = delete;
    ZRuntimeEventPump& operator=(ZRuntimeEventPump&&) = delete;

    // 从 EventQueue drain 事件并逐个分发，返回本次汇总统计
    NODISCARD SRuntimeEventPumpSummary PumpOnce();

    // 处理外部提供的事件数组，不经过 EventQueue drain。
    // 用于测试注入手工构造的畸形事件，验证防御性路径。
    NODISCARD SRuntimeEventPumpSummary PumpEvents(TVector<SBackendEvent> Events);

    // 设置设备连接事件回调
    void SetDeviceConnectedHandler(std::function<void(const SDeviceInfo&)> Handler);

    // 设置设备断开事件回调
    void SetDeviceDisconnectedHandler(std::function<void(const SDeviceId&)> Handler);

    // 设置输入事件回调（先于 MappingSession 执行）
    void SetInputEventHandler(std::function<void(const SInputEvent&)> Handler);

    // 返回最近处理记录的快照拷贝
    NODISCARD TVector<SRuntimeEventPumpRecord> ListRecentRecords() const;

    // 返回当前记录数量
    NODISCARD uint32 GetRecentRecordCount() const noexcept;

    // 清空处理记录，不影响 handlers、EventQueue 或 MappingSession
    void ClearRecentRecords();

    // 清空摇杆方向合成器状态（设备断开或全局清理时自动调用）
    void ClearStickDirectionState();

    // 清空指定设备的摇杆方向合成器状态
    void ClearStickDirectionStateForDevice(const SDeviceId& DeviceId);

private:
    // 处理单个后端事件，更新 summary 并追加 record
    void ProcessEvent(const SBackendEvent& Event, SRuntimeEventPumpSummary& Summary);

    // 追加记录，超过容量时丢弃最旧
    void AppendRecord(SRuntimeEventPumpRecord Record);

    ZBackendEventQueue& EventQueue;
    ZMappingSession& MappingSession;

    std::function<void(const SDeviceInfo&)> DeviceConnectedHandler;
    std::function<void(const SDeviceId&)> DeviceDisconnectedHandler;
    std::function<void(const SInputEvent&)> InputEventHandler;

    TVector<SRuntimeEventPumpRecord> RecentRecords;

    ZStickDirectionSynthesizer StickDirectionSynthesizer;
};

}  // namespace MappyZ
