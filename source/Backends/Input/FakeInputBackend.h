// 假输入后端，用于单元测试和无硬件集成测试。
// 不启动真实输入线程，不访问 SDL 或平台 API。
// 测试代码通过 AddDevice / RemoveDevice / EmitInput 主动注入设备和事件，
// 验证 Runtime 和 UI Bridge 对 IInputBackend 回调的响应。
//
// 本类不做线程安全承诺，所有调用应在同一线程完成。

#pragma once

#include "Backends/Input/InputBackend.h"

namespace ZeroMapper
{

class ZFakeInputBackend final : public IInputBackend
{
public:
    ZFakeInputBackend() = default;
    ~ZFakeInputBackend() override = default;

    // ── IInputBackend 实现 ──

    ZERO_NODISCARD TResult<void> Start() override;
    void Stop() override;
    ZERO_NODISCARD bool IsRunning() const noexcept override;
    ZERO_NODISCARD TVector<SDeviceInfo> ListDevices() const override;

    // ── 测试注入接口 ──

    // 向设备快照中添加设备。running 状态下同步触发 OnDeviceConnected。
    // 如果设备 ID 已存在，不重复添加、不触发回调，输出警告日志。
    void AddDevice(const SDeviceInfo& DeviceInfo);

    // 从设备快照中移除设备。running 状态下同步触发 OnDeviceDisconnected。
    // 如果设备 ID 不存在，不触发回调，输出警告日志。
    void RemoveDevice(const SDeviceId& DeviceId);

    // 注入输入事件。running 状态下同步触发 OnInputEvent。
    // stopped 状态下不触发回调，输出调试日志。
    void EmitInput(const SInputEvent& Event);

private:
    bool bRunning = false;
    TVector<SDeviceInfo> Devices;
};

}  // namespace ZeroMapper
