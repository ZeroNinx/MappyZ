import QtQuick

// 设备列表面板：显示已连接设备和空状态提示
Panel {
    id: devicesPanel

    required property var appController
    required property string selectedDevice
    required property string selectedDeviceDisplayName

    heading: "Devices"

    // 供 StatusBar 等外部组件读取设备数量
    readonly property int deviceCount: deviceRepeater.count

    signal deviceSelected(string deviceId, string displayName)

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn

            width: parent.width
            spacing: 10

            Repeater {
                id: deviceRepeater

                model: devicesPanel.appController ? devicesPanel.appController.deviceModel : null

                Rectangle {
                    required property string deviceId
                    required property string displayName
                    required property string backend
                    required property string vendorId
                    required property string productId

                    width: contentColumn.width
                    height: 110
                    radius: 4
                    color: devicesPanel.selectedDevice === deviceId ? "#2d2d2d" : "#1f1f1f"
                    border.color: devicesPanel.selectedDevice === deviceId
                        ? devicesPanel.theme.accent : devicesPanel.theme.border
                    border.width: 1

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: stateTag.left
                        anchors.rightMargin: 8
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: displayName
                        color: devicesPanel.theme.text
                        font.pixelSize: 12
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Tag {
                        id: stateTag

                        theme: devicesPanel.theme
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        label: "Connected"
                        tone: devicesPanel.theme.success
                    }

                    FieldLabel {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 36
                        text: "Backend"
                    }

                    ValueText {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 74
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 36
                        text: backend
                    }

                    FieldLabel {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 58
                        text: "ID"
                    }

                    ValueText {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 74
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 58
                        text: vendorId + ":" + productId
                    }

                    FieldLabel {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 80
                        text: "Profile"
                    }

                    ValueText {
                        theme: devicesPanel.theme
                        anchors.left: parent.left
                        anchors.leftMargin: 74
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 80
                        text: "Unassigned"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: devicesPanel.deviceSelected(deviceId, displayName)
                    }
                }
            }

            // 无设备时的引导说明
            Column {
                visible: deviceRepeater.count === 0
                width: contentColumn.width
                spacing: 6
                topPadding: 8

                Text {
                    text: "No gamepads connected"
                    color: devicesPanel.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: "Connect a gamepad to get started.\nSupported: Xbox, PlayStation, and other XInput / DirectInput devices."
                    color: devicesPanel.theme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }
    }
}
