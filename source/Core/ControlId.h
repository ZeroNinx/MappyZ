// 标准控件命名约定。
// 定义项目内部使用的控件标识字符串常量，与 profile JSON 中的 control_id 字段对齐。
// 控件标识统一使用 snake_case，保留字符串开放形式以适应不同后端的映射需求。

#pragma once

#include "Core/ProjectCore.h"

namespace ZeroMapper
{

// 控件标识使用 StdStringView 常量，避免运行时分配
namespace ControlId
{

// ── 面板按钮（Xbox 布局，使用方位命名以适配不同手柄品牌） ──

inline constexpr StdStringView ButtonSouth   = "button_south";
inline constexpr StdStringView ButtonEast    = "button_east";
inline constexpr StdStringView ButtonWest    = "button_west";
inline constexpr StdStringView ButtonNorth   = "button_north";

// ── 功能按钮 ──

inline constexpr StdStringView ButtonStart   = "button_start";
inline constexpr StdStringView ButtonBack    = "button_back";
inline constexpr StdStringView ButtonGuide   = "button_guide";

// ── 摇杆按下 ──

inline constexpr StdStringView LeftStickButton  = "left_stick_button";
inline constexpr StdStringView RightStickButton = "right_stick_button";

// ── 肩键 ──

inline constexpr StdStringView LeftShoulder  = "left_shoulder";
inline constexpr StdStringView RightShoulder = "right_shoulder";

// ── 扳机（模拟量） ──

inline constexpr StdStringView LeftTrigger   = "left_trigger";
inline constexpr StdStringView RightTrigger  = "right_trigger";

// ── 摇杆（双轴） ──

inline constexpr StdStringView LeftStick     = "left_stick";
inline constexpr StdStringView RightStick    = "right_stick";

// ── 方向键 ──

inline constexpr StdStringView DpadUp        = "dpad_up";
inline constexpr StdStringView DpadDown      = "dpad_down";
inline constexpr StdStringView DpadLeft      = "dpad_left";
inline constexpr StdStringView DpadRight     = "dpad_right";

}  // namespace ControlId

}  // namespace ZeroMapper
