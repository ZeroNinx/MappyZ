// ZInputCaptureModel 单元测试。
// 验证 capture 生命周期：begin/cancel/complete、输入过滤规则（摇杆漂移、
// 按钮释放忽略、扳机/摇杆阈值）、signal 发射和状态查询。

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include "Core/ControlId.h"
#include "UI/Bridge/InputCaptureModel.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SInputEvent MakeButtonEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    EInputEventType EventType)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Button;
    Event.EventType = EventType;
    Event.Value = (EventType == EInputEventType::Pressed) ? 1.0f : 0.0f;
    return Event;
}

static SInputEvent MakeHatEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    EInputEventType EventType)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Hat;
    Event.EventType = EventType;
    Event.Value = (EventType == EInputEventType::Pressed) ? 1.0f : 0.0f;
    return Event;
}

static SInputEvent MakeTriggerEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    float32 Value)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Trigger;
    Event.EventType = EInputEventType::Changed;
    Event.Value = Value;
    return Event;
}

static SInputEvent MakeAxis1DEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    float32 Value)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Axis1D;
    Event.EventType = EInputEventType::Changed;
    Event.Value = Value;
    return Event;
}

static SInputEvent MakeAxis2DEvent(
    const StdString& DeviceId,
    StdStringView ControlId,
    float32 X,
    float32 Y)
{
    SInputEvent Event;
    Event.DeviceId = SDeviceId{.Value = DeviceId};
    Event.ControlId = StdString(ControlId);
    Event.ControlType = EInputControlType::Axis2D;
    Event.EventType = EInputEventType::Changed;
    Event.Axis2D = SAxis2DValue{.X = X, .Y = Y};
    return Event;
}

// ── 默认状态 ──

TEST_CASE("InputCaptureModel default state is inactive with empty fields",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.DeviceId().isEmpty());
    REQUIRE(Model.ControlId().isEmpty());
    REQUIRE(Model.DisplayText().isEmpty());
}

// ── begin ──

TEST_CASE("InputCaptureModel begin enters active state and emits CaptureStateChanged",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    QSignalSpy StateSpy(&Model, &ZInputCaptureModel::captureStateChanged);

    Model.begin("dev_1");

    REQUIRE(Model.IsActive());
    REQUIRE(StateSpy.count() == 1);
    REQUIRE(Model.DisplayText() == "Waiting for input...");
}

TEST_CASE("InputCaptureModel begin with empty deviceId shows any-device text",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    Model.begin();

    REQUIRE(Model.IsActive());
    REQUIRE(Model.DisplayText() == "Waiting for any device input...");
}

TEST_CASE("InputCaptureModel begin clears previous captured control",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    // 完成一次 capture
    Model.begin("dev_1");
    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    REQUIRE(Model.ControlId() == "button_south");

    // 重新 begin 应清空上一轮
    QSignalSpy ResultSpy(&Model, &ZInputCaptureModel::captureResultChanged);

    Model.begin("dev_1");

    REQUIRE(Model.IsActive());
    REQUIRE(Model.DeviceId().isEmpty());
    REQUIRE(Model.ControlId().isEmpty());
    REQUIRE(ResultSpy.count() == 1);
}

TEST_CASE("InputCaptureModel CaptureResultChanged emitted on capture completion",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy ResultSpy(&Model, &ZInputCaptureModel::captureResultChanged);

    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(ResultSpy.count() == 1);
    REQUIRE(Model.DeviceId() == "dev_1");
    REQUIRE(Model.ControlId() == "button_south");
}

// ── cancel ──

TEST_CASE("InputCaptureModel cancel active capture emits CaptureStateChanged and CaptureCancelled",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy StateSpy(&Model, &ZInputCaptureModel::captureStateChanged);
    QSignalSpy CancelSpy(&Model, &ZInputCaptureModel::captureCancelled);

    Model.cancel();

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(StateSpy.count() == 1);
    REQUIRE(CancelSpy.count() == 1);
}

TEST_CASE("InputCaptureModel cancel when inactive is no-op",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    QSignalSpy StateSpy(&Model, &ZInputCaptureModel::captureStateChanged);
    QSignalSpy CancelSpy(&Model, &ZInputCaptureModel::captureCancelled);

    Model.cancel();

    REQUIRE(StateSpy.count() == 0);
    REQUIRE(CancelSpy.count() == 0);
}

// ── HandleInputEvent 基本 ──

TEST_CASE("InputCaptureModel HandleInputEvent when inactive is no-op",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(CompleteSpy.count() == 0);
}

TEST_CASE("InputCaptureModel HandleInputEvent ignores non-target device",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Model.IsActive());
    REQUIRE(CompleteSpy.count() == 0);
}

TEST_CASE("InputCaptureModel HandleInputEvent completes on target device button press",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy StateSpy(&Model, &ZInputCaptureModel::captureStateChanged);
    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.DeviceId() == "dev_1");
    REQUIRE(Model.ControlId() == "button_south");
    REQUIRE(StateSpy.count() == 1);
    REQUIRE(CompleteSpy.count() == 1);
    REQUIRE(CompleteSpy.at(0).at(0).toString() == "dev_1");
    REQUIRE(CompleteSpy.at(0).at(1).toString() == "button_south");
}

TEST_CASE("InputCaptureModel begin with empty deviceId accepts any device",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin();

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeButtonEvent("any_dev", ControlId::ButtonNorth, EInputEventType::Pressed));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.DeviceId() == "any_dev");
    REQUIRE(Model.ControlId() == "button_north");
    REQUIRE(CompleteSpy.count() == 1);
}

// ── 输入过滤：Button / Hat ──

TEST_CASE("InputCaptureModel Button Released does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Hat Released does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    Model.HandleInputEvent(
        MakeHatEvent("dev_1", ControlId::DpadUp, EInputEventType::Released));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Hat Pressed completes capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeHatEvent("dev_1", ControlId::DpadUp, EInputEventType::Pressed));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "dpad_up");
    REQUIRE(CompleteSpy.count() == 1);
}

// ── 输入过滤：Trigger ──

TEST_CASE("InputCaptureModel Trigger at threshold does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    Model.HandleInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.5f));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Trigger above threshold completes capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.51f));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "left_trigger");
    REQUIRE(CompleteSpy.count() == 1);
}

// ── 输入过滤：Axis1D ──

TEST_CASE("InputCaptureModel Axis1D at deadzone does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    Model.HandleInputEvent(
        MakeAxis1DEvent("dev_1", "horizontal_axis", 0.7f));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Axis1D above deadzone completes capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeAxis1DEvent("dev_1", "horizontal_axis", 0.71f));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "horizontal_axis");
    REQUIRE(CompleteSpy.count() == 1);
}

TEST_CASE("InputCaptureModel Axis1D negative above deadzone completes capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    Model.HandleInputEvent(
        MakeAxis1DEvent("dev_1", "horizontal_axis", -0.71f));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "horizontal_axis");
}

// ── 输入过滤：Axis2D ──

TEST_CASE("InputCaptureModel Axis2D small drift does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    // sqrt(0.1^2 + 0.1^2) ≈ 0.141，远低于 0.7 阈值
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.1f, 0.1f));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Axis2D at deadzone boundary does not complete capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    // sqrt(0.5^2 + 0.5^2) ≈ 0.707，但需要严格大于 0.7
    // 使用精确在阈值上的值：(0.7, 0.0) = 0.7
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.7f, 0.0f));

    REQUIRE(Model.IsActive());
}

TEST_CASE("InputCaptureModel Axis2D above deadzone completes capture for non-stick axis",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    // 非摇杆的 Axis2D 仍按原逻辑捕获
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", "some_other_axis", 0.71f, 0.0f));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "some_other_axis");
    REQUIRE(CompleteSpy.count() == 1);
}

TEST_CASE("InputCaptureModel stick Axis2D is rejected in favor of direction buttons",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    // left_stick/right_stick 的 Axis2D 即使超过阈值也不捕获
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.9f, 0.0f));
    REQUIRE(Model.IsActive());

    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::RightStick, 0.0f, -0.9f));
    REQUIRE(Model.IsActive());

    // 方向 Button pressed 完成捕获
    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);
    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", "left_stick_up", EInputEventType::Pressed));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "left_stick_up");
    REQUIRE(CompleteSpy.count() == 1);
}

TEST_CASE("InputCaptureModel joystick micro-drift does not preempt capture",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    Model.begin("dev_1");

    // 模拟多帧微小漂移
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.02f, -0.01f));
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::RightStick, -0.03f, 0.02f));
    Model.HandleInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.01f, 0.03f));

    // 仍然 active，漂移未触发 capture
    REQUIRE(Model.IsActive());

    // 真正的按钮按下才完成 capture
    QSignalSpy CompleteSpy(&Model, &ZInputCaptureModel::captureCompleted);

    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE_FALSE(Model.IsActive());
    REQUIRE(Model.ControlId() == "button_south");
    REQUIRE(CompleteSpy.count() == 1);
}

// ── DisplayText ──

TEST_CASE("InputCaptureModel DisplayText shows captured controlId after completion",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;

    Model.begin("dev_1");
    Model.HandleInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonEast, EInputEventType::Pressed));

    REQUIRE(Model.DisplayText() == "button_east");
}

// ── header 依赖检查 ──

TEST_CASE("InputCaptureModel header has no SDL or Win32 dependencies",
    "[UI][InputCaptureModel]")
{
    ZInputCaptureModel Model;
    REQUIRE_FALSE(Model.IsActive());
}

TEST_CASE("InputCaptureModel signals use lowerCamelCase for QML Connections compatibility",
    "[UI][InputCaptureModel]")
{
    const QMetaObject* Meta = &ZInputCaptureModel::staticMetaObject;

    REQUIRE(Meta->indexOfSignal("captureStateChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("captureResultChanged()") >= 0);
    REQUIRE(Meta->indexOfSignal("captureCompleted(QString,QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("captureCancelled()") >= 0);
}
