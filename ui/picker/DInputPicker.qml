import QtQuick

// DInput 映射选择页：未来扩展占位，所有 tile disabled
Item {
    id: dinputPicker

    required property var theme

    implicitHeight: dinputLayout.implicitHeight

    Column {
        id: dinputLayout

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 16

        Text {
            text: "提示：选择一个 DInput 控制来作为映射目标"
            color: dinputPicker.theme.accent
            font.pixelSize: 12
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 24

            // ── 摇杆 / 轴（模拟）──
            Rectangle {
                width: 340
                height: 210
                radius: 4
                color: "transparent"
                border.color: dinputPicker.theme.border
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Text {
                        text: "摇杆 / 轴（模拟）"
                        color: dinputPicker.theme.text
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            { axis: "左摇杆 X", neg: "X-", pos: "X+" },
                            { axis: "左摇杆 Y", neg: "Y-", pos: "Y+" },
                            { axis: "右摇杆 X", neg: "X-", pos: "X+" },
                            { axis: "右摇杆 Y", neg: "Y-", pos: "Y+" }
                        ]

                        Row {
                            required property var modelData
                            spacing: 8

                            Text {
                                width: 80
                                text: modelData.axis
                                color: dinputPicker.theme.muted
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                height: 32
                            }

                            PickerKey {
                                theme: dinputPicker.theme; label: modelData.neg
                                keyWidth: 2.0; height: 32; enabled: false
                            }
                            PickerKey {
                                theme: dinputPicker.theme; label: modelData.pos
                                keyWidth: 2.0; height: 32; enabled: false
                            }
                        }
                    }
                }
            }

            // ── 扳机（轴）──
            Rectangle {
                width: 200
                height: 210
                radius: 4
                color: "transparent"
                border.color: dinputPicker.theme.border
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Text {
                        text: "扳机（轴）"
                        color: dinputPicker.theme.text
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Text {
                        text: "左扳机（LT / L2）"
                        color: dinputPicker.theme.muted
                        font.pixelSize: 12
                    }

                    PickerKey {
                        theme: dinputPicker.theme; label: "轴 +"
                        keyWidth: 3.0; height: 36; enabled: false
                    }

                    Text {
                        text: "右扳机（RT / R2）"
                        color: dinputPicker.theme.muted
                        font.pixelSize: 12
                    }

                    PickerKey {
                        theme: dinputPicker.theme; label: "轴 +"
                        keyWidth: 3.0; height: 36; enabled: false
                    }

                    Text {
                        text: "扳机使用正向轴值（0 → 1）"
                        color: dinputPicker.theme.muted
                        font.pixelSize: 10
                    }
                }
            }

            // ── 方向键（D-Pad）──
            Rectangle {
                width: 180
                height: 210
                radius: 4
                color: "transparent"
                border.color: dinputPicker.theme.border
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "方向键（D-Pad）"
                        color: dinputPicker.theme.text
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Item { width: 1; height: 8 }

                    // 十字布局
                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 4

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            PickerKey {
                                theme: dinputPicker.theme; label: "上"
                                keyWidth: 1.5; height: 36; enabled: false
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4
                            PickerKey {
                                theme: dinputPicker.theme; label: "左"
                                keyWidth: 1.5; height: 36; enabled: false
                            }
                            PickerKey {
                                theme: dinputPicker.theme; label: "右"
                                keyWidth: 1.5; height: 36; enabled: false
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            PickerKey {
                                theme: dinputPicker.theme; label: "下"
                                keyWidth: 1.5; height: 36; enabled: false
                            }
                        }
                    }
                }
            }
        }

        // ── 按钮（Button）──
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 340 + 200 + 180 + 2 * 24
            height: dinputButtonColumn.implicitHeight + 24
            radius: 4
            color: "transparent"
            border.color: dinputPicker.theme.border
            border.width: 1

            Column {
                id: dinputButtonColumn

                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "按钮（Button）"
                    color: dinputPicker.theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4
                    Repeater {
                        model: ["B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8"]
                        PickerKey {
                            required property string modelData
                            theme: dinputPicker.theme; label: modelData
                            keyWidth: 1.5; height: 36; enabled: false
                        }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4
                    Repeater {
                        model: ["B9", "B10", "B11", "B12", "B13", "B14", "B15", "B16"]
                        PickerKey {
                            required property string modelData
                            theme: dinputPicker.theme; label: modelData
                            keyWidth: 1.5; height: 36; enabled: false
                        }
                    }
                }
            }
        }
    }
}
