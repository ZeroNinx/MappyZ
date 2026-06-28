import QtQuick

// 映射选择器的可点击 tile，用于键盘、鼠标、DInput 页的统一按键样式
Rectangle {
    id: pickerKey

    required property var theme

    property string label: ""
    property string kind: ""
    property string value: ""
    property bool selected: false
    property real keyWidth: 1.0
    property real keyUnit: 48

    signal clicked()

    width: Math.round(keyWidth * keyUnit)
    height: Math.round(keyUnit * 0.833)
    radius: 4
    opacity: enabled ? 1.0 : 0.22

    color: {
        if (!enabled) return "#2a2a2a"
        if (selected) return Qt.rgba(
            theme.accent.r, theme.accent.g, theme.accent.b, 0.3)
        if (mouseArea.containsMouse) return "#3a3a3a"
        return "#2e2e2e"
    }

    border.color: {
        if (selected) return theme.accent
        if (!enabled) return "#303030"
        if (mouseArea.containsMouse) return "#606060"
        return "#4a4a4a"
    }
    border.width: selected ? 2 : 1

    Text {
        anchors.centerIn: parent
        text: pickerKey.label
        color: pickerKey.selected
            ? theme.accent : (pickerKey.enabled ? "#e8e8e8" : "#555555")
        font.pixelSize: label.length > 3 ? 10 : 12
        font.bold: pickerKey.selected
        horizontalAlignment: Text.AlignHCenter
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: pickerKey.enabled
        cursorShape: pickerKey.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: { if (pickerKey.enabled) pickerKey.clicked() }
    }
}
