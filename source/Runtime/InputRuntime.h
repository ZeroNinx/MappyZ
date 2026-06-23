// 运行时输入状态管理器。
// 消费 IInputBackend 的输入事件回调，维护最近事件列表和当前控件状态快照。
// 为后续映射引擎、输入调试 UI 和热插拔清理提供稳定的输入查询入口。
//
// 本类不拥有输入后端，通过引用接收。
// 本类只订阅 OnInputEvent，设备生命周期归 ZDeviceManager 管理。
// 本轮不做线程安全承诺，所有调用应在同一线程完成。

#pragma once

#include "Backends/Input/InputBackend.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

class ZInputRuntime
{
public:
    // 最近事件列表的固定容量上限
    static constexpr uint32 MaxRecentEvents = 256;

    // 不拥有后端，后端生命周期必须长于 runtime
    explicit ZInputRuntime(IInputBackend& Backend);
    ~ZInputRuntime();

    // 禁止拷贝和移动，runtime 持有后端引用和回调绑定
    ZInputRuntime(const ZInputRuntime&) = delete;
    ZInputRuntime& operator=(const ZInputRuntime&) = delete;
    ZInputRuntime(ZInputRuntime&&) = delete;
    ZInputRuntime& operator=(ZInputRuntime&&) = delete;

    // 订阅后端输入事件回调。可重复调用，重复调用不会重复订阅或清空已有状态。
    void Attach();

    // 取消后端输入事件回调订阅。可重复调用，重复调用安全无副作用。
    void Detach();

    // 查询是否已订阅后端回调
    NODISCARD bool IsAttached() const noexcept;

    // 返回最近事件列表拷贝，调用方修改返回值不影响 runtime 内部状态
    NODISCARD TVector<SInputEvent> ListRecentEvents() const;

    // 返回最近事件数量
    NODISCARD uint32 GetRecentEventCount() const noexcept;

    // 按设备 ID 和控件 ID 查找当前控件状态，找到时返回事件副本，找不到时返回空
    NODISCARD TOptional<SInputEvent> FindControlState(const SDeviceId& DeviceId, StdStringView ControlId) const;

    // 返回当前已追踪的控件数量
    NODISCARD uint32 GetTrackedControlCount() const noexcept;

    // 清空最近事件和当前控件状态，但不改变 attach 状态
    void Clear();

private:
    // 后端输入事件回调处理
    void HandleInputEvent(const SInputEvent& Event);

    // 组合 (DeviceId, ControlId) 为 HashMap 的键
    static StdString MakeControlStateKey(const SDeviceId& DeviceId, StdStringView ControlId);

    IInputBackend& Backend;
    bool bAttached = false;

    TVector<SInputEvent> RecentEvents;
    THashMap<StdString, SInputEvent> ControlStates;
};

}  // namespace MappyZ
