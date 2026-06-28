import QtQuick

// 顶部工具栏：产品名、运行时副标题、Remap 开关、Profile 标签和保存入口
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
        id: topBarActions

        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        ActionButton {
            theme: topBar.theme
            label: topBar.appController && topBar.appController.mappingEnabled
                ? "Remap Active" : "Remap Paused"
            primary: topBar.appController ? topBar.appController.mappingEnabled : false
            onClicked: {
                if (!topBar.appController) return
                topBar.appController.mappingEnabled = !topBar.appController.mappingEnabled
                remapFeedback.show(
                    topBar.appController.mappingEnabled
                        ? "Mapped output dispatch enabled"
                        : "Mapped output dispatch paused",
                    topBar.appController.mappingEnabled
                        ? topBar.theme.success : topBar.theme.muted)
            }
        }

        Tag {
            theme: topBar.theme
            label: "Profile: " + (topBar.appController
                ? topBar.appController.profileDisplayText : "Default")
            tone: {
                if (!topBar.appController) return "#3c3c3c"
                var severity = topBar.appController.profileSaveSeverity
                if (severity === "danger") return topBar.theme.warning
                if (severity === "caution") return topBar.theme.accentSoft
                return "#3c3c3c"
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

    // Remap 切换操作反馈
    InlineMessage {
        id: remapFeedback

        theme: topBar.theme
        width: topBarActions.width
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.top: topBarActions.bottom
        anchors.topMargin: 4
    }
}
