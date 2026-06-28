// ZStickDirectionSynthesizer 单元测试。
// 验证 Axis2D 事件到方向虚拟 Button 事件的合成逻辑：
// 阈值判定、状态变化检测、多方向同时激活、多设备隔离、缓存清理。

#include <catch2/catch_test_macros.hpp>

#include "Runtime/StickDirectionSynthesizer.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SInputEvent MakeAxis2DEvent(
    const StdString& DeviceId,
    const StdString& ControlId,
    float32 X,
    float32 Y)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = ControlId;
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = SAxis2DValue{.X = X, .Y = Y};
    return Event;
}

// 在事件列表中查找指定 ControlId 的事件
static const SInputEvent* FindEvent(
    const TVector<SInputEvent>& Events,
    const StdString& ControlId)
{
    for (const auto& Event : Events)
    {
        if (Event.ControlId == ControlId)
        {
            return &Event;
        }
    }
    return nullptr;
}

// ── 中性位不产生事件 ──

TEST_CASE("Axis2D neutral position produces no direction events",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.0f, 0.0f));

    REQUIRE(Events.empty());
}

// ── 超过阈值产生 pressed ──

TEST_CASE("Axis2D X > threshold produces right pressed",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.6f, 0.0f));

    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].ControlId == "left_stick_right");
    REQUIRE(Events[0].ControlType == EInputControlType::Button);
    REQUIRE(Events[0].EventType == EInputEventType::Pressed);
    REQUIRE(Events[0].Value == 1.0f);
    REQUIRE(Events[0].DeviceId.Value == "pad1");
}

// ── 回落到阈值内产生 released ──

TEST_CASE("Axis2D fallback below threshold produces released",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    // 先激活 right
    (void)Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.6f, 0.0f));

    // 回落
    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.4f, 0.0f));

    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].ControlId == "left_stick_right");
    REQUIRE(Events[0].EventType == EInputEventType::Released);
    REQUIRE(Events[0].Value == 0.0f);
}

// ── 方向切换序列 ──

TEST_CASE("Axis2D direction switch produces release + press",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    // 先推右
    auto Events1 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.6f, 0.0f));
    REQUIRE(Events1.size() == 1);
    REQUIRE(Events1[0].ControlId == "left_stick_right");
    REQUIRE(Events1[0].EventType == EInputEventType::Pressed);

    // 切到左：必须先释放 right 再按下 left，避免两方向同时按下
    auto Events2 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", -0.6f, 0.0f));
    REQUIRE(Events2.size() == 2);

    // 断言顺序：release 在前，press 在后
    REQUIRE(Events2[0].ControlId == "left_stick_right");
    REQUIRE(Events2[0].EventType == EInputEventType::Released);
    REQUIRE(Events2[1].ControlId == "left_stick_left");
    REQUIRE(Events2[1].EventType == EInputEventType::Pressed);
}

// ── Y 轴方向切换也保证先释放后按下 ──

TEST_CASE("Axis2D Y direction switch releases old before pressing new",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    // 先推下
    auto Events1 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.0f, 0.6f));
    REQUIRE(Events1.size() == 1);
    REQUIRE(Events1[0].ControlId == "left_stick_down");
    REQUIRE(Events1[0].EventType == EInputEventType::Pressed);

    // 切到上
    auto Events2 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.0f, -0.6f));
    REQUIRE(Events2.size() == 2);
    REQUIRE(Events2[0].ControlId == "left_stick_down");
    REQUIRE(Events2[0].EventType == EInputEventType::Released);
    REQUIRE(Events2[1].ControlId == "left_stick_up");
    REQUIRE(Events2[1].EventType == EInputEventType::Pressed);
}

// ── Y 轴负值产生 up ──

TEST_CASE("Axis2D Y negative produces up pressed",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.0f, -0.6f));

    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].ControlId == "left_stick_up");
    REQUIRE(Events[0].EventType == EInputEventType::Pressed);
}

// ── Y 轴正值产生 down ──

TEST_CASE("Axis2D Y positive produces down pressed",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.0f, 0.6f));

    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].ControlId == "left_stick_down");
    REQUIRE(Events[0].EventType == EInputEventType::Pressed);
}

// ── 对角方向同时激活 ──

TEST_CASE("Axis2D diagonal produces two direction events",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "right_stick", 0.7f, -0.7f));

    REQUIRE(Events.size() == 2);

    auto* Up = FindEvent(Events, "right_stick_up");
    auto* Right = FindEvent(Events, "right_stick_right");
    REQUIRE(Up != nullptr);
    REQUIRE(Right != nullptr);
    REQUIRE(Up->EventType == EInputEventType::Pressed);
    REQUIRE(Right->EventType == EInputEventType::Pressed);
}

// ── 多设备缓存互不影响 ──

TEST_CASE("Multi-device stick direction caches are independent",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    // 设备 A 推右
    auto EventsA = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("padA", "left_stick", 0.6f, 0.0f));
    REQUIRE(EventsA.size() == 1);
    REQUIRE(EventsA[0].ControlId == "left_stick_right");
    REQUIRE(EventsA[0].DeviceId.Value == "padA");

    // 设备 B 推左
    auto EventsB = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("padB", "left_stick", -0.6f, 0.0f));
    REQUIRE(EventsB.size() == 1);
    REQUIRE(EventsB[0].ControlId == "left_stick_left");
    REQUIRE(EventsB[0].DeviceId.Value == "padB");

    // 设备 A 归零只释放 A 的 right，不影响 B
    auto EventsA2 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("padA", "left_stick", 0.0f, 0.0f));
    REQUIRE(EventsA2.size() == 1);
    REQUIRE(EventsA2[0].ControlId == "left_stick_right");
    REQUIRE(EventsA2[0].EventType == EInputEventType::Released);
    REQUIRE(EventsA2[0].DeviceId.Value == "padA");

    // 设备 B 仍保持 left pressed，再次推同值不产生事件
    auto EventsB2 = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("padB", "left_stick", -0.6f, 0.0f));
    REQUIRE(EventsB2.empty());
}

// ── ClearDevice 清除指定设备的缓存 ──

TEST_CASE("ClearDevice resets direction cache for target device",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    // 激活方向
    (void)Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.6f, 0.0f));

    // 清除设备缓存
    Synthesizer.ClearDevice(SDeviceId{.Value = "pad1"});

    // 再次推同方向应重新产生 pressed（因为缓存已清空）
    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "left_stick", 0.6f, 0.0f));
    REQUIRE(Events.size() == 1);
    REQUIRE(Events[0].ControlId == "left_stick_right");
    REQUIRE(Events[0].EventType == EInputEventType::Pressed);
}

// ── 非摇杆 Axis2D 事件被忽略 ──

TEST_CASE("Non-stick Axis2D events are ignored",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    auto Events = Synthesizer.ProcessAxis2D(
        MakeAxis2DEvent("pad1", "some_other_axis", 0.9f, 0.9f));
    REQUIRE(Events.empty());
}

// ── 非 Axis2D 控件类型被忽略 ──

TEST_CASE("Non-Axis2D control type events are ignored",
    "[Runtime][StickDirectionSynthesizer]")
{
    ZStickDirectionSynthesizer Synthesizer;

    SInputEvent ButtonEvent;
    ButtonEvent.DeviceId = SDeviceId{.Value = "pad1"};
    ButtonEvent.ControlId = "left_stick";
    ButtonEvent.ControlType = EInputControlType::Button;
    ButtonEvent.EventType = EInputEventType::Pressed;
    ButtonEvent.Value = 1.0f;

    auto Events = Synthesizer.ProcessAxis2D(ButtonEvent);
    REQUIRE(Events.empty());
}
