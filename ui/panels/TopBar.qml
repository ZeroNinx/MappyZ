import QtQuick

// 顶部工具栏：产品名、运行时副标题、Profile 选择器
Rectangle {
    id: topBar

    required property var theme
    required property var appController

    // 转发 ProfileSelector 的设置入口信号，自身不创建设置页或持有设置状态。
    signal settingsRequested()

    height: 54
    color: theme.panelHeader

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: topBar.theme.border
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
        color: topBar.theme.muted
        font.pixelSize: 12
    }

    ProfileSelector {
        id: profileSelector

        objectName: "profileSelector"
        theme: topBar.theme
        appController: topBar.appController
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter

        onSettingsRequested: topBar.settingsRequested()
    }
}
