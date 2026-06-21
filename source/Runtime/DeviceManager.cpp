// ZDeviceManager 实现。
// 所有非预期路径（重复设备、未知设备断开、状态不一致）都输出日志以方便定位问题。

#include "Runtime/DeviceManager.h"

#include <algorithm>
#include <cstdio>

namespace ZeroMapper
{

ZDeviceManager::ZDeviceManager(IInputBackend& Backend)
    : Backend(Backend)
{
}

ZDeviceManager::~ZDeviceManager()
{
    // best-effort 清理回调，避免 manager 销毁后回调悬垂
    Detach();
}

void ZDeviceManager::Attach()
{
    if (bAttached)
    {
        std::fprintf(stderr, "[DeviceManager] 调试: 已处于 attached 状态，跳过重复订阅\n");
        return;
    }

    // 订阅后端设备回调
    // 本轮约定 manager attach 期间不允许外部改写后端的设备回调
    Backend.OnDeviceConnected = [this](const SDeviceInfo& DeviceInfo)
    {
        HandleDeviceConnected(DeviceInfo);
    };

    Backend.OnDeviceDisconnected = [this](const SDeviceId& DeviceId)
    {
        HandleDeviceDisconnected(DeviceId);
    };

    // 读取后端当前已知设备作为初始快照
    Devices = Backend.ListDevices();

    bAttached = true;
}

void ZDeviceManager::Detach()
{
    if (!bAttached)
    {
        return;
    }

    // 清空后端回调，避免 manager 销毁后回调指向已释放内存
    Backend.OnDeviceConnected = nullptr;
    Backend.OnDeviceDisconnected = nullptr;

    bAttached = false;
}

bool ZDeviceManager::IsAttached() const noexcept
{
    return bAttached;
}

TVector<SDeviceInfo> ZDeviceManager::ListDevices() const
{
    return Devices;
}

TOptional<SDeviceInfo> ZDeviceManager::FindDevice(const SDeviceId& DeviceId) const
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
        return std::nullopt;
    }

    return *Iterator;
}

uint32 ZDeviceManager::GetDeviceCount() const noexcept
{
    return static_cast<uint32>(Devices.size());
}

void ZDeviceManager::HandleDeviceConnected(const SDeviceInfo& DeviceInfo)
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
            "[DeviceManager] 警告: 设备 ID \"%s\" 已存在于快照中，跳过添加\n",
            DeviceInfo.Id.Value.c_str());
        return;
    }

    Devices.push_back(DeviceInfo);
}

void ZDeviceManager::HandleDeviceDisconnected(const SDeviceId& DeviceId)
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
            "[DeviceManager] 警告: 设备 ID \"%s\" 不在快照中，忽略断开事件\n",
            DeviceId.Value.c_str());
        return;
    }

    Devices.erase(Iterator);
}

}  // namespace ZeroMapper
