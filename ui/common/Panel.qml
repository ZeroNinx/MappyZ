import QtQuick

// 带标题栏的面板容器，通过 default property 让调用处直接嵌套内容
Rectangle {
    id: panel

    required property var theme

    property string heading: ""
    default property alias content: contentHost.data

    color: theme.panel
    radius: 4
    border.width: 0
    clip: true

    Rectangle {
        id: titleBar

        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        height: 33
        color: panel.theme.panelHeader

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 11
            anchors.verticalCenter: parent.verticalCenter
            text: panel.heading
            color: panel.theme.text
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: panel.theme.border
        }
    }

    Item {
        id: contentHost

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 12
    }

    // 外框边线置于最顶层
    Rectangle {
        anchors.fill: parent
        radius: panel.radius
        color: "transparent"
        border.color: panel.theme.border
        border.width: 1
        z: 100
    }
}
