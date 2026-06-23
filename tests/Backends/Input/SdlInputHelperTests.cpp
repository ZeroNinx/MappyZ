// SdlInputHelpers 纯转换函数测试。
// 不依赖 SDL 运行时，验证按钮映射、轴归一化和轴映射逻辑。
// 这些测试在没有安装 SDL3 的环境下也能编译和运行。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/SdlInputHelpers.h"
#include "Core/ControlId.h"

using namespace MappyZ;
using namespace MappyZ::SdlInputHelpers;

// ── 构造辅助 ──

// 验证按钮映射的辅助函数
static void RequireButtonMapping(int SdlButtonValue, StdStringView ExpectedControlId)
{
    auto Result = MapButtonToControlId(SdlButtonValue);
    REQUIRE(Result.has_value());
    REQUIRE(Result.value() == ExpectedControlId);
}

// ── 按钮映射 ──

TEST_CASE("MapButtonToControlId maps face buttons", "[SdlHelpers][Button]")
{
    RequireButtonMapping(SdlButton::South, ControlId::ButtonSouth);
    RequireButtonMapping(SdlButton::East, ControlId::ButtonEast);
    RequireButtonMapping(SdlButton::West, ControlId::ButtonWest);
    RequireButtonMapping(SdlButton::North, ControlId::ButtonNorth);
}

TEST_CASE("MapButtonToControlId maps function buttons", "[SdlHelpers][Button]")
{
    RequireButtonMapping(SdlButton::Start, ControlId::ButtonStart);
    RequireButtonMapping(SdlButton::Back, ControlId::ButtonBack);
    RequireButtonMapping(SdlButton::Guide, ControlId::ButtonGuide);
}

TEST_CASE("MapButtonToControlId maps shoulder buttons", "[SdlHelpers][Button]")
{
    RequireButtonMapping(SdlButton::LeftShoulder, ControlId::LeftShoulder);
    RequireButtonMapping(SdlButton::RightShoulder, ControlId::RightShoulder);
}

TEST_CASE("MapButtonToControlId maps stick buttons", "[SdlHelpers][Button]")
{
    RequireButtonMapping(SdlButton::LeftStick, ControlId::LeftStickButton);
    RequireButtonMapping(SdlButton::RightStick, ControlId::RightStickButton);
}

TEST_CASE("MapButtonToControlId maps dpad buttons", "[SdlHelpers][Button]")
{
    RequireButtonMapping(SdlButton::DpadUp, ControlId::DpadUp);
    RequireButtonMapping(SdlButton::DpadDown, ControlId::DpadDown);
    RequireButtonMapping(SdlButton::DpadLeft, ControlId::DpadLeft);
    RequireButtonMapping(SdlButton::DpadRight, ControlId::DpadRight);
}

TEST_CASE("MapButtonToControlId returns nullopt for unknown button", "[SdlHelpers][Button]")
{
    // MISC1 (15) 及以上的按钮不在支持范围内
    REQUIRE_FALSE(MapButtonToControlId(15).has_value());
    REQUIRE_FALSE(MapButtonToControlId(20).has_value());
    REQUIRE_FALSE(MapButtonToControlId(-1).has_value());
    REQUIRE_FALSE(MapButtonToControlId(999).has_value());
}

// ── 轴映射 ──

TEST_CASE("MapAxisToMapping maps left stick axes", "[SdlHelpers][Axis]")
{
    auto LeftX = MapAxisToMapping(SdlAxis::LeftX);
    REQUIRE(LeftX.has_value());
    REQUIRE(LeftX->ControlId == ControlId::LeftStick);
    REQUIRE(LeftX->ControlType == EInputControlType::Axis2D);
    REQUIRE(LeftX->bIsXAxis == true);

    auto LeftY = MapAxisToMapping(SdlAxis::LeftY);
    REQUIRE(LeftY.has_value());
    REQUIRE(LeftY->ControlId == ControlId::LeftStick);
    REQUIRE(LeftY->ControlType == EInputControlType::Axis2D);
    REQUIRE(LeftY->bIsXAxis == false);
}

TEST_CASE("MapAxisToMapping maps right stick axes", "[SdlHelpers][Axis]")
{
    auto RightX = MapAxisToMapping(SdlAxis::RightX);
    REQUIRE(RightX.has_value());
    REQUIRE(RightX->ControlId == ControlId::RightStick);
    REQUIRE(RightX->ControlType == EInputControlType::Axis2D);
    REQUIRE(RightX->bIsXAxis == true);

    auto RightY = MapAxisToMapping(SdlAxis::RightY);
    REQUIRE(RightY.has_value());
    REQUIRE(RightY->ControlId == ControlId::RightStick);
    REQUIRE(RightY->ControlType == EInputControlType::Axis2D);
    REQUIRE(RightY->bIsXAxis == false);
}

TEST_CASE("MapAxisToMapping maps trigger axes", "[SdlHelpers][Axis]")
{
    auto LeftTrigger = MapAxisToMapping(SdlAxis::LeftTrigger);
    REQUIRE(LeftTrigger.has_value());
    REQUIRE(LeftTrigger->ControlId == ControlId::LeftTrigger);
    REQUIRE(LeftTrigger->ControlType == EInputControlType::Trigger);

    auto RightTrigger = MapAxisToMapping(SdlAxis::RightTrigger);
    REQUIRE(RightTrigger.has_value());
    REQUIRE(RightTrigger->ControlId == ControlId::RightTrigger);
    REQUIRE(RightTrigger->ControlType == EInputControlType::Trigger);
}

TEST_CASE("MapAxisToMapping returns nullopt for unknown axis", "[SdlHelpers][Axis]")
{
    REQUIRE_FALSE(MapAxisToMapping(-1).has_value());
    REQUIRE_FALSE(MapAxisToMapping(6).has_value());
    REQUIRE_FALSE(MapAxisToMapping(999).has_value());
}

// ── 摇杆轴归一化 ──

TEST_CASE("NormalizeStickAxis normalizes center to zero", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeStickAxis(0) == 0.0f);
}

TEST_CASE("NormalizeStickAxis normalizes positive maximum to 1.0", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeStickAxis(32767) == 1.0f);
}

TEST_CASE("NormalizeStickAxis normalizes negative maximum to -1.0", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeStickAxis(-32768) == -1.0f);
}

TEST_CASE("NormalizeStickAxis normalizes positive intermediate value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeStickAxis(16384);

    // 16384 / 32767 ≈ 0.50002，确认在合理范围内
    REQUIRE(Result > 0.49f);
    REQUIRE(Result < 0.51f);
}

TEST_CASE("NormalizeStickAxis normalizes negative intermediate value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeStickAxis(-16384);

    // -16384 / 32768 = -0.5，精确值
    REQUIRE(Result == -0.5f);
}

TEST_CASE("NormalizeStickAxis normalizes small positive value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeStickAxis(1);
    REQUIRE(Result > 0.0f);
    REQUIRE(Result < 0.001f);
}

TEST_CASE("NormalizeStickAxis normalizes small negative value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeStickAxis(-1);
    REQUIRE(Result < 0.0f);
    REQUIRE(Result > -0.001f);
}

// ── 扳机轴归一化 ──

TEST_CASE("NormalizeTriggerAxis normalizes zero to 0.0", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeTriggerAxis(0) == 0.0f);
}

TEST_CASE("NormalizeTriggerAxis normalizes maximum to 1.0", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeTriggerAxis(32767) == 1.0f);
}

TEST_CASE("NormalizeTriggerAxis clamps negative to 0.0", "[SdlHelpers][Normalize]")
{
    REQUIRE(NormalizeTriggerAxis(-1) == 0.0f);
    REQUIRE(NormalizeTriggerAxis(-32768) == 0.0f);
}

TEST_CASE("NormalizeTriggerAxis normalizes intermediate value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeTriggerAxis(16384);

    // 16384 / 32767 ≈ 0.50002
    REQUIRE(Result > 0.49f);
    REQUIRE(Result < 0.51f);
}

TEST_CASE("NormalizeTriggerAxis normalizes small positive value", "[SdlHelpers][Normalize]")
{
    float32 Result = NormalizeTriggerAxis(1);
    REQUIRE(Result > 0.0f);
    REQUIRE(Result < 0.001f);
}

// ── Axis2D 合并 ──

TEST_CASE("MergeAxis2DCache X then Y produces correct merged value", "[SdlHelpers][Merge]")
{
    SAxis2DValue Cache;

    // 先收到 X 轴事件
    auto AfterX = MergeAxis2DCache(Cache, true, 0.5f);
    REQUIRE(AfterX.X == 0.5f);
    REQUIRE(AfterX.Y == 0.0f);

    // 再收到 Y 轴事件，应保留之前的 X 值
    auto AfterY = MergeAxis2DCache(Cache, false, -0.3f);
    REQUIRE(AfterY.X == 0.5f);
    REQUIRE(AfterY.Y == -0.3f);
}

TEST_CASE("MergeAxis2DCache Y then X produces correct merged value", "[SdlHelpers][Merge]")
{
    SAxis2DValue Cache;

    // 先收到 Y 轴事件
    auto AfterY = MergeAxis2DCache(Cache, false, 0.8f);
    REQUIRE(AfterY.X == 0.0f);
    REQUIRE(AfterY.Y == 0.8f);

    // 再收到 X 轴事件，应保留之前的 Y 值
    auto AfterX = MergeAxis2DCache(Cache, true, -0.6f);
    REQUIRE(AfterX.X == -0.6f);
    REQUIRE(AfterX.Y == 0.8f);
}

TEST_CASE("MergeAxis2DCache repeated same axis updates value", "[SdlHelpers][Merge]")
{
    SAxis2DValue Cache;

    (void)MergeAxis2DCache(Cache, true, 0.1f);
    (void)MergeAxis2DCache(Cache, true, 0.9f);

    // 缓存中 X 应为最新值，Y 保持默认
    REQUIRE(Cache.X == 0.9f);
    REQUIRE(Cache.Y == 0.0f);
}

TEST_CASE("MergeAxis2DCache updates cache in place", "[SdlHelpers][Merge]")
{
    SAxis2DValue Cache;

    (void)MergeAxis2DCache(Cache, true, 0.5f);
    (void)MergeAxis2DCache(Cache, false, -0.7f);

    // 验证 Cache 本身被修改（返回值和 Cache 一致）
    REQUIRE(Cache.X == 0.5f);
    REQUIRE(Cache.Y == -0.7f);
}

// ── 事件分类 ──

TEST_CASE("ClassifyGamepadEvent classifies ADDED as DeviceAdded", "[SdlHelpers][Event]")
{
    REQUIRE(ClassifyGamepadEvent(SdlGamepadEvent::Added) == ESdlGamepadAction::DeviceAdded);
}

TEST_CASE("ClassifyGamepadEvent classifies REMOVED as DeviceRemoved", "[SdlHelpers][Event]")
{
    REQUIRE(ClassifyGamepadEvent(SdlGamepadEvent::Removed) == ESdlGamepadAction::DeviceRemoved);
}

TEST_CASE("ClassifyGamepadEvent classifies REMAPPED as DeviceRemapped not DeviceAdded", "[SdlHelpers][Event]")
{
    auto Action = ClassifyGamepadEvent(SdlGamepadEvent::Remapped);
    REQUIRE(Action == ESdlGamepadAction::DeviceRemapped);
    REQUIRE(Action != ESdlGamepadAction::DeviceAdded);
}

TEST_CASE("ClassifyGamepadEvent classifies button events as ButtonInput", "[SdlHelpers][Event]")
{
    REQUIRE(ClassifyGamepadEvent(SdlGamepadEvent::ButtonDown) == ESdlGamepadAction::ButtonInput);
    REQUIRE(ClassifyGamepadEvent(SdlGamepadEvent::ButtonUp) == ESdlGamepadAction::ButtonInput);
}

TEST_CASE("ClassifyGamepadEvent classifies axis motion as AxisInput", "[SdlHelpers][Event]")
{
    REQUIRE(ClassifyGamepadEvent(SdlGamepadEvent::AxisMotion) == ESdlGamepadAction::AxisInput);
}

TEST_CASE("ClassifyGamepadEvent classifies unknown event as Ignored", "[SdlHelpers][Event]")
{
    REQUIRE(ClassifyGamepadEvent(0) == ESdlGamepadAction::Ignored);
    REQUIRE(ClassifyGamepadEvent(9999) == ESdlGamepadAction::Ignored);
}
