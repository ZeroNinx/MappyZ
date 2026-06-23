// ZFakeInputBackend 实现。
// 所有非预期路径（重复设备、缺少回调、错误状态下调用）都输出日志以方便定位问题。

#include "Backends/Input/FakeInputBackend.h"

#include <algorithm>
#include <cstdio>

namespace MappyZ
{

TResult<void> ZFakeInputBackend::Start()
{
    if (bRunning)
    {
        return TResult<void>::Ok();
    }

    if (InjectedStartError.has_value())
    {
        return TResult<void>::Err(
            MakeError(EErrorCode::Unknown, InjectedStartError.value()));
    }

    bRunning = true;
    return TResult<void>::Ok();
}

void ZFakeInputBackend::Stop()
{
    bRunning = false;
}

bool ZFakeInputBackend::IsRunning() const noexcept
{
    return bRunning;
}

TVector<SDeviceInfo> ZFakeInputBackend::ListDevices() const
{
    return Devices;
}

void ZFakeInputBackend::SetStartError(StdString Message)
{
    InjectedStartError = std::move(Message);
}

void ZFakeInputBackend::ClearStartError()
{
    InjectedStartError.reset();
}

void ZFakeInputBackend::AddDevice(const SDeviceInfo& DeviceInfo)
{
    // 检查设备 ID 是否已存在，避免重复添加
    auto Iterator = std::find_if(
        Devices.begin(),
        Devices.end(),
        [&DeviceInfo](const SDeviceInfo& Existing)
        {
            return Existing.Id == DeviceInfo.Id;
        });

    if (Iterator != Devices.end())
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 警告: 设备 ID \"%s\" 已存在，跳过添加\n",
            DeviceInfo.Id.Value.c_str());
        return;
    }

    Devices.push_back(DeviceInfo);

    if (!bRunning)
    {
        return;
    }

    if (!OnDeviceConnected)
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 调试: 设备 \"%s\" 已连接，但 OnDeviceConnected 未设置\n",
            DeviceInfo.Id.Value.c_str());
        return;
    }

    OnDeviceConnected(DeviceInfo);
}

void ZFakeInputBackend::RemoveDevice(const SDeviceId& DeviceId)
{
    auto Iterator = std::find_if(
        Devices.begin(),
        Devices.end(),
        [&DeviceId](const SDeviceInfo& Existing)
        {
            return Existing.Id == DeviceId;
        });

    if (Iterator == Devices.end())
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 警告: 设备 ID \"%s\" 不存在，跳过移除\n",
            DeviceId.Value.c_str());
        return;
    }

    Devices.erase(Iterator);

    if (!bRunning)
    {
        return;
    }

    if (!OnDeviceDisconnected)
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 调试: 设备 \"%s\" 已断开，但 OnDeviceDisconnected 未设置\n",
            DeviceId.Value.c_str());
        return;
    }

    OnDeviceDisconnected(DeviceId);
}

void ZFakeInputBackend::EmitInput(const SInputEvent& Event)
{
    if (!bRunning)
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 调试: 后端未运行，忽略输入事件 (控件: \"%s\")\n",
            Event.ControlId.c_str());
        return;
    }

    if (!OnInputEvent)
    {
        std::fprintf(
            stderr,
            "[FakeInputBackend] 调试: 收到输入事件 (控件: \"%s\")，但 OnInputEvent 未设置\n",
            Event.ControlId.c_str());
        return;
    }

    OnInputEvent(Event);
}

}  // namespace MappyZ
