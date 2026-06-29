import QtQuick

// 底部状态栏：显示设备数、运行时状态、输出状态和保存状态
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
                + " | Runtime: " + statusBar.appController.runtimeDisplayText
                + " | Output: " + statusBar.appController.outputDisplayText
                + " | Profile: " + statusBar.appController.profileSaveDisplayText)
            : "Devices: 0 | Runtime: Unknown"
        color: "#ffffff"
        font.pixelSize: 11
    }
}
