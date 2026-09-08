import QtQuick
import QtQuick.Controls

// Automatic Profile Switching 设置页。
// 规则列表绑定 appController.autoProfileRuleModel（只读投影），所有增删改经
// appController 的 invokable 完成——页面不缓存规则、不直接切换 profile。
Item {
    id: page

    required property var theme
    required property var appController

    // 前台服务可为 null（离屏测试未注入）；仅进程选择器使用。
    property var foregroundService: null

    // 二级对话框关闭时把焦点交还给它（通常是 Settings 对话框），支撑 Escape 逐层关闭。
    property var overlayFocusTarget: null

    // 当前选中行的权威身份用 ruleId 表示，不长期缓存 row index。
    property string selectedRuleId: ""

    readonly property var _model:
        page.appController ? page.appController.autoProfileRuleModel : null

    // 模型整体 reset（增删改 / profile 变化）后，若选中行已不存在则清除选择。
    Connections {
        target: page._model
        function onModelReset() {
            if (page.selectedRuleId !== ""
                && page._model.rowForRuleId(page.selectedRuleId) < 0) {
                page.selectedRuleId = ""
            }
        }
    }

    Column {
        id: content

        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // ── 标题 + 说明 ──
        Text {
            text: "Automatic Profile Switching"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Switch to a profile automatically when a matching application comes to the "
                + "foreground. When no rule matches, the Default profile is restored."
            color: page.theme.muted
            font.pixelSize: 12
        }

        // ── 当前前台进程 ──
        Column {
            width: parent.width
            spacing: 2

            Text {
                text: "Current foreground application"
                color: page.theme.muted
                font.pixelSize: 11
            }

            Text {
                objectName: "automaticForegroundText"
                width: parent.width
                elide: Text.ElideRight
                text: (page.appController && page.appController.foregroundProcessName)
                    ? page.appController.foregroundProcessName
                    : "(unknown)"
                color: page.theme.text
                font.pixelSize: 14
            }
        }

        // ── 工具栏：Add / Delete ──
        Row {
            width: parent.width
            spacing: 8

            ActionButton {
                objectName: "automaticAddRuleButton"
                theme: page.theme
                label: "Add"
                primary: true
                onClicked: addRuleDialog.open()
            }

            ActionButton {
                objectName: "automaticDeleteRuleButton"
                theme: page.theme
                label: "Delete"
                // 无选中行时禁用。
                enabled: page.selectedRuleId !== ""
                onClicked: page._deleteSelected()
            }
        }

        // ── 自动切换状态消息（求值/持久化反馈）──
        Text {
            objectName: "automaticStatusText"
            width: parent.width
            wrapMode: Text.WordWrap
            visible: text !== ""
            text: (page.appController && page.appController.automaticProfileMessage)
                ? page.appController.automaticProfileMessage : ""
            color: page.theme.warning
            font.pixelSize: 12
        }

        // ── 规则表 ──
        Rectangle {
            width: parent.width
            height: page.height - content.spacing * 5 - 24 * 2 - 170
            color: page.theme.surface
            border.color: page.theme.border
            border.width: 1
            radius: 4

            // 表头（固定列：Enabled / Process / Profile / Status）。
            Row {
                id: tableHeader

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                height: 28
                spacing: 8

                // 剩余宽度在 Process / Profile / Status 间分配。
                readonly property real flexWidth:
                    (width - 60 - spacing * 3)

                Text {
                    width: 60
                    text: "Enabled"
                    color: page.theme.muted
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: tableHeader.flexWidth * 0.32
                    text: "Process"
                    color: page.theme.muted
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: tableHeader.flexWidth * 0.34
                    text: "Profile"
                    color: page.theme.muted
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: tableHeader.flexWidth * 0.34
                    text: "Status"
                    color: page.theme.muted
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: tableHeader.bottom
                anchors.topMargin: 4
                height: 1
                color: page.theme.border
            }

            // 空态提示
            Text {
                anchors.centerIn: parent
                visible: ruleList.count === 0
                text: "No automatic switching rules yet."
                color: page.theme.muted
                font.pixelSize: 12
            }

            ListView {
                id: ruleList

                objectName: "automaticRuleList"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: tableHeader.bottom
                anchors.bottom: parent.bottom
                anchors.margins: 8
                anchors.topMargin: 8
                clip: true
                model: page._model

                delegate: Rectangle {
                    id: ruleRow

                    // 用单一 model 属性访问各 role，避免 required property "enabled"
                    // 遮蔽 Item.enabled 触发 override 警告。
                    required property var model

                    width: ruleList.width
                    height: 44
                    radius: 4
                    // 选中高亮以 ruleId 判定，不依赖 row index。
                    color: ruleRow.model.ruleId === page.selectedRuleId
                        ? page.theme.accentHover
                        : (rowMouse.containsMouse ? page.theme.panelHeader : "transparent")

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        // 单击行选中。复选框与其自身交互区不受影响。
                        onClicked: page.selectedRuleId = ruleRow.model.ruleId
                    }

                    Row {
                        id: rowLayout

                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        // 与表头一致的弹性宽度基准。
                        readonly property real flexWidth:
                            (width - 60 - spacing * 3)

                        CheckBox {
                            width: 60
                            anchors.verticalCenter: parent.verticalCenter
                            checkState: ruleRow.model.enabled ? Qt.Checked : Qt.Unchecked
                            nextCheckState: function() {
                                if (!page.appController) {
                                    return ruleRow.model.enabled ? Qt.Checked : Qt.Unchecked
                                }
                                page.appController.setAutomaticProfileRuleEnabled(
                                    ruleRow.model.ruleId, !ruleRow.model.enabled)
                                // 提交后模型整体 reset，delegate 重建并从模型重新求值；
                                // 此处返回权威值仅为避免瞬时视觉抖动。
                                return ruleRow.model.enabled ? Qt.Checked : Qt.Unchecked
                            }
                        }

                        // Process 列。
                        Text {
                            width: rowLayout.flexWidth * 0.32
                            anchors.verticalCenter: parent.verticalCenter
                            text: ruleRow.model.processName
                            color: page.theme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }

                        // Profile 列：缺失时红色高亮。
                        Text {
                            width: rowLayout.flexWidth * 0.34
                            anchors.verticalCenter: parent.verticalCenter
                            text: ruleRow.model.profileName
                            color: ruleRow.model.valid ? page.theme.text : page.theme.danger
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }

                        // Status 列：仅在引用失效或有状态文本时显示（窄窗口优先压缩）。
                        Text {
                            width: rowLayout.flexWidth * 0.34
                            anchors.verticalCenter: parent.verticalCenter
                            visible: ruleRow.model.statusText !== "" && !ruleRow.model.valid
                            text: ruleRow.model.statusText
                            color: page.theme.danger
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: page.theme.border
                        opacity: 0.5
                    }
                }
            }
        }
    }

    // 删除当前选中规则，删除后选中相邻行（若还有）。
    function _deleteSelected() {
        if (!appController || selectedRuleId === "" || !_model) {
            return
        }
        var oldRow = _model.rowForRuleId(selectedRuleId)
        var ok = appController.removeAutomaticProfileRule(selectedRuleId)
        if (!ok) {
            return
        }
        var remaining = _model.rowCount()
        if (remaining === 0 || oldRow < 0) {
            selectedRuleId = ""
        } else {
            var nextRow = Math.min(oldRow, remaining - 1)
            selectedRuleId = _model.ruleIdAt(nextRow)
        }
    }

    // ── 添加规则对话框（页面内 modal overlay）──
    AddAutomaticProfileRuleDialog {
        id: addRuleDialog

        objectName: "automaticAddRuleDialog"
        theme: page.theme
        appController: page.appController
        foregroundService: page.foregroundService
        // 关闭时焦点交还 Settings 对话框。
        focusOnClose: page.overlayFocusTarget
    }
}
