// 运行时设备管理器。
// 消费 IInputBackend 的设备连接/断开回调，维护运行时设备快照。
// 为后续 UI Bridge、输入运行时和热插拔流程提供稳定的设备查询入口。
//
// 本类不拥有输入后端，通过引用接收。
// 本轮不做线程安全承诺，所有调用应在同一线程完成。
// 本轮不订阅 OnInputEvent，输入事件归后续 ZInputRuntime 模块处理。

#pragma once

#include "Backends/Input/InputBackend.h"
#include "Core/DeviceId.h"
#include "Core/ProjectCore.h"

namespace ZeroMapper
{

class ZDeviceManager
{
public:
    // 不拥有后端，后端生命周期必须长于 manager
    explicit ZDeviceManager(IInputBackend& Backend);
    ~ZDeviceManager();

    // 禁止拷贝和移动，manager 持有后端引用和回调绑定
    ZDeviceManager(const ZDeviceManager&) = delete;
    ZDeviceManager& operator=(const ZDeviceManager&) = delete;
    ZDeviceManager(ZDeviceManager&&) = delete;
    ZDeviceManager& operator=(ZDeviceManager&&) = delete;

    // 订阅后端设备回调并读取初始设备快照。可重复调用，重复调用不会重复订阅或重复插入设备。
    void Attach();

    // 取消后端设备回调订阅并清空回调。可重复调用，重复调用安全无副作用。
    void Detach();

    // 查询是否已订阅后端回调
    ZERO_NODISCARD bool IsAttached() const noexcept;

    // 返回当前设备快照拷贝，调用方修改返回值不影响 manager 内部状态
    ZERO_NODISCARD TVector<SDeviceInfo> ListDevices() const;

    // 按设备 ID 查找设备，找到时返回设备副本，找不到时返回空
    ZERO_NODISCARD TOptional<SDeviceInfo> FindDevice(const SDeviceId& DeviceId) const;

    // 返回当前已知设备数量
    ZERO_NODISCARD uint32 GetDeviceCount() const noexcept;

private:
    // 后端设备连接回调处理
    void HandleDeviceConnected(const SDeviceInfo& DeviceInfo);

    // 后端设备断开回调处理
    void HandleDeviceDisconnected(const SDeviceId& DeviceId);

    IInputBackend& Backend;
    bool bAttached = false;
    TVector<SDeviceInfo> Devices;
};

}  // namespace ZeroMapper
