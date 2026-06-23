// 输入后端统一接口。
// 所有输入后端（SDL3、假后端等）都必须实现此接口。
// Runtime 通过此接口获取设备列表、订阅设备连接/断开和输入事件，不直接依赖具体后端实现。
//
// 本接口只依赖 Core 层数据类型和标准库回调，不引入 Qt、SDL 或平台 API。

#pragma once

#include "Core/DeviceId.h"
#include "Core/InputEvent.h"
#include "Core/ProjectCore.h"

#include <functional>

namespace MappyZ
{

class IInputBackend
{
public:
    virtual ~IInputBackend() = default;

    // 禁止拷贝和移动，后端持有回调和内部状态，所有权语义不适合拷贝
    IInputBackend(const IInputBackend&) = delete;
    IInputBackend& operator=(const IInputBackend&) = delete;
    IInputBackend(IInputBackend&&) = delete;
    IInputBackend& operator=(IInputBackend&&) = delete;

    // 启动后端，开始监听设备和输入事件。失败时通过 TResult 返回错误信息。
    NODISCARD virtual TResult<void> Start() = 0;

    // 停止后端，释放所有后端资源。幂等操作，多次调用不应失败。
    virtual void Stop() = 0;

    // 查询后端是否正在运行
    NODISCARD virtual bool IsRunning() const noexcept = 0;

    // 返回当前后端已知的设备快照。返回值为拷贝，调用方修改不影响后端内部状态。
    NODISCARD virtual TVector<SDeviceInfo> ListDevices() const = 0;

public:
    // 设备连接回调，后端检测到新设备时触发
    std::function<void(const SDeviceInfo& DeviceInfo)> OnDeviceConnected;

    // 设备断开回调，后端检测到设备移除时触发
    std::function<void(const SDeviceId& DeviceId)> OnDeviceDisconnected;

    // 输入事件回调，后端收到标准化输入事件时触发
    std::function<void(const SInputEvent& Event)> OnInputEvent;

protected:
    // 只允许子类构造
    IInputBackend() = default;
};

}  // namespace MappyZ
