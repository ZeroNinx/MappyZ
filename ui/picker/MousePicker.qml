import QtQuick

// 鼠标映射选择页：左侧鼠标按钮、中间鼠标轮廓、右侧鼠标移动方向
Item {
    id: mousePicker

    required property var theme

    property string pendingKind: ""
    property string pendingValue: ""

    signal mouseActionSelected(string kind, string value)

    implicitHeight: mouseContainer.implicitHeight

    // 统一容器：固定宽度、圆角边框、水平居中
    Rectangle {
        id: mouseContainer

        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 784)
        implicitHeight: containerRow.implicitHeight + 32
        radius: 6
        color: "transparent"
        border.color: mousePicker.theme.border
        border.width: 1

        Row {
            id: containerRow

            anchors.centerIn: parent
            spacing: 24

            // ── 左列：鼠标按钮 ──
            Column {
                id: buttonColumn

                width: 264
                spacing: 10

                Text {
                    text: "鼠标按钮"
                    color: mousePicker.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                // 左键 / 右键 / 中键
                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "左键"
                        kind: "MouseButton"; value: "Left"
                        keyWidth: 1.75; height: 64
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Left"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Left")
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "右键"
                        kind: "MouseButton"; value: "Right"
                        keyWidth: 1.75; height: 64
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Right"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Right")
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "中键"
                        kind: "MouseButton"; value: "Middle"
                        keyWidth: 1.75; height: 64
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Middle"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Middle")
                    }
                }

                // 滚轮上 / 滚轮下
                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "滚轮上"
                        keyWidth: 2.7; height: 44
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "滚轮下"
                        keyWidth: 2.7; height: 44
                        enabled: false
                    }
                }
            }

            // ── 中列：鼠标轮廓简化图形（垂直居中于左右按钮组）──
            Item {
                width: 120
                height: buttonColumn.height
                anchors.verticalCenter: buttonColumn.verticalCenter

                // 鼠标主体
                Rectangle {
                    anchors.centerIn: parent
                    width: 80
                    height: 140
                    radius: 40
                    color: "transparent"
                    border.color: mousePicker.theme.accent
                    border.width: 1.5
                    opacity: 0.5

                    // 中键/滚轮指示
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 22
                        width: 10
                        height: 22
                        radius: 5
                        color: Qt.rgba(
                            mousePicker.theme.accent.r,
                            mousePicker.theme.accent.g,
                            mousePicker.theme.accent.b, 0.4)
                    }

                    // 顶部线缆
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        width: 2
                        height: 16
                        color: mousePicker.theme.border
                    }
                }

                // 侧键 1
                PickerKey {
                    theme: mousePicker.theme; label: "侧1"
                    keyWidth: 1.0; height: 24
                    enabled: false
                    anchors.right: parent.horizontalCenter
                    anchors.rightMargin: 48
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -16
                }

                // 侧键 2
                PickerKey {
                    theme: mousePicker.theme; label: "侧2"
                    keyWidth: 1.0; height: 24
                    enabled: false
                    anchors.right: parent.horizontalCenter
                    anchors.rightMargin: 48
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 16
                }
            }

            // ── 右列：鼠标移动方向 ──
            Column {
                width: 264
                spacing: 10

                Text {
                    text: "鼠标移动"
                    color: mousePicker.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "← 左移"
                        keyWidth: 2.7; height: 64
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "→ 右移"
                        keyWidth: 2.7; height: 64
                        enabled: false
                    }
                }

                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "↑ 上移"
                        keyWidth: 2.7; height: 64
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "↓ 下移"
                        keyWidth: 2.7; height: 64
                        enabled: false
                    }
                }
            }
        }
    }
}
