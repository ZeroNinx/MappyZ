import QtQuick

// 顶部工具栏：产品名、运行时副标题、Profile 标签、Mapping 开关、Save 按钮
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
                ? topBar.appController.activeProfileName : "Default"
            tone: "#3c3c3c"
        }

        ActionButton {
            theme: topBar.theme
            label: topBar.appController
                ? (topBar.appController.mappingEnabled ? "Mapping On" : "Mapping Off")
                : "Mapping Off"
            primary: topBar.appController ? topBar.appController.mappingEnabled : false
            onClicked: {
                if (topBar.appController)
                    topBar.appController.mappingEnabled = !topBar.appController.mappingEnabled
            }
        }

        ActionButton {
            id: realOutputButton

            theme: topBar.theme
            label: _confirmPending ? "Confirm Real Output" : (
                topBar.appController && topBar.appController.realOutputEnabled
                    ? "Real Output On" : "Real Output")
            primary: topBar.appController
                ? topBar.appController.realOutputEnabled : false
            enabled: topBar.appController
                ? !topBar.appController.outputModeSwitching : false

            property bool _confirmPending: false

            Timer {
                id: confirmTimer
                interval: 3000
                onTriggered: realOutputButton._confirmPending = false
            }

            onClicked: {
                if (!topBar.appController) return

                // 关闭真实输出不需要确认
                if (topBar.appController.realOutputEnabled) {
                    topBar.appController.setRealOutputEnabled(false)
                    return
                }

                // 两步确认：第一次点击进入待确认状态
                if (!_confirmPending) {
                    _confirmPending = true
                    confirmTimer.restart()
                    return
                }

                // 第二次点击：确认开启
                _confirmPending = false
                confirmTimer.stop()
                topBar.appController.setRealOutputEnabled(true)
            }
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
