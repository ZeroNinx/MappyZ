import QtQuick

// 通用操作按钮，支持 primary 高亮和 hover 反馈
Rectangle {
    id: button

    required property var theme

    property string label: ""
    property bool primary: false

    signal clicked()

    width: labelText.implicitWidth + 28
    height: 30
    radius: 3
    color: mouseArea.containsMouse
        ? theme.accentHover
        : (primary ? theme.accentSoft : theme.surface)
    border.color: primary ? theme.accent : theme.border
    border.width: 1

    Text {
        id: labelText

        anchors.centerIn: parent
        text: button.label
        color: "#ffffff"
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }
}
