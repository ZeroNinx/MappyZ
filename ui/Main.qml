import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 1120
    height: 720
    minimumWidth: 1040
    minimumHeight: 700
    visible: true
    title: "MappyZ"
    color: theme.window

    Theme { id: theme }

    // context property 别名：避免子组件 required property 同名遮蔽
    readonly property var _appController: appController

    // ── 应用级状态 ──

    property string selectedDevice: ""
    property string selectedDeviceDisplayName: ""
    property string selectedControl: "button_south"
    property string selectedAction: "Keyboard: Space"
    property string latestControlForSelectedDevice: ""

    // ── 设备生命周期 signal 驱动设备选择 ──

    Connections {
        target: appController.deviceModel

        function onDeviceAdded(deviceId) {
            if (root.selectedDevice === "") {
                var row = root._findDeviceRow(deviceId)
                if (row >= 0) {
                    root.selectedDevice = deviceId
                    root.selectedDeviceDisplayName = appController.deviceModel.displayNameAt(row)
                }
            }
        }

        function onDeviceUpdated(deviceId) {
            if (deviceId === root.selectedDevice) {
                root.selectedDeviceDisplayName = appController.deviceModel.displayNameAt(
                    _findDeviceRow(deviceId))
            }
        }

        function onDeviceRemoved(deviceId) {
            if (deviceId !== root.selectedDevice) return
            if (appController.deviceModel.rowCount() > 0) {
                root.selectedDevice = appController.deviceModel.deviceIdAt(0)
                root.selectedDeviceDisplayName = appController.deviceModel.displayNameAt(0)
            } else {
                root.selectedDevice = ""
                root.selectedDeviceDisplayName = ""
            }
        }

        function onDeviceModelReset() {
            if (appController.deviceModel.rowCount() > 0) {
                root.selectedDevice = appController.deviceModel.deviceIdAt(0)
                root.selectedDeviceDisplayName = appController.deviceModel.displayNameAt(0)
            } else {
                root.selectedDevice = ""
                root.selectedDeviceDisplayName = ""
            }
        }
    }

    // ── 输入状态 signal 驱动 latestControl 更新 ──

    Connections {
        target: appController.inputStateModel

        function onControlStateChanged(deviceId, controlId) {
            if (deviceId === root.selectedDevice) {
                root.latestControlForSelectedDevice = controlId
            }
        }

        function onDeviceStateRemoved(deviceId) {
            if (deviceId === root.selectedDevice) {
                root.latestControlForSelectedDevice = ""
            }
        }

        function onInputStateReset() {
            root.latestControlForSelectedDevice = ""
        }
    }

    // ── capture 完成 signal 驱动 selectedControl 更新 ──

    Connections {
        target: appController.inputCapture

        function onCaptureCompleted(deviceId, controlId) {
            if (deviceId === root.selectedDevice) {
                root.selectedControl = controlId
            }
        }
    }

    // ── 设备切换时从快照初始化 latestControl ──

    onSelectedDeviceChanged: {
        if (selectedDevice !== "") {
            latestControlForSelectedDevice = appController.inputStateModel.latestControlId(selectedDevice)
        } else {
            latestControlForSelectedDevice = ""
        }
    }

    function _findDeviceRow(deviceId) {
        var model = appController.deviceModel
        for (var i = 0; i < model.rowCount(); i++) {
            if (model.deviceIdAt(i) === deviceId) return i
        }
        return -1
    }

    // ── 生命周期 ──

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

    // ── 布局编排 ──

    TopBar {
        id: topBar

        theme: theme
        appController: root._appController
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    DevicesPanel {
        id: devicePanel

        theme: theme
        appController: root._appController
        selectedDevice: root.selectedDevice
        selectedDeviceDisplayName: root.selectedDeviceDisplayName
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.top: topBar.bottom
        anchors.topMargin: 12
        anchors.bottom: eventPanel.top
        anchors.bottomMargin: 12
        width: 260

        onDeviceSelected: function(deviceId, displayName) {
            root.selectedDevice = deviceId
            root.selectedDeviceDisplayName = displayName
        }
    }

    GamepadView {
        id: gamepadPanel

        theme: theme
        appController: root._appController
        selectedDevice: root.selectedDevice
        selectedDeviceDisplayName: root.selectedDeviceDisplayName
        selectedControl: root.selectedControl
        latestControlForSelectedDevice: root.latestControlForSelectedDevice
        anchors.left: devicePanel.right
        anchors.leftMargin: 12
        anchors.right: bindingPanel.left
        anchors.rightMargin: 12
        anchors.top: devicePanel.top
        anchors.bottom: devicePanel.bottom

        onControlSelected: function(controlId) {
            root.selectedControl = controlId
            appController.inputCapture.cancel()
        }

        onActionButtonControlSelected: function(controlId) {
            root.selectedControl = controlId
        }
    }

    BindingEditor {
        id: bindingPanel

        theme: theme
        appController: root._appController
        selectedDevice: root.selectedDevice
        selectedControl: root.selectedControl
        selectedAction: root.selectedAction
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: devicePanel.top
        anchors.bottom: devicePanel.bottom
        width: 300

        onClearControlRequested: root.selectedControl = ""
        onSelectedActionChangedByUi: function(actionText) {
            root.selectedAction = actionText
        }
    }

    EventLogPanel {
        id: eventPanel

        theme: theme
        appController: root._appController
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: statusBar.top
        anchors.bottomMargin: 12
        height: 158
    }

    StatusBar {
        id: statusBar

        theme: theme
        appController: root._appController
        deviceCount: devicePanel.deviceCount
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
