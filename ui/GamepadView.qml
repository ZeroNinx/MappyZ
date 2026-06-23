import QtQuick

// 手柄可视化面板：显示控件布局、按压状态和摇杆偏移
Panel {
    id: gamepadView

    required property var appController
    required property string selectedDevice
    required property string selectedDeviceDisplayName
    required property string selectedControl
    required property string latestControlForSelectedDevice

    heading: "Gamepad View"

    // ControlDot 点击触发，父级应设置 selectedControl 并取消 capture
    signal controlSelected(string controlId)

    // Back / Start 等非 ControlDot 按钮触发，父级只设置 selectedControl
    signal actionButtonControlSelected(string controlId)

    readonly property var _inputStateModel: appController
        ? appController.inputStateModel : null

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "#1f1f1f"
            border.color: gamepadView.theme.border
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 14
            text: gamepadView.selectedDeviceDisplayName || "No device selected"
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 38
            text: "Selected input: "
                + (gamepadView.latestControlForSelectedDevice || gamepadView.selectedControl)
            color: gamepadView.theme.muted
            font.pixelSize: 12
        }

        Rectangle {
            id: controllerBody

            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 520)
            height: 260
            radius: 74
            color: gamepadView.theme.surface
            border.color: gamepadView.theme.border
            border.width: 1
        }

        Rectangle {
            anchors.centerIn: controllerBody
            width: controllerBody.width * 0.46
            height: 114
            radius: 24
            color: "#1a1a1a"
            border.color: gamepadView.theme.border
        }

        // ── 左摇杆 ──

        ControlDot {
            id: leftStickDot

            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl

            anchors.left: controllerBody.left
            anchors.leftMargin: 92 + leftStickDot.inputState.axisX * 16
            anchors.top: controllerBody.top
            anchors.topMargin: 74 + leftStickDot.inputState.axisY * 16
            label: "LS"
            controlId: "left_stick"

            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        // ── 右摇杆 ──

        ControlDot {
            id: rightStickDot

            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl

            anchors.right: controllerBody.right
            anchors.rightMargin: 132 - rightStickDot.inputState.axisX * 16
            anchors.top: controllerBody.top
            anchors.topMargin: 148 + rightStickDot.inputState.axisY * 16
            label: "RS"
            controlId: "right_stick"

            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        // ── 十字键 ──

        Grid {
            anchors.left: controllerBody.left
            anchors.leftMargin: 78
            anchors.bottom: controllerBody.bottom
            anchors.bottomMargin: 42
            columns: 3
            rows: 3
            spacing: 4

            Item { width: 30; height: 30 }
            ControlDot {
                theme: gamepadView.theme
                inputStateModel: gamepadView._inputStateModel
                selectedDevice: gamepadView.selectedDevice
                selectedControl: gamepadView.selectedControl
                width: 30; height: 30; radius: 4
                label: "U"; controlId: "dpad_up"
                onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
            }
            Item { width: 30; height: 30 }

            ControlDot {
                theme: gamepadView.theme
                inputStateModel: gamepadView._inputStateModel
                selectedDevice: gamepadView.selectedDevice
                selectedControl: gamepadView.selectedControl
                width: 30; height: 30; radius: 4
                label: "L"; controlId: "dpad_left"
                onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
            }
            Rectangle {
                width: 30; height: 30; radius: 4
                color: "#333333"
                border.color: gamepadView.theme.border
            }
            ControlDot {
                theme: gamepadView.theme
                inputStateModel: gamepadView._inputStateModel
                selectedDevice: gamepadView.selectedDevice
                selectedControl: gamepadView.selectedControl
                width: 30; height: 30; radius: 4
                label: "R"; controlId: "dpad_right"
                onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
            }

            Item { width: 30; height: 30 }
            ControlDot {
                theme: gamepadView.theme
                inputStateModel: gamepadView._inputStateModel
                selectedDevice: gamepadView.selectedDevice
                selectedControl: gamepadView.selectedControl
                width: 30; height: 30; radius: 4
                label: "D"; controlId: "dpad_down"
                onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
            }
            Item { width: 30; height: 30 }
        }

        // ── ABXY 按钮 ──

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            anchors.right: controllerBody.right
            anchors.rightMargin: 82
            anchors.top: controllerBody.top
            anchors.topMargin: 68
            label: "Y"; controlId: "button_north"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            anchors.right: controllerBody.right
            anchors.rightMargin: 42
            anchors.top: controllerBody.top
            anchors.topMargin: 108
            label: "B"; controlId: "button_east"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            anchors.right: controllerBody.right
            anchors.rightMargin: 122
            anchors.top: controllerBody.top
            anchors.topMargin: 108
            label: "X"; controlId: "button_west"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            anchors.right: controllerBody.right
            anchors.rightMargin: 82
            anchors.top: controllerBody.top
            anchors.topMargin: 148
            label: "A"; controlId: "button_south"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        // ── Back / Start ──

        Row {
            anchors.horizontalCenter: controllerBody.horizontalCenter
            anchors.top: controllerBody.top
            anchors.topMargin: 92
            spacing: 10

            ActionButton {
                theme: gamepadView.theme
                label: "Back"
                onClicked: gamepadView.actionButtonControlSelected("button_back")
            }

            ActionButton {
                theme: gamepadView.theme
                label: "Start"
                onClicked: gamepadView.actionButtonControlSelected("button_start")
            }
        }

        // ── LT / RT 扳机 ──

        Row {
            anchors.left: controllerBody.left
            anchors.leftMargin: 96
            anchors.right: controllerBody.right
            anchors.rightMargin: 96
            anchors.bottom: controllerBody.top
            anchors.bottomMargin: 14
            spacing: 12

            Rectangle {
                InputControlState {
                    id: leftTriggerState

                    inputStateModel: gamepadView._inputStateModel
                    deviceId: gamepadView.selectedDevice
                    controlId: "left_trigger"
                }

                width: (parent.width - 12) / 2
                height: 28
                radius: 4
                color: gamepadView.selectedControl === "left_trigger"
                    ? gamepadView.theme.accentHover
                    : Qt.rgba(0.2, 0.2, 0.2,
                        1.0 - leftTriggerState.value * 0.5 + 0.5)
                border.color: leftTriggerState.value > 0.5
                    ? gamepadView.theme.accent : gamepadView.theme.border

                Text {
                    anchors.centerIn: parent
                    text: "LT " + (leftTriggerState.displayValue || "0.00")
                    color: gamepadView.theme.text
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadView.actionButtonControlSelected("left_trigger")
                }
            }

            Rectangle {
                InputControlState {
                    id: rightTriggerState

                    inputStateModel: gamepadView._inputStateModel
                    deviceId: gamepadView.selectedDevice
                    controlId: "right_trigger"
                }

                width: (parent.width - 12) / 2
                height: 28
                radius: 4
                color: gamepadView.selectedControl === "right_trigger"
                    ? gamepadView.theme.accentHover
                    : Qt.rgba(0.2, 0.2, 0.2,
                        1.0 - rightTriggerState.value * 0.5 + 0.5)
                border.color: rightTriggerState.value > 0.5
                    ? gamepadView.theme.accent : gamepadView.theme.border

                Text {
                    anchors.centerIn: parent
                    text: "RT " + (rightTriggerState.displayValue || "0.00")
                    color: gamepadView.theme.text
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadView.actionButtonControlSelected("right_trigger")
                }
            }
        }
    }
}
