// ShouldShowMainWindow 启动策略测试。
// 纯函数无 Qt 依赖，直接覆盖真值表四种组合与两个边界断言。

#include <catch2/catch_test_macros.hpp>

#include "App/StartupPolicy.h"

using namespace MappyZ;

TEST_CASE("ShouldShowMainWindow covers the four truth table combinations",
    "[App][Startup]")
{
    // 仅 (托盘可用 && 启动即最小化) 隐藏，其余组合一律显示。
    REQUIRE(ShouldShowMainWindow(false, false));
    REQUIRE(ShouldShowMainWindow(false, true));
    REQUIRE(ShouldShowMainWindow(true, false));
    REQUIRE_FALSE(ShouldShowMainWindow(true, true));
}

TEST_CASE("Tray unavailable always shows even when start minimized is true",
    "[App][Startup]")
{
    REQUIRE(ShouldShowMainWindow(false, true));
}

TEST_CASE("Tray available with start minimized true hides the window",
    "[App][Startup]")
{
    REQUIRE_FALSE(ShouldShowMainWindow(true, true));
}
