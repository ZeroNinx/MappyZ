import QtQuick

// 手柄控件圆点，显示按压状态并支持点击选择。
// 点击时只发 selected signal，由父组件决定后续行为。
Rectangle {
    id: controlDot

    required property var theme
    required property var inputStateModel
    required property string selectedDevice
    required property string selectedControl

    property string controlId: ""
    property string label: ""

    // 暴露内部状态供父组件读取（如摇杆偏移）
    readonly property alias inputState: controlState

    signal selected(string controlId)

    InputControlState {
        id: controlState

        inputStateModel: controlDot.inputStateModel || null
        deviceId: controlDot.selectedDevice
        controlId: controlDot.controlId
    }

    property bool active: controlState.pressed

    width: 38
    height: 38
    radius: 19
    color: active ? theme.accentSoft
        : (selectedControl === controlId ? "#3a3a3a" : theme.surface)
    border.color: active ? theme.accent
        : (selectedControl === controlId ? theme.accent : theme.border)
    border.width: 1

    Text {
        anchors.centerIn: parent
        text: controlDot.label
        color: controlDot.active || controlDot.selectedControl === controlDot.controlId
            ? "#ffffff" : controlDot.theme.text
        font.pixelSize: 12
        font.bold: true
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: controlDot.selected(controlDot.controlId)
    }
}
