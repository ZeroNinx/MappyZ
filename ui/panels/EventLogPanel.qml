import QtQuick

// 事件日志面板：显示 LogModel 中的 lifecycle 级事件
Panel {
    id: eventLogPanel

    required property var appController

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
                id: logRepeater

                model: eventLogPanel.appController
                    ? eventLogPanel.appController.logModel : null

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
                        width: 80
                        text: time
                        color: eventLogPanel.theme.muted
                        font.pixelSize: 11
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 96
                        anchors.verticalCenter: parent.verticalCenter
                        width: 58
                        text: level
                        color: level === "Error"
                            ? eventLogPanel.theme.warning
                            : (level === "Success"
                                ? eventLogPanel.theme.success
                                : (level === "Warning"
                                    ? "#e0a528"
                                    : eventLogPanel.theme.text))
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 162
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

            // 日志为空时的占位提示
            Text {
                visible: logRepeater.count === 0
                text: "No events yet"
                color: eventLogPanel.theme.muted
                font.pixelSize: 12
                topPadding: 8
            }
        }
    }
}
