import QtQuick

// 鼠标映射选择页：左侧鼠标按钮 + 中间鼠标轮廓 + 右侧移动方向（十字键）
Item {
    id: mousePicker

    required property var theme

    property string pendingKind: ""
    property string pendingValue: ""

    signal mouseActionSelected(string kind, string value)

    implicitHeight: mouseContainer.implicitHeight

    // 统一容器，水平居中
    Rectangle {
        id: mouseContainer

        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 760)
        implicitHeight: contentRow.implicitHeight + 32
        radius: 6
        color: "transparent"
        border.color: mousePicker.theme.border
        border.width: 1

        Row {
            id: contentRow

            anchors.centerIn: parent
            spacing: 24

            // ── 左列：鼠标按钮 ──
            Column {
                id: buttonColumn

                width: 240
                spacing: 8

                Text {
                    text: "鼠标按钮"
                    color: mousePicker.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                // 左键 / 中键 / 右键
                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "左键"
                        kind: "MouseButton"; value: "Left"
                        keyWidth: 1.58; height: 48
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Left"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Left")
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "中键"
                        kind: "MouseButton"; value: "Middle"
                        keyWidth: 1.58; height: 48
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Middle"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Middle")
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "右键"
                        kind: "MouseButton"; value: "Right"
                        keyWidth: 1.58; height: 48
                        enabled: true
                        selected: mousePicker.pendingKind === "MouseButton"
                            && mousePicker.pendingValue === "Right"
                        onClicked: mousePicker.mouseActionSelected("MouseButton", "Right")
                    }
                }

                // 滚轮上 / 侧键1
                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "滚轮上"
                        keyWidth: 2.44; height: 36
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "侧键1"
                        keyWidth: 2.44; height: 36
                        enabled: false
                    }
                }

                // 滚轮下 / 侧键2
                Row {
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "滚轮下"
                        keyWidth: 2.44; height: 36
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "侧键2"
                        keyWidth: 2.44; height: 36
                        enabled: false
                    }
                }
            }

            // ── 中列：鼠标轮廓图形 ──
            Item {
                width: 100
                height: buttonColumn.height
                anchors.verticalCenter: buttonColumn.verticalCenter

                Rectangle {
                    anchors.centerIn: parent
                    width: 70
                    height: 120
                    radius: 35
                    color: "transparent"
                    border.color: mousePicker.theme.accent
                    border.width: 1.5
                    opacity: 0.5

                    // 滚轮指示
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 20
                        width: 10
                        height: 20
                        radius: 5
                        color: Qt.rgba(
                            mousePicker.theme.accent.r,
                            mousePicker.theme.accent.g,
                            mousePicker.theme.accent.b, 0.4)
                    }

                    // 线缆
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        width: 2
                        height: 14
                        color: mousePicker.theme.border
                    }
                }
            }

            // ── 右列：鼠标移动方向（十字键布局）──
            Column {
                width: 240
                spacing: 8

                Text {
                    text: "鼠标移动"
                    color: mousePicker.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                // 十字键：上
                Item {
                    width: parent.width
                    height: 36

                    PickerKey {
                        anchors.horizontalCenter: parent.horizontalCenter
                        theme: mousePicker.theme; label: "↑ 上移"
                        keyWidth: 2.0; height: 36
                        enabled: false
                    }
                }

                // 十字键：左 + 右
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6

                    PickerKey {
                        theme: mousePicker.theme; label: "← 左移"
                        keyWidth: 2.0; height: 36
                        enabled: false
                    }
                    PickerKey {
                        theme: mousePicker.theme; label: "→ 右移"
                        keyWidth: 2.0; height: 36
                        enabled: false
                    }
                }

                // 十字键：下
                Item {
                    width: parent.width
                    height: 36

                    PickerKey {
                        anchors.horizontalCenter: parent.horizontalCenter
                        theme: mousePicker.theme; label: "↓ 下移"
                        keyWidth: 2.0; height: 36
                        enabled: false
                    }
                }
            }
        }
    }
}
