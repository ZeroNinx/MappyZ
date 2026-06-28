// ZInputStateModel 单元测试。
// 验证输入状态模型的行为：事件插入/更新、按钮/扳机/摇杆 role 计算、
// 设备删除、invokable 查询默认值、sequence 单调递增和 latestControlId。

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QSignalSpy>

#include "Core/ControlId.h"
#include "UI/Bridge/InputStateModel.h"

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

// ── 默认状态 ──

TEST_CASE("InputStateModel default rowCount is zero",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    REQUIRE(Model.rowCount() == 0);
}

// ── ApplyInputEvent 插入新行 ──

TEST_CASE("InputStateModel ApplyInputEvent inserts new row with all roles",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;
    QSignalSpy InsertSpy(&Model, &QAbstractItemModel::rowsInserted);

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(InsertSpy.count() == 1);

    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::DeviceIdRole).toString() == "dev_1");
    REQUIRE(Index.data(ZInputStateModel::ControlIdRole).toString() == "button_south");
    REQUIRE(Index.data(ZInputStateModel::ControlTypeRole).toInt()
        == static_cast<int>(EInputControlType::Button));
    REQUIRE(Index.data(ZInputStateModel::EventTypeRole).toInt()
        == static_cast<int>(EInputEventType::Pressed));
    REQUIRE(Index.data(ZInputStateModel::ValueRole).toDouble() == 1.0);
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == true);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "pressed");
    REQUIRE(Index.data(ZInputStateModel::SequenceRole).toULongLong() > 0);
}

// ── ApplyInputEvent 更新已有行 ──

TEST_CASE("InputStateModel ApplyInputEvent updates existing row and emits dataChanged",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy ChangedSpy(&Model, &QAbstractItemModel::dataChanged);

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(ChangedSpy.count() == 1);

    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == false);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "released");
}

// ── 不同设备同名控件分成不同行 ──

TEST_CASE("InputStateModel different devices with same control are separate rows",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Model.rowCount() == 2);

    QModelIndex Row0 = Model.index(0);
    QModelIndex Row1 = Model.index(1);
    REQUIRE(Row0.data(ZInputStateModel::DeviceIdRole).toString() == "dev_1");
    REQUIRE(Row1.data(ZInputStateModel::DeviceIdRole).toString() == "dev_2");
    REQUIRE(Row0.data(ZInputStateModel::PressedRole).toBool() == true);
    REQUIRE(Row1.data(ZInputStateModel::PressedRole).toBool() == false);
}

// ── Button Pressed/Released ──

TEST_CASE("InputStateModel button pressed and released updates PressedRole and DisplayValueRole",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == true);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "pressed");

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == false);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "released");
}

// ── Trigger 值更新 ──

TEST_CASE("InputStateModel trigger value updates ValueRole PressedRole and DisplayValueRole",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.3f));

    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::ValueRole).toDouble() == Catch::Approx(0.3));
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == false);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "0.30");

    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.8f));

    REQUIRE(Index.data(ZInputStateModel::ValueRole).toDouble() == Catch::Approx(0.8));
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == true);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "0.80");
}

// ── Trigger PressedRole 阈值精确验证 ──

TEST_CASE("InputStateModel trigger PressedRole uses fixed threshold value greater than 0.5",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    // 恰好 0.5 不算按下
    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.5f));
    REQUIRE(Model.isPressed("dev_1", "left_trigger") == false);

    // 刚超过 0.5 算按下
    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.51f));
    REQUIRE(Model.isPressed("dev_1", "left_trigger") == true);
}

// ── Axis2D 更新 ──

TEST_CASE("InputStateModel Axis2D updates AxisXRole AxisYRole and DisplayValueRole",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, -0.45f, 0.72f));

    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::AxisXRole).toDouble() == Catch::Approx(-0.45));
    REQUIRE(Index.data(ZInputStateModel::AxisYRole).toDouble() == Catch::Approx(0.72));
    REQUIRE(Index.data(ZInputStateModel::PressedRole).toBool() == false);
    REQUIRE(Index.data(ZInputStateModel::DisplayValueRole).toString() == "(-0.45, 0.72)");
}

// ── SequenceRole 全局单调递增 ──

TEST_CASE("InputStateModel SequenceRole uses global monotonic counter",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonNorth, EInputEventType::Pressed));

    auto Seq0 = Model.index(0).data(ZInputStateModel::SequenceRole).toULongLong();
    auto Seq1 = Model.index(1).data(ZInputStateModel::SequenceRole).toULongLong();
    REQUIRE(Seq1 > Seq0);

    // 更新第一行后 sequence 也递增
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    auto Seq0Updated = Model.index(0).data(ZInputStateModel::SequenceRole).toULongLong();
    REQUIRE(Seq0Updated > Seq1);
}

// ── latestControlId 全局 ──

TEST_CASE("InputStateModel latestControlId returns global latest control",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonNorth, EInputEventType::Pressed));

    REQUIRE(Model.latestControlId() == "button_north");

    // 更新 dev_1 的按钮后，最新变为 dev_1 的控件
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Model.latestControlId() == "button_south");
}

// ── latestControlId 按设备过滤 ──

TEST_CASE("InputStateModel latestControlId filtered by deviceId",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonNorth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonEast, EInputEventType::Pressed));

    REQUIRE(Model.latestControlId("dev_1") == "button_east");
    REQUIRE(Model.latestControlId("dev_2") == "button_north");
}

// ── RemoveDevice ──

TEST_CASE("InputStateModel RemoveDevice removes all states for device",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonNorth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Model.rowCount() == 3);

    QSignalSpy RemoveSpy(&Model, &QAbstractItemModel::rowsRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(Model.rowCount() == 1);
    REQUIRE(RemoveSpy.count() == 2);

    // 剩余行应为 dev_2
    QModelIndex Index = Model.index(0);
    REQUIRE(Index.data(ZInputStateModel::DeviceIdRole).toString() == "dev_2");
}

// ── clear ──

TEST_CASE("InputStateModel clear removes all states and emits reset",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonNorth, EInputEventType::Pressed));

    QSignalSpy ResetSpy(&Model, &QAbstractItemModel::modelReset);

    Model.clear();

    REQUIRE(Model.rowCount() == 0);
    REQUIRE(ResetSpy.count() == 1);

    // 重复 clear 安全，不多发 reset
    Model.clear();
    REQUIRE(ResetSpy.count() == 1);
}

// ── invokable 查询默认值 ──

TEST_CASE("InputStateModel invokable queries return safe defaults for unknown device",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    REQUIRE(Model.isPressed("unknown", "button_south") == false);
    REQUIRE(Model.value("unknown", "button_south") == Catch::Approx(0.0));
    REQUIRE(Model.axisX("unknown", "left_stick") == Catch::Approx(0.0));
    REQUIRE(Model.axisY("unknown", "left_stick") == Catch::Approx(0.0));
    REQUIRE(Model.displayValue("unknown", "button_south") == "");
    REQUIRE(Model.latestControlId() == "");
    REQUIRE(Model.latestControlId("unknown") == "");
}

// ── Hat pressed/released ──

TEST_CASE("InputStateModel hat pressed and released works like button",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeHatEvent("dev_1", ControlId::DpadUp, EInputEventType::Pressed));

    REQUIRE(Model.isPressed("dev_1", "dpad_up") == true);
    REQUIRE(Model.displayValue("dev_1", "dpad_up") == "pressed");

    Model.ApplyInputEvent(
        MakeHatEvent("dev_1", ControlId::DpadUp, EInputEventType::Released));

    REQUIRE(Model.isPressed("dev_1", "dpad_up") == false);
    REQUIRE(Model.displayValue("dev_1", "dpad_up") == "released");
}

// ── data() 防御性边界 ──

TEST_CASE("InputStateModel data returns empty QVariant for invalid index",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    // 越界行
    QModelIndex OutOfRange = Model.index(99);
    REQUIRE(OutOfRange.data(ZInputStateModel::DeviceIdRole) == QVariant());

    // 无效 index
    QModelIndex Invalid;
    REQUIRE(Model.data(Invalid, ZInputStateModel::DeviceIdRole) == QVariant());
}

// ── header 不包含 SDL 或 Win32 ──

TEST_CASE("InputStateModel header has no SDL or Win32 dependencies",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;
    REQUIRE(Model.rowCount() == 0);
}

// ══════════════════════════════════════════════════════════════
// 语义 signal 测试
// ══════════════════════════════════════════════════════════════

TEST_CASE("InputStateModel meta-object does not contain revision property",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    // 如果 revision Q_PROPERTY 存在，propertyIndex 返回非负值
    int Index = Model.metaObject()->indexOfProperty("revision");
    REQUIRE(Index < 0);
}

TEST_CASE("InputStateModel signals use lowerCamelCase for QML Connections compatibility",
    "[UI][InputStateModel]")
{
    const QMetaObject* Meta = &ZInputStateModel::staticMetaObject;

    REQUIRE(Meta->indexOfSignal("controlStateChanged(QString,QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("deviceStateRemoved(QString)") >= 0);
    REQUIRE(Meta->indexOfSignal("inputStateReset()") >= 0);
}

TEST_CASE("InputStateModel ApplyInputEvent emits ControlStateChanged on insert",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    QSignalSpy Spy(&Model, &ZInputStateModel::controlStateChanged);

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Spy.count() == 1);
    auto Args = Spy.takeFirst();
    REQUIRE(Args.at(0).toString() == "dev_1");
    REQUIRE(Args.at(1).toString() == "button_south");
}

TEST_CASE("InputStateModel ApplyInputEvent emits ControlStateChanged on update",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy Spy(&Model, &ZInputStateModel::controlStateChanged);

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Released));

    REQUIRE(Spy.count() == 1);
    auto Args = Spy.takeFirst();
    REQUIRE(Args.at(0).toString() == "dev_1");
    REQUIRE(Args.at(1).toString() == "button_south");
}

TEST_CASE("InputStateModel ControlStateChanged snapshot is already updated in slot",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    // 在 signal slot 中验证快照已包含新值
    bool bPressedInSlot = false;
    double ValueInSlot = -1.0;
    QObject::connect(&Model, &ZInputStateModel::controlStateChanged,
        [&Model, &bPressedInSlot, &ValueInSlot](const QString& DeviceId, const QString& ControlId)
        {
            bPressedInSlot = Model.isPressed(DeviceId, ControlId);
            ValueInSlot = Model.value(DeviceId, ControlId);
        });

    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.75f));

    REQUIRE(bPressedInSlot == true);
    REQUIRE(ValueInSlot == Catch::Approx(0.75));
}

TEST_CASE("InputStateModel different devices emit distinguishable deviceId in ControlStateChanged",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    QSignalSpy Spy(&Model, &ZInputStateModel::controlStateChanged);

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_2", ControlId::ButtonSouth, EInputEventType::Pressed));

    REQUIRE(Spy.count() == 2);
    REQUIRE(Spy.at(0).at(0).toString() == "dev_1");
    REQUIRE(Spy.at(1).at(0).toString() == "dev_2");
}

TEST_CASE("InputStateModel trigger update emits ControlStateChanged with trigger controlId",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    QSignalSpy Spy(&Model, &ZInputStateModel::controlStateChanged);

    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.5f));

    REQUIRE(Spy.count() == 1);
    REQUIRE(Spy.at(0).at(1).toString() == "left_trigger");
}

TEST_CASE("InputStateModel Axis2D update emits ControlStateChanged with stick controlId",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    QSignalSpy Spy(&Model, &ZInputStateModel::controlStateChanged);

    Model.ApplyInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, 0.1f, -0.2f));

    REQUIRE(Spy.count() == 1);
    REQUIRE(Spy.at(0).at(1).toString() == "left_stick");
}

TEST_CASE("InputStateModel RemoveDevice emits DeviceStateRemoved when states exist",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonNorth, EInputEventType::Pressed));

    QSignalSpy Spy(&Model, &ZInputStateModel::deviceStateRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "dev_1"});

    REQUIRE(Spy.count() == 1);
    REQUIRE(Spy.takeFirst().at(0).toString() == "dev_1");
}

TEST_CASE("InputStateModel RemoveDevice does not emit DeviceStateRemoved for unknown device",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy Spy(&Model, &ZInputStateModel::deviceStateRemoved);

    Model.RemoveDevice(SDeviceId{.Value = "not_exist"});

    REQUIRE(Spy.count() == 0);
}

TEST_CASE("InputStateModel clear emits InputStateReset for non-empty model",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));

    QSignalSpy Spy(&Model, &ZInputStateModel::inputStateReset);

    Model.clear();

    REQUIRE(Spy.count() == 1);
}

TEST_CASE("InputStateModel clear does not emit InputStateReset for empty model",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    QSignalSpy Spy(&Model, &ZInputStateModel::inputStateReset);

    Model.clear();

    REQUIRE(Spy.count() == 0);
}

TEST_CASE("InputStateModel snapshot queries remain unchanged after signal refactor",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    Model.ApplyInputEvent(
        MakeButtonEvent("dev_1", ControlId::ButtonSouth, EInputEventType::Pressed));
    Model.ApplyInputEvent(
        MakeTriggerEvent("dev_1", ControlId::LeftTrigger, 0.6f));
    Model.ApplyInputEvent(
        MakeAxis2DEvent("dev_1", ControlId::LeftStick, -0.3f, 0.7f));

    REQUIRE(Model.isPressed("dev_1", "button_south") == true);
    REQUIRE(Model.value("dev_1", "left_trigger") == Catch::Approx(0.6));
    REQUIRE(Model.axisX("dev_1", "left_stick") == Catch::Approx(-0.3));
    REQUIRE(Model.axisY("dev_1", "left_stick") == Catch::Approx(0.7));
    REQUIRE(Model.displayValue("dev_1", "button_south") == "pressed");
    REQUIRE(Model.latestControlId("dev_1") == "left_stick");
}

// ── 方向 Button 状态查询 ──

TEST_CASE("InputStateModel stick direction button events update pressed state",
    "[UI][InputStateModel]")
{
    ZInputStateModel Model;

    // 方向虚拟 Button 事件
    SInputEvent UpPressed;
    UpPressed.DeviceId = SDeviceId{.Value = "dev_1"};
    UpPressed.ControlId = "left_stick_up";
    UpPressed.ControlType = EInputControlType::Button;
    UpPressed.EventType = EInputEventType::Pressed;
    UpPressed.Value = 1.0f;

    Model.ApplyInputEvent(UpPressed);

    REQUIRE(Model.isPressed("dev_1", "left_stick_up") == true);
    REQUIRE(Model.displayValue("dev_1", "left_stick_up") == "pressed");
    REQUIRE(Model.latestControlId("dev_1") == "left_stick_up");

    // 释放
    SInputEvent UpReleased = UpPressed;
    UpReleased.EventType = EInputEventType::Released;
    UpReleased.Value = 0.0f;

    Model.ApplyInputEvent(UpReleased);

    REQUIRE(Model.isPressed("dev_1", "left_stick_up") == false);
    REQUIRE(Model.displayValue("dev_1", "left_stick_up") == "released");
}
