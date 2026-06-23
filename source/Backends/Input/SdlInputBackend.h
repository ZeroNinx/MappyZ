// SDL3 真实输入后端，通过 SDL Gamepad API 读取物理手柄输入。
// 使用 Pimpl 隔离 SDL 头文件，本头文件不包含任何 SDL 类型。
// 后端通过独立 worker 线程轮询 SDL 事件，回调从 worker 线程触发。
//
// 线程安全说明：
// - Start() / Stop() / IsRunning() / ListDevices() 可从任意线程调用
// - 所有回调（OnDeviceConnected、OnDeviceDisconnected、OnInputEvent）从 worker 线程触发
// - 使用方（Runtime、UI Bridge）需自行处理跨线程同步

#pragma once

#include "Backends/Input/InputBackend.h"

namespace MappyZ
{

class ZSdlInputBackend final : public IInputBackend
{
public:
    ZSdlInputBackend();

    // 析构函数在 .cpp 中定义，确保 Pimpl 类型完整可析构
    ~ZSdlInputBackend() override;

    // ── IInputBackend 实现 ──

    NODISCARD TResult<void> Start() override;
    void Stop() override;
    NODISCARD bool IsRunning() const noexcept override;
    NODISCARD TVector<SDeviceInfo> ListDevices() const override;

private:
    // Pimpl：SDL handle、设备 map、轴缓存、worker 线程全部封装在 SImpl 中
    struct SImpl;
    TUniquePtr<SImpl> Impl;
};

}  // namespace MappyZ
