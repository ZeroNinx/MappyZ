import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 1120
    height: 720
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: "MappyZ"
    color: theme.window

    QtObject {
        id: theme

        readonly property color window: "#1e1e1e"
        readonly property color panel: "#252526"
        readonly property color panelHeader: "#2d2d30"
        readonly property color surface: "#2a2d2e"
        readonly property color border: "#3f3f46"
        readonly property color text: "#cccccc"
        readonly property color muted: "#808080"
        readonly property color accent: "#007acc"
        readonly property color accentSoft: "#0e639c"
        readonly property color accentHover: "#094771"
        readonly property color success: "#73c991"
        readonly property color warning: "#f48771"
        readonly property color danger: "#f44747"
    }

    property bool captureMode: false
    property string selectedDevice: ""
    property string selectedDeviceDisplayName: ""
    property string selectedControl: "button_south"
    property string selectedAction: "Keyboard: Space"

    Component.onCompleted: {
        var ok = appController.initializeRuntime(true)
        if (ok) {
            ok = appController.startRuntime()
        }
        if (ok) {
            appController.startPumpTimer(16)
        }
    }

    onClosing: {
        appController.stopPumpTimer()
        appController.stopRuntime()
    }

    ListModel {
        id: mappingModel

        ListElement { input: "A / South"; output: "Space"; actionKind: "Keyboard" }
        ListElement { input: "RT"; output: "Left Click"; actionKind: "Mouse" }
        ListElement { input: "Right Stick"; output: "Mouse Move"; actionKind: "Axis" }
        ListElement { input: "Start"; output: "Enter"; actionKind: "Keyboard" }
    }

    ListModel {
        id: eventModel

        ListElement { time: "00:12.840"; level: "Info"; message: "Profile snapshot applied: Default FPS" }
        ListElement { time: "00:13.112"; level: "Input"; message: "button_south pressed, value=1.0" }
        ListElement { time: "00:13.114"; level: "Map"; message: "button_south -> Keyboard Space" }
        ListElement { time: "00:14.021"; level: "Output"; message: "SendInput keyboard action completed" }
    }

    component Panel: Rectangle {
        id: panel

        property string heading: ""
        property alias content: contentHost.data

        color: theme.panel
        radius: 4
        border.width: 0
        clip: true

        Rectangle {
            id: titleBar

            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            height: 33
            color: theme.panelHeader

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 11
                anchors.verticalCenter: parent.verticalCenter
                text: panel.heading
                color: theme.text
                font.pixelSize: 12
                font.bold: true
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: theme.border
            }
        }

        Item {
            id: contentHost

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 12
        }

        Rectangle {
            anchors.fill: parent
            radius: panel.radius
            color: "transparent"
            border.color: theme.border
            border.width: 1
            z: 100
        }
    }

    component ActionButton: Rectangle {
        id: button

        property string label: ""
        property bool primary: false
        signal clicked()

        width: labelText.implicitWidth + 28
        height: 30
        radius: 3
        color: mouseArea.containsMouse ? theme.accentHover : (primary ? theme.accentSoft : theme.surface)
        border.color: primary ? theme.accent : theme.border
        border.width: 1

        Text {
            id: labelText

            anchors.centerIn: parent
            text: button.label
            color: "#ffffff"
            font.pixelSize: 12
        }

        MouseArea {
            id: mouseArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }

    component Tag: Rectangle {
        property string label: ""
        property color tone: theme.accentSoft

        width: labelText.implicitWidth + 16
        height: 22
        radius: 11
        color: tone

        Text {
            id: labelText

            anchors.centerIn: parent
            text: parent.label
            color: "#ffffff"
            font.pixelSize: 11
        }
    }

    component FieldLabel: Text {
        color: theme.muted
        font.pixelSize: 11
    }

    component ValueText: Text {
        color: theme.text
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    component ControlDot: Rectangle {
        id: controlDot

        property string controlId: ""
        property string label: ""

        width: 38
        height: 38
        radius: 19
        color: root.selectedControl === controlId ? theme.accentSoft : theme.surface
        border.color: root.selectedControl === controlId ? theme.accent : theme.border
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: controlDot.label
            color: root.selectedControl === controlDot.controlId ? "#ffffff" : theme.text
            font.pixelSize: 12
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.selectedControl = controlDot.controlId
                root.captureMode = false
            }
        }
    }

    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 54
        color: theme.panelHeader

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.border
        }

        Text {
            id: productName

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: "MappyZ"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            anchors.left: productName.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "Gamepad remapping runtime"
            color: theme.muted
            font.pixelSize: 12
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Tag {
                label: "Default FPS"
                tone: "#3c3c3c"
            }

            ActionButton {
                label: appController.mappingEnabled ? "Mapping On" : "Mapping Off"
                primary: appController.mappingEnabled
                onClicked: appController.mappingEnabled = !appController.mappingEnabled
            }

            ActionButton {
                label: "Save Profile"
                onClicked: eventModel.insert(0, {
                    "time": "now",
                    "level": "Profile",
                    "message": "Profile save requested"
                })
            }
        }
    }

    Panel {
        id: devicePanel

        heading: "Devices"
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.top: topBar.bottom
        anchors.topMargin: 12
        anchors.bottom: eventPanel.top
        anchors.bottomMargin: 12
        width: 260

        content: [
            Column {
                anchors.fill: parent
                spacing: 10

                Repeater {
                    id: deviceRepeater
                    model: appController.deviceModel

                    onCountChanged: {
                        if (root.selectedDevice === "") return
                        for (var i = 0; i < count; i++) {
                            if (appController.deviceModel.deviceIdAt(i) === root.selectedDevice) return
                        }
                        root.selectedDevice = ""
                        root.selectedDeviceDisplayName = ""
                    }

                    Rectangle {
                        width: parent.width
                        height: 110
                        radius: 4
                        color: root.selectedDevice === deviceId ? "#2d2d2d" : "#1f1f1f"
                        border.color: root.selectedDevice === deviceId ? theme.accent : theme.border
                        border.width: 1

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.right: stateTag.left
                            anchors.rightMargin: 8
                            anchors.top: parent.top
                            anchors.topMargin: 10
                            text: displayName
                            color: theme.text
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Tag {
                            id: stateTag

                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            label: appController.runtimeState
                            tone: appController.runtimeState === "running" ? theme.success : "#555555"
                        }

                        FieldLabel {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 36
                            text: "Backend"
                        }

                        ValueText {
                            anchors.left: parent.left
                            anchors.leftMargin: 74
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 36
                            text: backend
                        }

                        FieldLabel {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 58
                            text: "ID"
                        }

                        ValueText {
                            anchors.left: parent.left
                            anchors.leftMargin: 74
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 58
                            text: vendorId + ":" + productId
                        }

                        FieldLabel {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 80
                            text: "Profile"
                        }

                        ValueText {
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
                            onClicked: {
                                root.selectedDevice = deviceId
                                root.selectedDeviceDisplayName = displayName
                            }
                        }
                    }
                }

                Text {
                    visible: deviceRepeater.count === 0
                    text: "No gamepads connected"
                    color: theme.muted
                    font.pixelSize: 12
                    topPadding: 8
                }

                Rectangle {
                    width: parent.width
                    height: 78
                    radius: 4
                    color: "#1f1f1f"
                    border.color: theme.border

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "Runtime"
                        color: theme.text
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 34
                        text: appController.runtimeMessage || "No runtime message"
                        color: theme.muted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
        ]
    }

    Panel {
        id: gamepadPanel

        heading: "Gamepad View"
        anchors.left: devicePanel.right
        anchors.leftMargin: 12
        anchors.right: bindingPanel.left
        anchors.rightMargin: 12
        anchors.top: devicePanel.top
        anchors.bottom: devicePanel.bottom

        content: [
            Item {
                anchors.fill: parent

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: "#1f1f1f"
                    border.color: theme.border
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    text: root.selectedDeviceDisplayName || "No device selected"
                    color: "#ffffff"
                    font.pixelSize: 15
                    font.bold: true
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 38
                    text: "Selected input: " + root.selectedControl
                    color: theme.muted
                    font.pixelSize: 12
                }

                Rectangle {
                    id: controllerBody

                    anchors.centerIn: parent
                    width: Math.min(parent.width - 80, 520)
                    height: 260
                    radius: 74
                    color: theme.surface
                    border.color: theme.border
                    border.width: 1
                }

                Rectangle {
                    anchors.centerIn: controllerBody
                    width: controllerBody.width * 0.46
                    height: 114
                    radius: 24
                    color: "#1a1a1a"
                    border.color: theme.border
                }

                ControlDot {
                    anchors.left: controllerBody.left
                    anchors.leftMargin: 92
                    anchors.top: controllerBody.top
                    anchors.topMargin: 74
                    label: "LS"
                    controlId: "left_stick"
                }

                ControlDot {
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 132
                    anchors.top: controllerBody.top
                    anchors.topMargin: 148
                    label: "RS"
                    controlId: "right_stick"
                }

                Grid {
                    anchors.left: controllerBody.left
                    anchors.leftMargin: 78
                    anchors.bottom: controllerBody.bottom
                    anchors.bottomMargin: 42
                    columns: 3
                    rows: 3
                    spacing: 4

                    Item { width: 30; height: 30 }
                    ControlDot { width: 30; height: 30; radius: 4; label: "U"; controlId: "dpad_up" }
                    Item { width: 30; height: 30 }
                    ControlDot { width: 30; height: 30; radius: 4; label: "L"; controlId: "dpad_left" }
                    Rectangle { width: 30; height: 30; radius: 4; color: "#333333"; border.color: theme.border }
                    ControlDot { width: 30; height: 30; radius: 4; label: "R"; controlId: "dpad_right" }
                    Item { width: 30; height: 30 }
                    ControlDot { width: 30; height: 30; radius: 4; label: "D"; controlId: "dpad_down" }
                    Item { width: 30; height: 30 }
                }

                ControlDot {
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 82
                    anchors.top: controllerBody.top
                    anchors.topMargin: 68
                    label: "Y"
                    controlId: "button_north"
                }

                ControlDot {
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 42
                    anchors.top: controllerBody.top
                    anchors.topMargin: 108
                    label: "B"
                    controlId: "button_east"
                }

                ControlDot {
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 122
                    anchors.top: controllerBody.top
                    anchors.topMargin: 108
                    label: "X"
                    controlId: "button_west"
                }

                ControlDot {
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 82
                    anchors.top: controllerBody.top
                    anchors.topMargin: 148
                    label: "A"
                    controlId: "button_south"
                }

                Row {
                    anchors.horizontalCenter: controllerBody.horizontalCenter
                    anchors.top: controllerBody.top
                    anchors.topMargin: 92
                    spacing: 10

                    ActionButton {
                        label: "Back"
                        onClicked: root.selectedControl = "button_back"
                    }

                    ActionButton {
                        label: "Start"
                        onClicked: root.selectedControl = "button_start"
                    }
                }

                Row {
                    anchors.left: controllerBody.left
                    anchors.leftMargin: 96
                    anchors.right: controllerBody.right
                    anchors.rightMargin: 96
                    anchors.bottom: controllerBody.top
                    anchors.bottomMargin: 14
                    spacing: 12

                    Rectangle {
                        width: (parent.width - 12) / 2
                        height: 28
                        radius: 4
                        color: root.selectedControl === "left_trigger" ? theme.accentHover : "#333333"
                        border.color: theme.border

                        Text {
                            anchors.centerIn: parent
                            text: "LT 0.18"
                            color: theme.text
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedControl = "left_trigger"
                        }
                    }

                    Rectangle {
                        width: (parent.width - 12) / 2
                        height: 28
                        radius: 4
                        color: root.selectedControl === "right_trigger" ? theme.accentHover : "#333333"
                        border.color: theme.border

                        Text {
                            anchors.centerIn: parent
                            text: "RT 0.74"
                            color: theme.text
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedControl = "right_trigger"
                        }
                    }
                }
            }
        ]
    }

    Panel {
        id: bindingPanel

        heading: "Binding Editor"
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: devicePanel.top
        anchors.bottom: devicePanel.bottom
        width: 300

        content: [
            Column {
                anchors.fill: parent
                spacing: 12

                FieldLabel { text: "Selected control" }

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 4
                    color: "#1f1f1f"
                    border.color: root.captureMode ? theme.warning : theme.border

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.captureMode ? "Waiting for input..." : root.selectedControl
                        color: root.captureMode ? theme.warning : "#ffffff"
                        font.pixelSize: 13
                        font.bold: true
                    }
                }

                Row {
                    spacing: 8

                    ActionButton {
                        label: "Capture Input"
                        primary: root.captureMode
                        onClicked: root.captureMode = !root.captureMode
                    }

                    ActionButton {
                        label: "Clear"
                        onClicked: root.selectedControl = ""
                    }
                }

                FieldLabel { text: "Action output" }

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 4
                    color: "#1f1f1f"
                    border.color: theme.border

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.selectedAction
                        color: theme.text
                        font.pixelSize: 13
                    }
                }

                Row {
                    spacing: 8

                    ActionButton {
                        label: "Keyboard"
                        onClicked: root.selectedAction = "Keyboard: Space"
                    }

                    ActionButton {
                        label: "Mouse"
                        onClicked: root.selectedAction = "Mouse: Left Click"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: theme.border
                }

                Text {
                    text: "Current mappings"
                    color: theme.text
                    font.pixelSize: 12
                    font.bold: true
                }

                Repeater {
                    model: mappingModel

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 4
                        color: "#1f1f1f"
                        border.color: theme.border

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: input
                            color: theme.text
                            font.pixelSize: 12
                        }

                        Text {
                            anchors.right: typeTag.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: output
                            color: theme.muted
                            font.pixelSize: 11
                        }

                        Tag {
                            id: typeTag

                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            label: actionKind
                            tone: actionKind === "Mouse" ? "#c4710c" : theme.accentSoft
                        }
                    }
                }
            }
        ]
    }

    Panel {
        id: eventPanel

        heading: "Event Log"
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: statusBar.top
        anchors.bottomMargin: 12
        height: 158

        content: [
            Column {
                anchors.fill: parent
                spacing: 6

                Repeater {
                    model: eventModel

                    Rectangle {
                        width: parent.width
                        height: 24
                        color: index % 2 === 0 ? "#1f1f1f" : theme.panel

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 70
                            text: time
                            color: theme.muted
                            font.pixelSize: 11
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 86
                            anchors.verticalCenter: parent.verticalCenter
                            width: 58
                            text: level
                            color: level === "Output" ? theme.success : (level === "Map" ? theme.accent : theme.text)
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 152
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: message
                            color: theme.text
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        ]
    }

    Rectangle {
        id: statusBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 28
        color: theme.accentHover

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "Devices: " + deviceRepeater.count + "    Runtime: " + appController.runtimeState + "    Mapping: " + (appController.mappingEnabled ? "enabled" : "paused") + "    Output: " + appController.outputState + "    Events: " + appController.lastDrainedEventCount
            color: "#ffffff"
            font.pixelSize: 11
        }
    }
}
