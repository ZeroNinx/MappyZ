import QtQuick
import QtQuick.Controls

// General 设置页：本轮只有 Start minimized 一项。设置修改后立即持久化，
// checked 始终反映 settingsManager.startMinimized，不在页面中维护重复缓存。
Item {
    id: page

    required property var theme
    required property var settingsManager

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 20

        Text {
            text: "General"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        // Start minimized 设置行：左侧标题 + 说明，右侧权威开关。
        Row {
            width: parent.width
            spacing: 16

            Column {
                width: parent.width - startMinimizedCheck.width - parent.spacing
                spacing: 4

                Text {
                    text: "Start minimized"
                    color: page.theme.text
                    font.pixelSize: 14
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Start MappyZ in the system tray. Takes effect on next launch."
                    color: page.theme.muted
                    font.pixelSize: 12
                }
            }

            CheckBox {
                id: startMinimizedCheck

                objectName: "startMinimizedCheckBox"
                anchors.verticalCenter: parent.verticalCenter

                // checkState 单向绑定权威来源；nextCheckState 拦截点击回写权威值，
                // 避免 checked binding 与点击切换形成反馈循环。
                checkState: (page.settingsManager && page.settingsManager.startMinimized)
                    ? Qt.Checked : Qt.Unchecked

                nextCheckState: function() {
                    if (!page.settingsManager) {
                        return Qt.Unchecked
                    }
                    // 用户点击时只调用一次 setter。
                    var desired = !page.settingsManager.startMinimized
                    var ok = page.settingsManager.setStartMinimized(desired)
                    if (!ok) {
                        // 保存失败：显示页面内错误反馈；返回管理器中的旧值使开关回滚。
                        errorMessage.show(
                            "Failed to save setting. Please try again.",
                            page.theme.danger)
                    }
                    // 成功时管理器值已翻转，返回新值；失败时值不变，返回旧值（视觉回滚）。
                    return page.settingsManager.startMinimized ? Qt.Checked : Qt.Unchecked
                }
            }
        }

        // 保存失败时的内联错误反馈；仅用于设置相关失败，不污染 profile 消息。
        InlineMessage {
            id: errorMessage

            theme: page.theme
        }
    }
}
