import QtQuick

// 绑定编辑面板：控件选择、Capture、映射选择器和 mapping 列表
Panel {
    id: bindingEditor

    required property var appController
    required property string selectedDevice
    required property string selectedControl

    heading: "Binding Editor"

    // 点击 mapping 行时，通知父级选中该控件
    signal mappingSelected(string controlId)

    // Clear 按钮触发，父级应清空 selectedControl
    signal clearControlRequested()

    // 外部注入的映射选择器弹窗引用（由 Main.qml 传入）
    property var mappingPickerDialog: null

    // action 选择状态：不再依赖 ComboBox index，直接维护 kind/value
    property string _selectedActionKind: "Keyboard"
    property string _selectedActionValue: "Space"
    property string _selectedActionDisplayText: "Keyboard: Space"

    // 通过 catalog 查 displayText 设置 pending action
    function setPendingAction(kind, value) {
        if (!appController || !appController.actionCatalogModel) return false
        var idx = appController.actionCatalogModel.findIndex(kind, value)
        if (idx < 0) return false
        _selectedActionKind = kind
        _selectedActionValue = value
        _selectedActionDisplayText = appController.actionCatalogModel.displayTextAt(idx)
        return true
    }

    // 供外部（Main.qml 的 accepted handler）显示 apply 反馈
    function applyFeedbackMessage(message, tone) {
        applyFeedback.show(message, tone)
    }

    readonly property bool captureMode: appController
        ? appController.inputCapture.active : false

    // runtime 处于 ready 或 running 状态时允许 Apply
    readonly property bool _runtimeAllowsApply: {
        if (!appController) return false
        var state = appController.runtimeState
        return state === "ready" || state === "running"
    }

    // Capture 可用条件：已选设备
    readonly property bool _canCapture: appController
        && selectedDevice !== ""

    // Choose 可用条件：已选控件
    readonly property bool _canChoose: selectedControl !== ""

    // 当前最优先的禁用原因提示（空字符串表示无禁用）
    readonly property string _disableHint: {
        if (!appController) return ""
        if (selectedDevice === "") return "Select a device first"
        if (!_runtimeAllowsApply) return "Initialize runtime first"
        if (captureMode) return ""
        if (selectedControl === "") return "Select an input first"
        if (_selectedActionKind === "") return "Select an action first"
        return ""
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn

            width: parent.width
            spacing: 12

            FieldLabel {
                theme: bindingEditor.theme
                text: "Selected control"
            }

            Rectangle {
                width: parent.width
                height: 44
                radius: 4
                color: "#1f1f1f"
                border.color: bindingEditor.captureMode
                    ? bindingEditor.theme.warning : bindingEditor.theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: bindingEditor.captureMode
                        ? bindingEditor.appController.inputCapture.displayText
                        : bindingEditor.selectedControl
                    color: bindingEditor.captureMode
                        ? bindingEditor.theme.warning : "#ffffff"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Capture Input"
                    primary: bindingEditor.captureMode
                    enabled: bindingEditor._canCapture || bindingEditor.captureMode
                    onClicked: {
                        if (!bindingEditor.appController) return
                        if (bindingEditor.captureMode) {
                            bindingEditor.appController.inputCapture.cancel()
                        } else {
                            bindingEditor.appController.inputCapture.begin(
                                bindingEditor.selectedDevice)
                        }
                    }
                }

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Clear"
                    onClicked: bindingEditor.clearControlRequested()
                }
            }

            FieldLabel {
                theme: bindingEditor.theme
                text: "Action output"
            }

            // 只读选择框：显示当前 pending action
            Rectangle {
                width: parent.width
                height: 44
                radius: 4
                color: "#1f1f1f"
                border.color: bindingEditor.theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: bindingEditor._selectedActionDisplayText !== ""
                        ? bindingEditor._selectedActionDisplayText : "未选择"
                    color: bindingEditor._selectedActionDisplayText !== ""
                        ? bindingEditor.theme.text : bindingEditor.theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Choose..."
                    enabled: bindingEditor._canChoose
                    onClicked: {
                        mappingPickerDialog.openFor(
                            bindingEditor.selectedControl,
                            bindingEditor._selectedActionKind,
                            bindingEditor._selectedActionValue)
                    }
                }
            }

            // 禁用原因提示（持久显示，直到条件解除）
            Text {
                visible: bindingEditor._disableHint !== ""
                    && applyFeedback.text === ""
                width: parent.width
                text: bindingEditor._disableHint
                color: bindingEditor.theme.muted
                font.pixelSize: 11
            }

            // Apply 操作反馈（临时显示，3 秒后自动消失）
            InlineMessage {
                id: applyFeedback

                theme: bindingEditor.theme
            }

            Rectangle {
                width: parent.width
                height: 1
                color: bindingEditor.theme.border
            }

            Text {
                text: "Current mappings"
                color: bindingEditor.theme.text
                font.pixelSize: 12
                font.bold: true
            }

            Repeater {
                id: mappingRepeater

                model: bindingEditor.appController
                    ? bindingEditor.appController.mappingRuleModel : null

                Rectangle {
                    required property string ruleId
                    required property string input
                    required property string output
                    required property string actionKind
                    required property string actionValue
                    required property string displayKind
                    required property bool ruleEnabled

                    width: contentColumn.width
                    height: 40
                    radius: 4
                    color: rowMouseArea.containsMouse ? "#262626" : "#1f1f1f"
                    border.color: bindingEditor.theme.border
                    opacity: ruleEnabled ? 1.0 : 0.5

                    // 点击行：回填 action picker + 通知父级选中控件
                    MouseArea {
                        id: rowMouseArea

                        anchors.fill: parent
                        anchors.rightMargin: deleteButton.width + toggleButton.width + 12
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            bindingEditor.setPendingAction(actionKind, actionValue)
                            bindingEditor.mappingSelected(input)
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: outputText.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: input
                        color: bindingEditor.theme.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        id: outputText

                        anchors.right: typeTag.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: output
                        color: bindingEditor.theme.muted
                        font.pixelSize: 11
                    }

                    Tag {
                        id: typeTag

                        theme: bindingEditor.theme
                        anchors.right: toggleButton.left
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        label: displayKind
                        tone: displayKind === "Mouse" ? "#c4710c" : bindingEditor.theme.accentSoft
                    }

                    // 启用/禁用切换按钮
                    Rectangle {
                        id: toggleButton

                        anchors.right: deleteButton.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: toggleLabel.implicitWidth + 10
                        height: 24
                        radius: 4
                        color: toggleMouseArea.containsMouse
                            ? (ruleEnabled ? "#203020" : "#2a2a2a") : "transparent"

                        Text {
                            id: toggleLabel

                            anchors.centerIn: parent
                            text: ruleEnabled ? "On" : "Off"
                            color: ruleEnabled
                                ? bindingEditor.theme.success : bindingEditor.theme.muted
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: toggleMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!bindingEditor.appController) return
                                var success = bindingEditor.appController.setBindingEnabled(
                                    ruleId, !ruleEnabled)
                                if (success) {
                                    var saved = bindingEditor.appController.profileSaveSeverity === "normal"
                                    var action = !ruleEnabled ? "Enabled" : "Disabled"
                                    toggleFeedback.show(
                                        saved ? action + " and saved: " + input
                                              : action + ", save failed: " + input,
                                        saved ? bindingEditor.theme.muted : bindingEditor.theme.warning)
                                } else {
                                    toggleFeedback.show(
                                        "Toggle failed",
                                        bindingEditor.theme.warning)
                                }
                            }
                        }
                    }

                    // 删除按钮
                    Rectangle {
                        id: deleteButton

                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        radius: 4
                        color: deleteMouseArea.containsMouse ? "#3a2020" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: deleteMouseArea.containsMouse
                                ? bindingEditor.theme.warning : bindingEditor.theme.muted
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: deleteMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!bindingEditor.appController) return
                                var success = bindingEditor.appController.removeBinding(ruleId)
                                if (success) {
                                    var saved = bindingEditor.appController.profileSaveSeverity === "normal"
                                    deleteFeedback.show(
                                        saved ? "Removed and saved: " + input
                                              : "Removed, save failed: " + input,
                                        saved ? bindingEditor.theme.muted : bindingEditor.theme.warning)
                                } else {
                                    deleteFeedback.show(
                                        "Remove failed",
                                        bindingEditor.theme.warning)
                                }
                            }
                        }
                    }
                }
            }

            // mapping 列表为空时的占位提示
            Text {
                visible: mappingRepeater.count === 0
                text: "No mappings yet"
                color: bindingEditor.theme.muted
                font.pixelSize: 12
                topPadding: 4
            }

            // 删除操作反馈
            InlineMessage {
                id: deleteFeedback

                theme: bindingEditor.theme
            }

            // 启用/禁用操作反馈
            InlineMessage {
                id: toggleFeedback

                theme: bindingEditor.theme
            }
        }
    }
}
