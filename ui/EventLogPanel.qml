import QtQuick

// 事件日志面板：显示 demo 日志列表
Panel {
    id: eventLogPanel

    required property var eventModel

    heading: "Event Log"

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn

            width: parent.width
            spacing: 6

            Repeater {
                model: eventLogPanel.eventModel

                Rectangle {
                    required property int index
                    required property string time
                    required property string level
                    required property string message

                    width: contentColumn.width
                    height: 24
                    color: index % 2 === 0 ? "#1f1f1f" : eventLogPanel.theme.panel

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 70
                        text: time
                        color: eventLogPanel.theme.muted
                        font.pixelSize: 11
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 86
                        anchors.verticalCenter: parent.verticalCenter
                        width: 58
                        text: level
                        color: level === "Output"
                            ? eventLogPanel.theme.success
                            : (level === "Map" ? eventLogPanel.theme.accent : eventLogPanel.theme.text)
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 152
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: message
                        color: eventLogPanel.theme.text
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
