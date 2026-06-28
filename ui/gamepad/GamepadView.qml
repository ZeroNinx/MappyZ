import QtQuick

// 手柄可视化面板：按 Xbox 360 实际布局分布控件
Panel {
    id: gamepadView

    required property var appController
    required property string selectedDevice
    required property string selectedDeviceDisplayName
    required property string selectedControl
    required property string latestControlForSelectedDevice

    heading: "Gamepad View"

    signal controlSelected(string controlId)
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

        // ── 标题区（固定高度，不被 trigger 遮挡） ──

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 14
            text: gamepadView.selectedDeviceDisplayName || "No device selected"
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 38
            text: "Selected input: "
                + (gamepadView.latestControlForSelectedDevice || gamepadView.selectedControl)
            color: gamepadView.theme.muted
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        // ── 手柄主体（更紧凑，给标题区留出空间） ──

        Rectangle {
            id: body

            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max((parent.height - 240) / 2, 96)
            width: Math.min(parent.width - 40, 500)
            height: 240
            radius: 64
            color: gamepadView.theme.surface
            border.color: gamepadView.theme.border
            border.width: 1
        }

        // ── 比例坐标系（参照 Xbox 360 实际布局） ──
        QtObject {
            id: layout

            readonly property real bx: body.x
            readonly property real by: body.y
            readonly property real bw: body.width
            readonly property real bh: body.height

            // LS 左上 (24%, 36%)
            readonly property real lsX: bx + bw * 0.24
            readonly property real lsY: by + bh * 0.36

            // ABXY 右上菱形中心 (77%, 38%)
            readonly property real abxyX: bx + bw * 0.77
            readonly property real abxyY: by + bh * 0.38
            readonly property int abxyR: 34

            // D-pad 左下 (30%, 70%)
            readonly property real dpadX: bx + bw * 0.30
            readonly property real dpadY: by + bh * 0.70

            // RS 右下 (60%, 70%)
            readonly property real rsX: bx + bw * 0.60
            readonly property real rsY: by + bh * 0.70

            // Guide 上方正中 (50%, 22%)
            readonly property real guideX: bx + bw * 0.50
            readonly property real guideY: by + bh * 0.22

            // Back/Start 中间偏上 (43%/57%, 42%)
            readonly property real backX: bx + bw * 0.43
            readonly property real startX: bx + bw * 0.57
            readonly property real menuY: by + bh * 0.42

            // 肩键（紧贴 body 顶部，与扳机对齐）
            readonly property real shoulderW: bw * 0.18
            readonly property real lShoulderX: bx + bw * 0.15
            readonly property real rShoulderX: bx + bw * 0.67
        }

        // ── LT / RT 扳机（半嵌入 body 顶部，形成一体感） ──

        Rectangle {
            InputControlState {
                id: leftTriggerState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "left_trigger"
            }

            x: layout.lShoulderX
            y: layout.by - 14
            width: layout.shoulderW
            height: 22
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
                font.pixelSize: 11
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

            x: layout.rShoulderX
            y: layout.by - 14
            width: layout.shoulderW
            height: 22
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
                font.pixelSize: 11
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("right_trigger")
            }
        }

        // ── LB / RB 肩键（body 内部顶部） ──

        Rectangle {
            InputControlState {
                id: leftShoulderState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "left_shoulder"
            }

            x: layout.lShoulderX
            y: layout.by + 12
            width: layout.shoulderW
            height: 20
            radius: 4
            color: leftShoulderState.pressed
                ? gamepadView.theme.accentSoft
                : (gamepadView.selectedControl === "left_shoulder"
                    ? gamepadView.theme.accentHover : "#333333")
            border.color: leftShoulderState.pressed
                ? gamepadView.theme.accent : gamepadView.theme.border

            Text {
                anchors.centerIn: parent
                text: "LB"
                color: gamepadView.theme.text
                font.pixelSize: 11
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("left_shoulder")
            }
        }

        Rectangle {
            InputControlState {
                id: rightShoulderState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "right_shoulder"
            }

            x: layout.rShoulderX
            y: layout.by + 12
            width: layout.shoulderW
            height: 20
            radius: 4
            color: rightShoulderState.pressed
                ? gamepadView.theme.accentSoft
                : (gamepadView.selectedControl === "right_shoulder"
                    ? gamepadView.theme.accentHover : "#333333")
            border.color: rightShoulderState.pressed
                ? gamepadView.theme.accent : gamepadView.theme.border

            Text {
                anchors.centerIn: parent
                text: "RB"
                color: gamepadView.theme.text
                font.pixelSize: 11
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("right_shoulder")
            }
        }

        // ── Guide 按钮（上方正中） ──

        Rectangle {
            InputControlState {
                id: guideButtonState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "button_guide"
            }

            x: layout.guideX - 14
            y: layout.guideY - 14
            width: 28
            height: 28
            radius: 14
            color: guideButtonState.pressed
                ? gamepadView.theme.accentSoft
                : (gamepadView.selectedControl === "button_guide"
                    ? gamepadView.theme.accentHover : "#333333")
            border.color: guideButtonState.pressed
                ? gamepadView.theme.accent : gamepadView.theme.border

            Text {
                anchors.centerIn: parent
                text: "⬤"
                color: gamepadView.theme.muted
                font.pixelSize: 10
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("button_guide")
            }
        }

        // ── Back / Start（Guide 两侧，宽度跟随文字避免截断）──

        Rectangle {
            InputControlState {
                id: backButtonState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "button_back"
            }

            x: layout.backX - width / 2
            y: layout.menuY - 11
            width: backBtnLabel.implicitWidth + 16
            height: 22
            radius: 3
            color: backButtonState.pressed
                ? gamepadView.theme.accentSoft
                : (gamepadView.selectedControl === "button_back"
                    ? gamepadView.theme.accentHover : "#333333")
            border.color: backButtonState.pressed
                ? gamepadView.theme.accent : gamepadView.theme.border

            Text {
                id: backBtnLabel

                anchors.centerIn: parent
                text: "Back"
                color: gamepadView.theme.text
                font.pixelSize: 10
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("button_back")
            }
        }

        Rectangle {
            InputControlState {
                id: startButtonState

                inputStateModel: gamepadView._inputStateModel
                deviceId: gamepadView.selectedDevice
                controlId: "button_start"
            }

            x: layout.startX - width / 2
            y: layout.menuY - 11
            width: startBtnLabel.implicitWidth + 16
            height: 22
            radius: 3
            color: startButtonState.pressed
                ? gamepadView.theme.accentSoft
                : (gamepadView.selectedControl === "button_start"
                    ? gamepadView.theme.accentHover : "#333333")
            border.color: startButtonState.pressed
                ? gamepadView.theme.accent : gamepadView.theme.border

            Text {
                id: startBtnLabel

                anchors.centerIn: parent
                text: "Start"
                color: gamepadView.theme.text
                font.pixelSize: 10
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: gamepadView.actionButtonControlSelected("button_start")
            }
        }

        // ── 左摇杆（左上） ──

        InputControlState {
            id: leftStickButtonState

            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "left_stick_button"
        }

        // 左摇杆方向高亮指示器
        InputControlState {
            id: lsUpState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "left_stick_up"
        }
        InputControlState {
            id: lsDownState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "left_stick_down"
        }
        InputControlState {
            id: lsLeftState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "left_stick_left"
        }
        InputControlState {
            id: lsRightState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "left_stick_right"
        }

        Repeater {
            model: [
                { dirState: lsUpState,    dx: 0,   dy: -26 },
                { dirState: lsDownState,  dx: 0,   dy: 26  },
                { dirState: lsLeftState,  dx: -26, dy: 0   },
                { dirState: lsRightState, dx: 26,  dy: 0   }
            ]

            Rectangle {
                required property var modelData
                x: layout.lsX - 5 + modelData.dx
                y: layout.lsY - 5 + modelData.dy
                width: 10
                height: 10
                radius: 5
                color: modelData.dirState.pressed
                    ? gamepadView.theme.accent : "transparent"
                border.color: modelData.dirState.pressed
                    ? gamepadView.theme.accent : gamepadView.theme.border
                border.width: 1
                opacity: modelData.dirState.pressed ? 1.0 : 0.3
            }
        }

        ControlDot {
            id: leftStickDot

            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl

            x: layout.lsX - 19 + inputState.axisX * 14
            y: layout.lsY - 19 + inputState.axisY * 14
            label: "LS"
            controlId: "left_stick"

            active: inputState.pressed
                || leftStickButtonState.pressed
                || Math.abs(inputState.axisX) > 0.15
                || Math.abs(inputState.axisY) > 0.15

            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        // ── 右摇杆（右下） ──

        InputControlState {
            id: rightStickButtonState

            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "right_stick_button"
        }

        // 右摇杆方向高亮指示器
        InputControlState {
            id: rsUpState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "right_stick_up"
        }
        InputControlState {
            id: rsDownState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "right_stick_down"
        }
        InputControlState {
            id: rsLeftState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "right_stick_left"
        }
        InputControlState {
            id: rsRightState
            inputStateModel: gamepadView._inputStateModel
            deviceId: gamepadView.selectedDevice
            controlId: "right_stick_right"
        }

        Repeater {
            model: [
                { dirState: rsUpState,    dx: 0,   dy: -26 },
                { dirState: rsDownState,  dx: 0,   dy: 26  },
                { dirState: rsLeftState,  dx: -26, dy: 0   },
                { dirState: rsRightState, dx: 26,  dy: 0   }
            ]

            Rectangle {
                required property var modelData
                x: layout.rsX - 5 + modelData.dx
                y: layout.rsY - 5 + modelData.dy
                width: 10
                height: 10
                radius: 5
                color: modelData.dirState.pressed
                    ? gamepadView.theme.accent : "transparent"
                border.color: modelData.dirState.pressed
                    ? gamepadView.theme.accent : gamepadView.theme.border
                border.width: 1
                opacity: modelData.dirState.pressed ? 1.0 : 0.3
            }
        }

        ControlDot {
            id: rightStickDot

            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl

            x: layout.rsX - 19 + inputState.axisX * 14
            y: layout.rsY - 19 + inputState.axisY * 14
            label: "RS"
            controlId: "right_stick"

            active: inputState.pressed
                || rightStickButtonState.pressed
                || Math.abs(inputState.axisX) > 0.15
                || Math.abs(inputState.axisY) > 0.15

            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        // ── 十字键（左下） ──

        Grid {
            x: layout.dpadX - 49
            y: layout.dpadY - 49
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

        // ── ABXY 菱形（右上） ──

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            x: layout.abxyX - 19
            y: layout.abxyY - layout.abxyR - 19
            label: "Y"; controlId: "button_north"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            x: layout.abxyX + layout.abxyR - 19
            y: layout.abxyY - 19
            label: "B"; controlId: "button_east"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            x: layout.abxyX - layout.abxyR - 19
            y: layout.abxyY - 19
            label: "X"; controlId: "button_west"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }

        ControlDot {
            theme: gamepadView.theme
            inputStateModel: gamepadView._inputStateModel
            selectedDevice: gamepadView.selectedDevice
            selectedControl: gamepadView.selectedControl
            x: layout.abxyX - 19
            y: layout.abxyY + layout.abxyR - 19
            label: "A"; controlId: "button_south"
            onSelected: function(controlId) { gamepadView.controlSelected(controlId) }
        }
    }
}
