import QtQuick

// 鼠标映射选择页：左侧鼠标按钮、中间鼠标轮廓、右侧鼠标移动方向
Item {
    id: mousePicker

    required property var theme

    property string pendingKind: ""
    property string pendingValue: ""

    signal mouseActionSelected(string kind, string value)

    implicitHeight: mouseLayout.implicitHeight

    Row {
        id: mouseLayout

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 24

        // ── 左侧：鼠标按钮 ──
        Column {
            spacing: 12
            width: 220

            Text {
                text: "鼠标按钮"
                color: mousePicker.theme.text
                font.pixelSize: 14
                font.bold: true
            }

            // 左键 / 右键 / 中键
            Row {
                spacing: 8

                PickerKey {
                    theme: mousePicker.theme; label: "左键"
                    kind: "MouseButton"; value: "Left"
                    keyWidth: 2.2; height: 70
                    enabled: true
                    selected: mousePicker.pendingKind === "MouseButton"
                        && mousePicker.pendingValue === "Left"
                    onClicked: mousePicker.mouseActionSelected("MouseButton", "Left")
                }
                PickerKey {
                    theme: mousePicker.theme; label: "右键"
                    kind: "MouseButton"; value: "Right"
                    keyWidth: 2.2; height: 70
                    enabled: true
                    selected: mousePicker.pendingKind === "MouseButton"
                        && mousePicker.pendingValue === "Right"
                    onClicked: mousePicker.mouseActionSelected("MouseButton", "Right")
                }
                PickerKey {
                    theme: mousePicker.theme; label: "中键"
                    kind: "MouseButton"; value: "Middle"
                    keyWidth: 2.2; height: 70
                    enabled: true
                    selected: mousePicker.pendingKind === "MouseButton"
                        && mousePicker.pendingValue === "Middle"
                    onClicked: mousePicker.mouseActionSelected("MouseButton", "Middle")
                }
            }

            // 滚轮上 / 滚轮下
            Row {
                spacing: 8

                PickerKey {
                    theme: mousePicker.theme; label: "滚轮上"
                    keyWidth: 2.2; height: 60
                    enabled: false
                }
                PickerKey {
                    theme: mousePicker.theme; label: "滚轮下"
                    keyWidth: 2.2; height: 60
                    enabled: false
                }
            }
        }

        // ── 中间：鼠标轮廓简化图形 ──
        Item {
            width: 180
            height: 260

            // 鼠标主体
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                width: 100
                height: 180
                radius: 50
                color: "transparent"
                border.color: mousePicker.theme.accent
                border.width: 1.5
                opacity: 0.5

                // 中键/滚轮指示
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 30
                    width: 14
                    height: 30
                    radius: 7
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
                    height: 20
                    color: mousePicker.theme.border
                }
            }

            // 侧键 1
            Row {
                anchors.right: parent.horizontalCenter
                anchors.rightMargin: 60
                anchors.top: parent.top
                anchors.topMargin: 80
                spacing: 4

                PickerKey {
                    theme: mousePicker.theme; label: "侧键1"
                    keyWidth: 1.4; height: 32
                    enabled: false
                }
            }

            // 侧键 2
            Row {
                anchors.right: parent.horizontalCenter
                anchors.rightMargin: 60
                anchors.top: parent.top
                anchors.topMargin: 125
                spacing: 4

                PickerKey {
                    theme: mousePicker.theme; label: "侧键2"
                    keyWidth: 1.4; height: 32
                    enabled: false
                }
            }
        }

        // ── 右侧：鼠标移动方向 ──
        Column {
            spacing: 12
            width: 220

            Text {
                text: "鼠标移动"
                color: mousePicker.theme.text
                font.pixelSize: 14
                font.bold: true
            }

            // 左移 / 右移
            Row {
                spacing: 8

                PickerKey {
                    theme: mousePicker.theme; label: "← 鼠标左移"
                    keyWidth: 2.2; height: 70
                    enabled: false
                }
                PickerKey {
                    theme: mousePicker.theme; label: "→ 鼠标右移"
                    keyWidth: 2.2; height: 70
                    enabled: false
                }
            }

            // 上移 / 下移
            Row {
                spacing: 8

                PickerKey {
                    theme: mousePicker.theme; label: "↑ 鼠标上移"
                    keyWidth: 2.2; height: 70
                    enabled: false
                }
                PickerKey {
                    theme: mousePicker.theme; label: "↓ 鼠标下移"
                    keyWidth: 2.2; height: 70
                    enabled: false
                }
            }
        }
    }
}
