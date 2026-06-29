import QtQuick

// 可编辑手柄映射视图：手柄实体居中 + 左右两列映射卡片
Panel {
    id: gamepadMappingView

    required property var appController
    required property string selectedDevice
    required property string selectedControl

    heading: "Gamepad Mapping View"

    signal controlSelected(string controlId)
    signal controlDoubleClicked(string controlId)

    readonly property var _mappingRuleModel: appController
        ? appController.mappingRuleModel : null
    readonly property var _inputStateModel: appController
        ? appController.inputStateModel : null

    // model reset 时递增，强制卡片 output 绑定刷新
    property int _mappingRevision: 0

    Connections {
        target: gamepadMappingView._mappingRuleModel
        function onModelReset() { gamepadMappingView._mappingRevision++ }
    }

    Item {
        id: layout
        anchors.fill: parent

        // 卡片列宽度：占可用宽度 24%，限制在 155~200 之间
        readonly property real cardColumnWidth: Math.max(
            Math.min(width * 0.24, 200), 155)

        // 手柄实体可用宽高
        readonly property real bodyAvailWidth: width - 2 * cardColumnWidth - 20
        // 上下卡片估算高度（3行+5行，含标题和间距）
        readonly property real bodyAvailHeight: height - 100 - 145 - 24
        // 手柄保持横向比例 1.8:1
        readonly property real bodyWidth: Math.min(bodyAvailWidth, bodyAvailHeight * 1.8, 460)
        readonly property real bodyHeight: bodyWidth / 1.8

        // ── 顶部：Back / Guide / Start ──
        MappingGroupCard {
            id: topCard
            z: 1
            theme: gamepadMappingView.theme
            mappingRuleModel: gamepadMappingView._mappingRuleModel
            selectedControl: gamepadMappingView.selectedControl
            mappingRevision: gamepadMappingView._mappingRevision
            title: "Back / Guide / Start"
            controls: [
                {controlId: "button_back", label: "Back"},
                {controlId: "button_guide", label: "Guide"},
                {controlId: "button_start", label: "Start"}
            ]
            width: layout.cardColumnWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 2
            onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
            onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
        }

        // ── 左列：LB/LT + Left Stick ──
        Column {
            id: leftColumn
            z: 1
            width: layout.cardColumnWidth
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: gamepadBody.verticalCenter
            spacing: 6

            MappingGroupCard {
                width: parent.width
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "LB / LT"
                controls: [
                    {controlId: "left_shoulder", label: "LB"},
                    {controlId: "left_trigger", label: "LT"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }

            MappingGroupCard {
                width: parent.width
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "Left Stick"
                controls: [
                    {controlId: "left_stick_button", label: "Click"},
                    {controlId: "left_stick_up", label: "Up"},
                    {controlId: "left_stick_left", label: "Left"},
                    {controlId: "left_stick_down", label: "Down"},
                    {controlId: "left_stick_right", label: "Right"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }
        }

        // ── 右列：RB/RT + ABXY ──
        Column {
            id: rightColumn
            z: 1
            width: layout.cardColumnWidth
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: gamepadBody.verticalCenter
            spacing: 6

            MappingGroupCard {
                width: parent.width
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "RB / RT"
                controls: [
                    {controlId: "right_shoulder", label: "RB"},
                    {controlId: "right_trigger", label: "RT"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }

            MappingGroupCard {
                width: parent.width
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "ABXY"
                controls: [
                    {controlId: "button_south", label: "A"},
                    {controlId: "button_east", label: "B"},
                    {controlId: "button_west", label: "X"},
                    {controlId: "button_north", label: "Y"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }
        }

        // ── 摇杆方向 InputControlState（需在手柄实体内引用）──

        InputControlState {
            id: lsButtonState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "left_stick_button"
        }
        InputControlState {
            id: lsUpState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "left_stick_up"
        }
        InputControlState {
            id: lsDownState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "left_stick_down"
        }
        InputControlState {
            id: lsLeftState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "left_stick_left"
        }
        InputControlState {
            id: lsRightState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "left_stick_right"
        }

        InputControlState {
            id: rsButtonState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "right_stick_button"
        }
        InputControlState {
            id: rsUpState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "right_stick_up"
        }
        InputControlState {
            id: rsDownState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "right_stick_down"
        }
        InputControlState {
            id: rsLeftState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "right_stick_left"
        }
        InputControlState {
            id: rsRightState
            inputStateModel: gamepadMappingView._inputStateModel
            deviceId: gamepadMappingView.selectedDevice
            controlId: "right_stick_right"
        }

        // ── 中间：手柄实体（横向比例，居中）──
        Rectangle {
            id: gamepadBody
            z: 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 4
            width: layout.bodyWidth
            height: layout.bodyHeight
            radius: width * 0.12
            color: gamepadMappingView.theme.surface
            border.color: gamepadMappingView.theme.border
            border.width: 1

            readonly property real bw: width
            readonly property real bh: height
            // 控件尺寸按手柄宽度缩放
            readonly property real dotSize: Math.max(bw * 0.08, 28)
            readonly property real dpadDotSize: Math.max(bw * 0.065, 22)
            readonly property real stickDotSize: Math.max(bw * 0.085, 30)

            // ── LT / RT 扳机（半嵌入顶部）──
            Rectangle {
                InputControlState {
                    id: ltState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "left_trigger"
                }
                x: gamepadBody.bw * 0.12
                y: -10
                width: gamepadBody.bw * 0.18; height: 18; radius: 3
                color: ltState.pressed ? gamepadMappingView.theme.accentSoft
                    : Qt.rgba(0.2, 0.2, 0.2, 1.0 - ltState.value * 0.5 + 0.5)
                border.color: ltState.value > 0.5 ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    anchors.centerIn: parent
                    text: "LT " + (ltState.displayValue || "0.00")
                    color: gamepadMappingView.theme.text; font.pixelSize: 9
                }
            }

            Rectangle {
                InputControlState {
                    id: rtState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "right_trigger"
                }
                x: gamepadBody.bw * 0.70
                y: -10
                width: gamepadBody.bw * 0.18; height: 18; radius: 3
                color: rtState.pressed ? gamepadMappingView.theme.accentSoft
                    : Qt.rgba(0.2, 0.2, 0.2, 1.0 - rtState.value * 0.5 + 0.5)
                border.color: rtState.value > 0.5 ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    anchors.centerIn: parent
                    text: "RT " + (rtState.displayValue || "0.00")
                    color: gamepadMappingView.theme.text; font.pixelSize: 9
                }
            }

            // ── LB / RB 肩键 ──
            Rectangle {
                InputControlState {
                    id: lbState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "left_shoulder"
                }
                x: gamepadBody.bw * 0.13
                y: gamepadBody.bh * 0.06
                width: gamepadBody.bw * 0.18; height: 18; radius: 4
                color: lbState.pressed ? gamepadMappingView.theme.accentSoft
                    : (gamepadMappingView.selectedControl === "left_shoulder"
                        ? gamepadMappingView.theme.accentHover : "#333333")
                border.color: lbState.pressed ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    anchors.centerIn: parent; text: "LB"
                    color: gamepadMappingView.theme.text; font.pixelSize: 10; font.bold: true
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadMappingView.controlSelected("left_shoulder")
                }
            }

            Rectangle {
                InputControlState {
                    id: rbState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "right_shoulder"
                }
                x: gamepadBody.bw * 0.69
                y: gamepadBody.bh * 0.06
                width: gamepadBody.bw * 0.18; height: 18; radius: 4
                color: rbState.pressed ? gamepadMappingView.theme.accentSoft
                    : (gamepadMappingView.selectedControl === "right_shoulder"
                        ? gamepadMappingView.theme.accentHover : "#333333")
                border.color: rbState.pressed ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    anchors.centerIn: parent; text: "RB"
                    color: gamepadMappingView.theme.text; font.pixelSize: 10; font.bold: true
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadMappingView.controlSelected("right_shoulder")
                }
            }

            // ── Guide / Back / Start ──
            Rectangle {
                InputControlState {
                    id: guideState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "button_guide"
                }
                x: gamepadBody.bw * 0.50 - 14
                y: gamepadBody.bh * 0.22 - 14
                width: 28; height: 28; radius: 14
                color: guideState.pressed ? gamepadMappingView.theme.accentSoft
                    : (gamepadMappingView.selectedControl === "button_guide"
                        ? gamepadMappingView.theme.accentHover : "#333333")
                border.color: guideState.pressed ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    anchors.centerIn: parent; text: "⬤"
                    color: gamepadMappingView.theme.muted; font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadMappingView.controlSelected("button_guide")
                }
            }

            Rectangle {
                InputControlState {
                    id: backState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "button_back"
                }
                x: gamepadBody.bw * 0.42 - backLabel.implicitWidth / 2 - 8
                y: gamepadBody.bh * 0.38 - 10
                width: backLabel.implicitWidth + 16; height: 20; radius: 3
                color: backState.pressed ? gamepadMappingView.theme.accentSoft
                    : (gamepadMappingView.selectedControl === "button_back"
                        ? gamepadMappingView.theme.accentHover : "#333333")
                border.color: backState.pressed ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    id: backLabel; anchors.centerIn: parent; text: "Back"
                    color: gamepadMappingView.theme.text; font.pixelSize: 9
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadMappingView.controlSelected("button_back")
                }
            }

            Rectangle {
                InputControlState {
                    id: startState
                    inputStateModel: gamepadMappingView._inputStateModel
                    deviceId: gamepadMappingView.selectedDevice
                    controlId: "button_start"
                }
                x: gamepadBody.bw * 0.58 - startLabel.implicitWidth / 2 - 8
                y: gamepadBody.bh * 0.38 - 10
                width: startLabel.implicitWidth + 16; height: 20; radius: 3
                color: startState.pressed ? gamepadMappingView.theme.accentSoft
                    : (gamepadMappingView.selectedControl === "button_start"
                        ? gamepadMappingView.theme.accentHover : "#333333")
                border.color: startState.pressed ? gamepadMappingView.theme.accent
                    : gamepadMappingView.theme.border
                Text {
                    id: startLabel; anchors.centerIn: parent; text: "Start"
                    color: gamepadMappingView.theme.text; font.pixelSize: 9
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: gamepadMappingView.controlSelected("button_start")
                }
            }

            // ── ABXY 菱形 ──
            Repeater {
                model: [
                    {cid: "button_north", lbl: "Y", rx: 0.77, ry: 0.22},
                    {cid: "button_east",  lbl: "B", rx: 0.85, ry: 0.38},
                    {cid: "button_west",  lbl: "X", rx: 0.69, ry: 0.38},
                    {cid: "button_south", lbl: "A", rx: 0.77, ry: 0.54}
                ]

                ControlDot {
                    required property var modelData
                    theme: gamepadMappingView.theme
                    inputStateModel: gamepadMappingView._inputStateModel
                    selectedDevice: gamepadMappingView.selectedDevice
                    selectedControl: gamepadMappingView.selectedControl
                    x: gamepadBody.bw * modelData.rx - gamepadBody.dotSize / 2
                    y: gamepadBody.bh * modelData.ry - gamepadBody.dotSize / 2
                    width: gamepadBody.dotSize; height: gamepadBody.dotSize
                    radius: gamepadBody.dotSize / 2
                    label: modelData.lbl
                    controlId: modelData.cid
                    onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
                }
            }

            // ── D-Pad 十字键 ──
            Grid {
                x: gamepadBody.bw * 0.28 - (gamepadBody.dpadDotSize * 3 + 8) / 2
                y: gamepadBody.bh * 0.68 - (gamepadBody.dpadDotSize * 3 + 8) / 2
                columns: 3; rows: 3; spacing: 4

                Item { width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize }
                ControlDot {
                    theme: gamepadMappingView.theme
                    inputStateModel: gamepadMappingView._inputStateModel
                    selectedDevice: gamepadMappingView.selectedDevice
                    selectedControl: gamepadMappingView.selectedControl
                    width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize; radius: 4
                    label: "U"; controlId: "dpad_up"
                    onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
                }
                Item { width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize }

                ControlDot {
                    theme: gamepadMappingView.theme
                    inputStateModel: gamepadMappingView._inputStateModel
                    selectedDevice: gamepadMappingView.selectedDevice
                    selectedControl: gamepadMappingView.selectedControl
                    width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize; radius: 4
                    label: "L"; controlId: "dpad_left"
                    onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
                }
                Rectangle {
                    width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize; radius: 4
                    color: "#333333"; border.color: gamepadMappingView.theme.border
                }
                ControlDot {
                    theme: gamepadMappingView.theme
                    inputStateModel: gamepadMappingView._inputStateModel
                    selectedDevice: gamepadMappingView.selectedDevice
                    selectedControl: gamepadMappingView.selectedControl
                    width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize; radius: 4
                    label: "R"; controlId: "dpad_right"
                    onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
                }

                Item { width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize }
                ControlDot {
                    theme: gamepadMappingView.theme
                    inputStateModel: gamepadMappingView._inputStateModel
                    selectedDevice: gamepadMappingView.selectedDevice
                    selectedControl: gamepadMappingView.selectedControl
                    width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize; radius: 4
                    label: "D"; controlId: "dpad_down"
                    onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
                }
                Item { width: gamepadBody.dpadDotSize; height: gamepadBody.dpadDotSize }
            }

            // ── 左摇杆（带轴偏移 + 方向指示器）──

            Repeater {
                model: [
                    {dirState: lsUpState,    dx: 0,   dy: -1},
                    {dirState: lsDownState,  dx: 0,   dy: 1},
                    {dirState: lsLeftState,  dx: -1,  dy: 0},
                    {dirState: lsRightState, dx: 1,   dy: 0}
                ]

                Rectangle {
                    required property var modelData
                    readonly property real cx: gamepadBody.bw * 0.24
                    readonly property real cy: gamepadBody.bh * 0.34
                    readonly property real offset: gamepadBody.stickDotSize * 0.85
                    x: cx - 5 + modelData.dx * offset
                    y: cy - 5 + modelData.dy * offset
                    width: 10; height: 10; radius: 5
                    color: modelData.dirState.pressed
                        ? gamepadMappingView.theme.accent : "transparent"
                    border.color: modelData.dirState.pressed
                        ? gamepadMappingView.theme.accent : gamepadMappingView.theme.border
                    border.width: 1
                    opacity: modelData.dirState.pressed ? 1.0 : 0.3
                }
            }

            ControlDot {
                id: leftStickDot
                theme: gamepadMappingView.theme
                inputStateModel: gamepadMappingView._inputStateModel
                selectedDevice: gamepadMappingView.selectedDevice
                selectedControl: gamepadMappingView.selectedControl
                x: gamepadBody.bw * 0.24 - gamepadBody.stickDotSize / 2
                    + inputState.axisX * gamepadBody.stickDotSize * 0.4
                y: gamepadBody.bh * 0.34 - gamepadBody.stickDotSize / 2
                    + inputState.axisY * gamepadBody.stickDotSize * 0.4
                width: gamepadBody.stickDotSize; height: gamepadBody.stickDotSize
                radius: gamepadBody.stickDotSize / 2
                label: "LS"
                controlId: "left_stick"
                active: inputState.pressed || lsButtonState.pressed
                    || Math.abs(inputState.axisX) > 0.15
                    || Math.abs(inputState.axisY) > 0.15
                onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
            }

            // ── 右摇杆（带轴偏移 + 方向指示器）──

            Repeater {
                model: [
                    {dirState: rsUpState,    dx: 0,   dy: -1},
                    {dirState: rsDownState,  dx: 0,   dy: 1},
                    {dirState: rsLeftState,  dx: -1,  dy: 0},
                    {dirState: rsRightState, dx: 1,   dy: 0}
                ]

                Rectangle {
                    required property var modelData
                    readonly property real cx: gamepadBody.bw * 0.60
                    readonly property real cy: gamepadBody.bh * 0.68
                    readonly property real offset: gamepadBody.stickDotSize * 0.85
                    x: cx - 5 + modelData.dx * offset
                    y: cy - 5 + modelData.dy * offset
                    width: 10; height: 10; radius: 5
                    color: modelData.dirState.pressed
                        ? gamepadMappingView.theme.accent : "transparent"
                    border.color: modelData.dirState.pressed
                        ? gamepadMappingView.theme.accent : gamepadMappingView.theme.border
                    border.width: 1
                    opacity: modelData.dirState.pressed ? 1.0 : 0.3
                }
            }

            ControlDot {
                id: rightStickDot
                theme: gamepadMappingView.theme
                inputStateModel: gamepadMappingView._inputStateModel
                selectedDevice: gamepadMappingView.selectedDevice
                selectedControl: gamepadMappingView.selectedControl
                x: gamepadBody.bw * 0.60 - gamepadBody.stickDotSize / 2
                    + inputState.axisX * gamepadBody.stickDotSize * 0.4
                y: gamepadBody.bh * 0.68 - gamepadBody.stickDotSize / 2
                    + inputState.axisY * gamepadBody.stickDotSize * 0.4
                width: gamepadBody.stickDotSize; height: gamepadBody.stickDotSize
                radius: gamepadBody.stickDotSize / 2
                label: "RS"
                controlId: "right_stick"
                active: inputState.pressed || rsButtonState.pressed
                    || Math.abs(inputState.axisX) > 0.15
                    || Math.abs(inputState.axisY) > 0.15
                onSelected: function(cid) { gamepadMappingView.controlSelected(cid) }
            }
        }

        // ── 底部行：D-Pad + Right Stick ──
        Row {
            id: bottomRow
            z: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            spacing: 12

            MappingGroupCard {
                width: layout.cardColumnWidth
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "D-Pad"
                controls: [
                    {controlId: "dpad_up", label: "Up"},
                    {controlId: "dpad_down", label: "Down"},
                    {controlId: "dpad_left", label: "Left"},
                    {controlId: "dpad_right", label: "Right"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }

            MappingGroupCard {
                width: layout.cardColumnWidth
                theme: gamepadMappingView.theme
                mappingRuleModel: gamepadMappingView._mappingRuleModel
                selectedControl: gamepadMappingView.selectedControl
                mappingRevision: gamepadMappingView._mappingRevision
                title: "Right Stick"
                controls: [
                    {controlId: "right_stick_button", label: "Click"},
                    {controlId: "right_stick_up", label: "Up"},
                    {controlId: "right_stick_down", label: "Down"},
                    {controlId: "right_stick_left", label: "Left"},
                    {controlId: "right_stick_right", label: "Right"}
                ]
                onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
            }
        }
    }
}
