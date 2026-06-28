import QtQuick

// 输入控件状态代理：监听语义 signal，维护本地显式状态属性。
// 通过 required property 接收 inputStateModel，不直接引用 appController。
QtObject {
    required property var inputStateModel
    required property string deviceId
    required property string controlId

    property bool pressed: false
    property double value: 0.0
    property double axisX: 0.0
    property double axisY: 0.0
    property string displayValue: ""

    function refresh() {
        if (!inputStateModel || deviceId === "" || controlId === "") {
            reset()
            return
        }
        pressed = inputStateModel.isPressed(deviceId, controlId)
        value = inputStateModel.value(deviceId, controlId)
        axisX = inputStateModel.axisX(deviceId, controlId)
        axisY = inputStateModel.axisY(deviceId, controlId)
        displayValue = inputStateModel.displayValue(deviceId, controlId)
    }

    function reset() {
        pressed = false
        value = 0.0
        axisX = 0.0
        axisY = 0.0
        displayValue = ""
    }

    onDeviceIdChanged: refresh()
    onControlIdChanged: refresh()
    onInputStateModelChanged: refresh()

    property Connections _inputConn: Connections {
        target: inputStateModel || null

        function onControlStateChanged(sigDeviceId, sigControlId) {
            if (sigDeviceId === deviceId && sigControlId === controlId) {
                refresh()
            }
        }

        function onDeviceStateRemoved(sigDeviceId) {
            if (sigDeviceId === deviceId) {
                reset()
            }
        }

        function onInputStateReset() {
            reset()
        }
    }
}
