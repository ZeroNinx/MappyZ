import QtQuick

// 底部状态栏：显示设备数、运行时状态、mapping 状态、输出状态和事件计数
Rectangle {
    id: statusBar

    required property var theme
    required property var appController
    required property int deviceCount

    height: 28
    color: theme.accentHover

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: statusBar.appController
            ? ("Devices: " + statusBar.deviceCount
                + "    Runtime: " + statusBar.appController.runtimeState
                + "    Mapping: " + (statusBar.appController.mappingEnabled ? "enabled" : "paused")
                + "    Output: " + statusBar.appController.outputDisplayText
                + "    Profile: " + (statusBar.appController.profileSaveState === "clean" ? "saved"
                    : statusBar.appController.profileSaveState === "error" ? "save error"
                    : "unsaved")
                + "    Events: " + statusBar.appController.lastDrainedEventCount)
            : "Devices: 0    Runtime: unknown"
        color: "#ffffff"
        font.pixelSize: 11
    }
}
