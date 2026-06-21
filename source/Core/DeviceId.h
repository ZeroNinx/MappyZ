// 设备标识与设备信息定义。
// SDeviceId 是设备在运行时的唯一标识，用于事件路由和状态查询。
// SDeviceInfo 是设备的完整描述信息，用于 UI 展示和 profile 匹配。
//
// 本文件有意放在 Core 而非 Runtime，因为 IInputBackend（Backend 层）需要
// 返回 TVector<SDeviceInfo>，放在 Core 可避免 Backend 反向依赖 Runtime。

#pragma once

#include "Core/ProjectCore.h"

namespace ZeroMapper
{

// 设备在运行时的唯一标识，由后端生成，整个会话内保持稳定
struct SDeviceId
{
    StdString Value;
};

// 设备的完整描述信息，供 UI 展示和 profile 设备匹配使用
struct SDeviceInfo
{
    // 运行时唯一标识
    SDeviceId Id;

    // 设备名称，来自后端或驱动报告
    StdString Name;

    // 输入后端标识，例如 "sdl3"
    StdString Backend;

    // USB Vendor ID，十六进制字符串，例如 "045e"
    StdString VendorId;

    // USB Product ID，十六进制字符串，例如 "02fd"
    StdString ProductId;

    // 后端提供的全局唯一标识符，例如 SDL GUID
    StdString Guid;

    // 系统级实例标识，用于区分同型号的多个设备
    StdString InstanceId;
};

inline bool operator==(const SDeviceId& Left, const SDeviceId& Right)
{
    return Left.Value == Right.Value;
}

inline bool operator!=(const SDeviceId& Left, const SDeviceId& Right)
{
    return !(Left == Right);
}

}  // namespace ZeroMapper
