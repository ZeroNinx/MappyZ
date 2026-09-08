import QtQuick

// 应用设置对话框：应用内 modal overlay（非第二个原生 Window），
// 左侧分类导航 + 右侧内容的桌面式双栏结构。本轮只有 General 分类。
// 设置项修改后立即持久化，无 Apply / Save / Cancel。
Rectangle {
    id: settingsDialog

    required property var theme
    required property var settingsManager
    required property var appController

    // 前台服务可为 null（离屏测试未注入）；进程选择器对 null 做降级。
    property var foregroundService: null

    property bool _bOpen: false

    // 当前选中的分类稳定 key（"general" / "automatic"），不依赖易变的 row 数字。
    property string currentCategory: "general"

    // 打开：默认选中 General，抢焦点以接收 Escape。
    function open() {
        currentCategory = "general"
        _bOpen = true
        settingsDialog.forceActiveFocus()
    }

    function close() {
        _bOpen = false
    }

    visible: _bOpen
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.6)
    z: 200

    // Escape 关闭对话框，不修改设置。
    focus: _bOpen
    Keys.onEscapePressed: function(event) {
        event.accepted = true
        settingsDialog.close()
    }

    // 点击遮罩关闭。
    MouseArea {
        anchors.fill: parent
        onClicked: settingsDialog.close()
    }

    Rectangle {
        id: panel

        objectName: "settingsPanel"
        anchors.centerIn: parent
        // 最小窗口尺寸下随窗口收缩，但保持双栏可用。
        width: Math.min(parent.width - 80, 760)
        height: Math.min(parent.height - 80, 520)
        radius: 8
        color: settingsDialog.theme.panel
        border.color: settingsDialog.theme.border
        border.width: 1

        // 阻止点击穿透到遮罩。
        MouseArea {
            anchors.fill: parent
            onClicked: function(event) { event.accepted = true }
        }

        // ── 标题栏：标题 + 唯一关闭入口 ──
        Item {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 56

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: "Settings"
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            ActionButton {
                objectName: "settingsCloseButton"
                theme: settingsDialog.theme
                label: "Close"
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                onClicked: settingsDialog.close()
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: settingsDialog.theme.border
            }
        }

        // ── 左侧分类导航（固定宽度 220）──
        Rectangle {
            id: nav

            anchors.left: parent.left
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            width: 220
            color: settingsDialog.theme.panelHeader
            radius: 0

            // 与右侧内容的清晰分隔线。
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: settingsDialog.theme.border
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 4

                // General 分类条目：选中高亮 / hover / 键盘焦点三态。
                Rectangle {
                    id: generalNavItem

                    objectName: "settingsNavGeneral"
                    width: parent.width
                    height: 56
                    radius: 4
                    activeFocusOnTab: true
                    color: settingsDialog.currentCategory === "general"
                        ? settingsDialog.theme.accentHover
                        : (generalNavMouse.containsMouse
                            ? settingsDialog.theme.surface
                            : "transparent")
                    border.width: generalNavItem.activeFocus ? 1 : 0
                    border.color: settingsDialog.theme.accent

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: "General"
                        color: settingsDialog.theme.text
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: generalNavMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsDialog.currentCategory = "general"
                    }

                    Keys.onReturnPressed: settingsDialog.currentCategory = "general"
                    Keys.onEnterPressed: settingsDialog.currentCategory = "general"
                }

                // Automatic Profile Switching 分类条目：三态与 General 一致。
                Rectangle {
                    id: automaticNavItem

                    objectName: "settingsNavAutomatic"
                    width: parent.width
                    height: 56
                    radius: 4
                    activeFocusOnTab: true
                    color: settingsDialog.currentCategory === "automatic"
                        ? settingsDialog.theme.accentHover
                        : (automaticNavMouse.containsMouse
                            ? settingsDialog.theme.surface
                            : "transparent")
                    border.width: automaticNavItem.activeFocus ? 1 : 0
                    border.color: settingsDialog.theme.accent

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Automatic Profile Switching"
                        color: settingsDialog.theme.text
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: automaticNavMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsDialog.currentCategory = "automatic"
                    }

                    Keys.onReturnPressed: settingsDialog.currentCategory = "automatic"
                    Keys.onEnterPressed: settingsDialog.currentCategory = "automatic"
                }
            }
        }

        // ── 右侧内容区 ──
        GeneralSettingsPage {
            objectName: "settingsGeneralPage"
            visible: settingsDialog.currentCategory === "general"
            theme: settingsDialog.theme
            settingsManager: settingsDialog.settingsManager
            anchors.left: nav.right
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
        }

        AutomaticProfileSettingsPage {
            objectName: "settingsAutomaticPage"
            visible: settingsDialog.currentCategory === "automatic"
            theme: settingsDialog.theme
            appController: settingsDialog.appController
            foregroundService: settingsDialog.foregroundService
            // 二级对话框关闭后焦点回到本 Settings 对话框，Escape 才能继续逐层关闭。
            overlayFocusTarget: settingsDialog
            anchors.left: nav.right
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
        }
    }
}
