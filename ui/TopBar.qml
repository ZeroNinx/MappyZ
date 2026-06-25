import QtQuick

// 顶部工具栏：产品名、运行时副标题、Profile 标签和保存入口
Rectangle {
    id: topBar

    required property var theme
    required property var appController

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

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Tag {
            theme: topBar.theme
            label: topBar.appController
                ? ("Profile: " + topBar.appController.activeProfileName) : "Profile: Default"
            tone: "#3c3c3c"
        }

        ActionButton {
            theme: topBar.theme
            label: "Save Profile"
            onClicked: {
                if (topBar.appController)
                    topBar.appController.saveActiveProfile()
            }
        }
    }
}
