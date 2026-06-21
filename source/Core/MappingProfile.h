// 映射 Profile 数据契约。
// 定义设备匹配条件和完整 profile 快照结构。
// Profile 是运行时可替换的数据快照，映射引擎只消费此结构，不直接读取 JSON 或 UI 状态。
// Core 层纯数据类型，不依赖 Runtime、Backend、UI 或平台 API。

#pragma once

#include "Core/MappingRule.h"
#include "Core/ProjectCore.h"

namespace ZeroMapper
{

// 设备匹配条件，用于 profile 与物理设备的关联
// 所有字段都可为空，后续 profile 匹配模块按可用字段评分
struct SDeviceMatch
{
    // 设备名称
    StdString Name;

    // 输入后端标识
    StdString Backend;

    // 厂商 ID
    StdString VendorId;

    // 产品 ID
    StdString ProductId;

    // 设备 GUID
    StdString Guid;

    // 设备实例 ID
    StdString InstanceId;
};

// 映射 Profile 快照，包含设备匹配条件和一组映射规则
struct SMappingProfile
{
    // Profile 数据格式版本，用于后续 schema 迁移
    uint32 SchemaVersion = 1;

    // Profile 标识
    StdString Id;

    // Profile 显示名称
    StdString Name;

    // 是否启用此 profile，禁用的 profile 不参与映射
    bool bEnabled = true;

    // 设备匹配条件
    SDeviceMatch DeviceMatch;

    // 映射规则列表，空列表合法，表示没有映射规则
    TVector<SMappingRule> Rules;
};

}  // namespace ZeroMapper
