import QtQuick

// 手柄图示组件：绘制手柄轮廓及所有控件可视化，不包含映射行列表
Rectangle {
    id: glyph

    required property var theme
    required property var inputStateModel
    required property string selectedDevice
    required property string selectedControl

    signal controlSelected(string controlId)

    radius: width * 0.12
    color: glyph.theme.surface
    border.color: glyph.theme.border
    border.width: 1

    readonly property real bw: width
    readonly property real bh: height
    // 控件尺寸按手柄宽度缩放
    readonly property real dotSize: Math.max(bw * 0.07, 26)
    readonly property real dpadDotSize: Math.max(bw * 0.055, 20)
    readonly property real stickDotSize: Math.max(bw * 0.085, 30)
    // ABXY cluster 参数
    readonly property real abxyRadius: Math.max(bw * 0.07, 26)
    readonly property real abxyBtnSize: Math.max(bw * 0.06, 22)

    // ── 摇杆方向 InputControlState ──

    InputControlState {
        id: lsButtonState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "left_stick_button"
    }
    InputControlState {
        id: lsUpState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "left_stick_up"
    }
    InputControlState {
        id: lsDownState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "left_stick_down"
    }
    InputControlState {
        id: lsLeftState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "left_stick_left"
    }
    InputControlState {
        id: lsRightState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "left_stick_right"
    }

    InputControlState {
        id: rsButtonState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "right_stick_button"
    }
    InputControlState {
        id: rsUpState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "right_stick_up"
    }
    InputControlState {
        id: rsDownState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "right_stick_down"
    }
    InputControlState {
        id: rsLeftState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "right_stick_left"
    }
    InputControlState {
        id: rsRightState
        inputStateModel: glyph.inputStateModel
        deviceId: glyph.selectedDevice
        controlId: "right_stick_right"
    }

    // ── LT / RT 扳机（半嵌入顶部）──

    Rectangle {
        InputControlState {
            id: ltState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "left_trigger"
        }
        x: glyph.bw * 0.12
        y: -10
        width: glyph.bw * 0.18; height: 18; radius: 3
        color: ltState.pressed ? glyph.theme.accentSoft
            : Qt.rgba(0.2, 0.2, 0.2, 1.0 - ltState.value * 0.5 + 0.5)
        border.color: ltState.value > 0.5 ? glyph.theme.accent
            : glyph.theme.border
        Text {
            anchors.centerIn: parent
            text: "LT " + (ltState.displayValue || "0.00")
            color: glyph.theme.text; font.pixelSize: 9
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("left_trigger")
        }
    }

    Rectangle {
        InputControlState {
            id: rtState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "right_trigger"
        }
        x: glyph.bw * 0.70
        y: -10
        width: glyph.bw * 0.18; height: 18; radius: 3
        color: rtState.pressed ? glyph.theme.accentSoft
            : Qt.rgba(0.2, 0.2, 0.2, 1.0 - rtState.value * 0.5 + 0.5)
        border.color: rtState.value > 0.5 ? glyph.theme.accent
            : glyph.theme.border
        Text {
            anchors.centerIn: parent
            text: "RT " + (rtState.displayValue || "0.00")
            color: glyph.theme.text; font.pixelSize: 9
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("right_trigger")
        }
    }

    // ── LB / RB 肩键 ──

    Rectangle {
        InputControlState {
            id: lbState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "left_shoulder"
        }
        x: glyph.bw * 0.13
        y: glyph.bh * 0.06
        width: glyph.bw * 0.18; height: 18; radius: 4
        color: lbState.pressed ? glyph.theme.accentSoft
            : (glyph.selectedControl === "left_shoulder"
                ? glyph.theme.accentHover : "#333333")
        border.color: lbState.pressed ? glyph.theme.accent
            : glyph.theme.border
        Text {
            anchors.centerIn: parent; text: "LB"
            color: glyph.theme.text; font.pixelSize: 10; font.bold: true
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("left_shoulder")
        }
    }

    Rectangle {
        InputControlState {
            id: rbState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "right_shoulder"
        }
        x: glyph.bw * 0.69
        y: glyph.bh * 0.06
        width: glyph.bw * 0.18; height: 18; radius: 4
        color: rbState.pressed ? glyph.theme.accentSoft
            : (glyph.selectedControl === "right_shoulder"
                ? glyph.theme.accentHover : "#333333")
        border.color: rbState.pressed ? glyph.theme.accent
            : glyph.theme.border
        Text {
            anchors.centerIn: parent; text: "RB"
            color: glyph.theme.text; font.pixelSize: 10; font.bold: true
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("right_shoulder")
        }
    }

    // ── Guide / Back / Start ──

    Rectangle {
        InputControlState {
            id: guideState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "button_guide"
        }
        x: glyph.bw * 0.50 - 14
        y: glyph.bh * 0.22 - 14
        width: 28; height: 28; radius: 14
        color: guideState.pressed ? glyph.theme.accentSoft
            : (glyph.selectedControl === "button_guide"
                ? glyph.theme.accentHover : "#333333")
        border.color: guideState.pressed ? glyph.theme.accent
            : glyph.theme.border
        Text {
            anchors.centerIn: parent; text: "G"
            color: guideState.pressed ? "#ffffff" : glyph.theme.muted
            font.pixelSize: 10; font.bold: true
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("button_guide")
        }
    }

    Rectangle {
        InputControlState {
            id: backState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "button_back"
        }
        x: glyph.bw * 0.42 - backLabel.implicitWidth / 2 - 8
        y: glyph.bh * 0.38 - 10
        width: backLabel.implicitWidth + 16; height: 20; radius: 3
        color: backState.pressed ? glyph.theme.accentSoft
            : (glyph.selectedControl === "button_back"
                ? glyph.theme.accentHover : "#333333")
        border.color: backState.pressed ? glyph.theme.accent
            : glyph.theme.border
        Text {
            id: backLabel; anchors.centerIn: parent; text: "Back"
            color: glyph.theme.text; font.pixelSize: 9
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("button_back")
        }
    }

    Rectangle {
        InputControlState {
            id: startState
            inputStateModel: glyph.inputStateModel
            deviceId: glyph.selectedDevice
            controlId: "button_start"
        }
        x: glyph.bw * 0.58 - startLabel.implicitWidth / 2 - 8
        y: glyph.bh * 0.38 - 10
        width: startLabel.implicitWidth + 16; height: 20; radius: 3
        color: startState.pressed ? glyph.theme.accentSoft
            : (glyph.selectedControl === "button_start"
                ? glyph.theme.accentHover : "#333333")
        border.color: startState.pressed ? glyph.theme.accent
            : glyph.theme.border
        Text {
            id: startLabel; anchors.centerIn: parent; text: "Start"
            color: glyph.theme.text; font.pixelSize: 9
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: glyph.controlSelected("button_start")
        }
    }

    // ── ABXY 菱形 cluster ──

    Item {
        id: abxyCluster
        // cluster 中心点
        readonly property real cx: glyph.bw * 0.77
        readonly property real cy: glyph.bh * 0.38
        readonly property real r: glyph.abxyRadius
        readonly property real btnSize: glyph.abxyBtnSize

        x: 0; y: 0; width: glyph.bw; height: glyph.bh

        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            x: abxyCluster.cx - abxyCluster.btnSize / 2
            y: abxyCluster.cy - abxyCluster.r - abxyCluster.btnSize / 2
            width: abxyCluster.btnSize; height: abxyCluster.btnSize
            radius: abxyCluster.btnSize / 2
            label: "Y"; controlId: "button_north"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            x: abxyCluster.cx + abxyCluster.r - abxyCluster.btnSize / 2
            y: abxyCluster.cy - abxyCluster.btnSize / 2
            width: abxyCluster.btnSize; height: abxyCluster.btnSize
            radius: abxyCluster.btnSize / 2
            label: "B"; controlId: "button_east"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            x: abxyCluster.cx - abxyCluster.r - abxyCluster.btnSize / 2
            y: abxyCluster.cy - abxyCluster.btnSize / 2
            width: abxyCluster.btnSize; height: abxyCluster.btnSize
            radius: abxyCluster.btnSize / 2
            label: "X"; controlId: "button_west"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            x: abxyCluster.cx - abxyCluster.btnSize / 2
            y: abxyCluster.cy + abxyCluster.r - abxyCluster.btnSize / 2
            width: abxyCluster.btnSize; height: abxyCluster.btnSize
            radius: abxyCluster.btnSize / 2
            label: "A"; controlId: "button_south"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
    }

    // ── D-Pad 十字键 ──

    Grid {
        x: glyph.bw * 0.28 - (glyph.dpadDotSize * 3 + 4) / 2
        y: glyph.bh * 0.68 - (glyph.dpadDotSize * 3 + 4) / 2
        columns: 3; rows: 3; spacing: 2

        Item { width: glyph.dpadDotSize; height: glyph.dpadDotSize }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            width: glyph.dpadDotSize; height: glyph.dpadDotSize; radius: 4
            label: "U"; controlId: "dpad_up"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        Item { width: glyph.dpadDotSize; height: glyph.dpadDotSize }

        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            width: glyph.dpadDotSize; height: glyph.dpadDotSize; radius: 4
            label: "L"; controlId: "dpad_left"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        Item {
            width: glyph.dpadDotSize; height: glyph.dpadDotSize
        }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            width: glyph.dpadDotSize; height: glyph.dpadDotSize; radius: 4
            label: "R"; controlId: "dpad_right"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }

        Item { width: glyph.dpadDotSize; height: glyph.dpadDotSize }
        ControlDot {
            theme: glyph.theme
            inputStateModel: glyph.inputStateModel
            selectedDevice: glyph.selectedDevice
            selectedControl: glyph.selectedControl
            width: glyph.dpadDotSize; height: glyph.dpadDotSize; radius: 4
            label: "D"; controlId: "dpad_down"
            onSelected: function(cid) { glyph.controlSelected(cid) }
        }
        Item { width: glyph.dpadDotSize; height: glyph.dpadDotSize }
    }

    // ── 左摇杆方向指示器 ──

    Repeater {
        model: [
            {dirState: lsUpState,    dx: 0,   dy: -1},
            {dirState: lsDownState,  dx: 0,   dy: 1},
            {dirState: lsLeftState,  dx: -1,  dy: 0},
            {dirState: lsRightState, dx: 1,   dy: 0}
        ]

        Rectangle {
            required property var modelData
            readonly property real cx: glyph.bw * 0.24
            readonly property real cy: glyph.bh * 0.34
            readonly property real offset: glyph.stickDotSize * 0.65
            x: cx - 6 + modelData.dx * offset
            y: cy - 6 + modelData.dy * offset
            width: 12; height: 12; radius: 6
            color: modelData.dirState.pressed
                ? glyph.theme.accent : "transparent"
            border.color: modelData.dirState.pressed
                ? glyph.theme.accent : glyph.theme.border
            border.width: 1.5
            opacity: modelData.dirState.pressed ? 1.0 : 0.5
        }
    }

    // ── 左摇杆（带轴偏移）──

    ControlDot {
        id: leftStickDot
        theme: glyph.theme
        inputStateModel: glyph.inputStateModel
        selectedDevice: glyph.selectedDevice
        selectedControl: glyph.selectedControl
        x: glyph.bw * 0.24 - glyph.stickDotSize / 2
            + inputState.axisX * glyph.stickDotSize * 0.4
        y: glyph.bh * 0.34 - glyph.stickDotSize / 2
            + inputState.axisY * glyph.stickDotSize * 0.4
        width: glyph.stickDotSize; height: glyph.stickDotSize
        radius: glyph.stickDotSize / 2
        label: "LS"
        controlId: "left_stick_button"
        stateControlId: "left_stick"
        active: inputState.pressed || lsButtonState.pressed
            || Math.abs(inputState.axisX) > 0.15
            || Math.abs(inputState.axisY) > 0.15
        onSelected: function(cid) { glyph.controlSelected(cid) }
    }

    // ── 右摇杆方向指示器 ──

    Repeater {
        model: [
            {dirState: rsUpState,    dx: 0,   dy: -1},
            {dirState: rsDownState,  dx: 0,   dy: 1},
            {dirState: rsLeftState,  dx: -1,  dy: 0},
            {dirState: rsRightState, dx: 1,   dy: 0}
        ]

        Rectangle {
            required property var modelData
            readonly property real cx: glyph.bw * 0.60
            readonly property real cy: glyph.bh * 0.68
            readonly property real offset: glyph.stickDotSize * 0.65
            x: cx - 6 + modelData.dx * offset
            y: cy - 6 + modelData.dy * offset
            width: 12; height: 12; radius: 6
            color: modelData.dirState.pressed
                ? glyph.theme.accent : "transparent"
            border.color: modelData.dirState.pressed
                ? glyph.theme.accent : glyph.theme.border
            border.width: 1.5
            opacity: modelData.dirState.pressed ? 1.0 : 0.5
        }
    }

    // ── 右摇杆（带轴偏移）──

    ControlDot {
        id: rightStickDot
        theme: glyph.theme
        inputStateModel: glyph.inputStateModel
        selectedDevice: glyph.selectedDevice
        selectedControl: glyph.selectedControl
        x: glyph.bw * 0.60 - glyph.stickDotSize / 2
            + inputState.axisX * glyph.stickDotSize * 0.4
        y: glyph.bh * 0.68 - glyph.stickDotSize / 2
            + inputState.axisY * glyph.stickDotSize * 0.4
        width: glyph.stickDotSize; height: glyph.stickDotSize
        radius: glyph.stickDotSize / 2
        label: "RS"
        controlId: "right_stick_button"
        stateControlId: "right_stick"
        active: inputState.pressed || rsButtonState.pressed
            || Math.abs(inputState.axisX) > 0.15
            || Math.abs(inputState.axisY) > 0.15
        onSelected: function(cid) { glyph.controlSelected(cid) }
    }
}
