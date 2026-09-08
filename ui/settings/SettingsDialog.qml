import QtQuick

// 应用设置对话框：应用内 modal overlay（非第二个原生 Window），
// 左侧分类导航 + 右侧内容的桌面式双栏结构。本轮只有 General 分类。
// 设置项修改后立即持久化，无 Apply / Save / Cancel。
Rectangle {
    id: settingsDialog

    required property var theme
    required property var settingsManager

    property bool _bOpen: false

    // 当前选中的分类索引（本轮只有 General = 0）。
    property int currentCategory: 0

    // 打开：默认选中 General，抢焦点以接收 Escape。
    function open() {
        currentCategory = 0
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
                    color: settingsDialog.currentCategory === 0
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
                        onClicked: settingsDialog.currentCategory = 0
                    }

                    Keys.onReturnPressed: settingsDialog.currentCategory = 0
                    Keys.onEnterPressed: settingsDialog.currentCategory = 0
                }
            }
        }

        // ── 右侧内容区 ──
        GeneralSettingsPage {
            objectName: "settingsGeneralPage"
            visible: settingsDialog.currentCategory === 0
            theme: settingsDialog.theme
            settingsManager: settingsDialog.settingsManager
            anchors.left: nav.right
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
        }
    }
}
