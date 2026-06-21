// 项目公共基础头。
// 所有 Core 模块头文件应首先包含此头，通过 using 声明获取 ZeroStyle 提供的类型别名和宏。
// 业务代码统一使用 ZeroMapper 命名空间内的名称，不直接使用 Zero:: 或 std:: 前缀。

#pragma once

#include "ZeroStyle.h"

namespace ZeroMapper
{

// ── 基础数值类型 ──

using Zero::float32;
using Zero::float64;
using Zero::int8;
using Zero::int16;
using Zero::int32;
using Zero::int64;
using Zero::uint8;
using Zero::uint16;
using Zero::uint32;
using Zero::uint64;

// ── 字符串与路径 ──

using Zero::StdPath;
using Zero::StdString;
using Zero::StdStringView;

// ── 智能指针与可选值 ──

using Zero::TOptional;
using Zero::TSharedPtr;
using Zero::TUniquePtr;
using Zero::TWeakPtr;

// ── 容器 ──

using Zero::TVector;
using Zero::TArray;
using Zero::TMap;
using Zero::THashMap;
using Zero::TSet;

// ── 错误处理 ──

using Zero::EErrorCode;
using Zero::SError;
using Zero::TResult;

}  // namespace ZeroMapper
