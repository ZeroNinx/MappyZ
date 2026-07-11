import QtQuick

// Inspector 面板：显示选中控件详情、Capture、Choose、Clear
Panel {
    id: inspector

    required property var appController
    required property string selectedDevice
    required property string selectedControl

    heading: "Inspector"

    // 外部注入的映射选择器弹窗引用（由 Main.qml 传入）
    property var mappingPickerDialog: null

    // action 选择状态
    property string _selectedActionKind: "Keyboard"
    property string _selectedActionValue: "Space"
    property string _selectedActionDisplayText: "Keyboard: Space"

    // model reset 时递增，强制绑定查询刷新
    property int _mappingRevision: 0

    Connections {
        target: inspector.appController
            ? inspector.appController.mappingRuleModel : null
        function onModelReset() {
            inspector._mappingRevision++
            inspector._syncActionFromCurrentBinding()
        }
    }

    // 切换选中控件时同步 Action output
    onSelectedControlChanged: _syncActionFromCurrentBinding()
    Component.onCompleted: _syncActionFromCurrentBinding()

    function _syncActionFromCurrentBinding() {
        if (!appController || !appController.mappingRuleModel
            || selectedControl === "") return
        var kind = appController.mappingRuleModel.actionKindForInput(selectedControl)
        var value = appController.mappingRuleModel.actionValueForInput(selectedControl)
        if (kind !== "" && value !== "") {
            setPendingAction(kind, value)
        } else {
            _selectedActionKind = ""
            _selectedActionValue = ""
            _selectedActionDisplayText = ""
        }
    }

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

    // 供外部显示 apply 反馈
    function applyFeedbackMessage(message, tone) {
        applyFeedback.show(message, tone)
    }

    readonly property bool captureMode: appController
        ? appController.inputCapture.active : false

    readonly property bool _runtimeAllowsApply: {
        if (!appController) return false
        var state = appController.runtimeState
        return state === "ready" || state === "running"
    }

    readonly property bool _canCapture: appController
        && selectedDevice !== ""

    readonly property bool _canChoose: selectedControl !== ""

    // 当前 control 是否已有绑定（包含 _mappingRevision 触发刷新）
    readonly property string _currentRuleId: {
        void(inspector._mappingRevision)
        if (!appController || !appController.mappingRuleModel
            || selectedControl === "") return ""
        return appController.mappingRuleModel.ruleIdForInput(selectedControl)
    }

    readonly property bool _hasBinding: _currentRuleId !== ""

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
                theme: inspector.theme
                text: "Selected control"
            }

            Rectangle {
                width: parent.width
                height: 44
                radius: 4
                color: "#1f1f1f"
                border.color: inspector.captureMode
                    ? inspector.theme.warning : inspector.theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: inspector.captureMode
                        ? inspector.appController.inputCapture.displayText
                        : (inspector.selectedControl !== ""
                            ? inspector.selectedControl : "No selection")
                    color: inspector.captureMode
                        ? inspector.theme.warning
                        : (inspector.selectedControl !== ""
                            ? "#ffffff" : inspector.theme.muted)
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            FieldLabel {
                theme: inspector.theme
                text: "Action output"
            }

            Rectangle {
                width: parent.width
                height: 44
                radius: 4
                color: "#1f1f1f"
                border.color: inspector.theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: inspector._selectedActionDisplayText !== ""
                        ? inspector._selectedActionDisplayText : "No action selected"
                    color: inspector._selectedActionDisplayText !== ""
                        ? inspector.theme.text : inspector.theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: inspector.theme
                    label: "Capture Input"
                    primary: inspector.captureMode
                    enabled: inspector._canCapture || inspector.captureMode
                    onClicked: {
                        if (!inspector.appController) return
                        if (inspector.captureMode) {
                            inspector.appController.inputCapture.cancel()
                        } else {
                            inspector.appController.inputCapture.begin(
                                inspector.selectedDevice)
                        }
                    }
                }

                ActionButton {
                    theme: inspector.theme
                    label: "Clear"
                    enabled: inspector._hasBinding
                    onClicked: {
                        if (!inspector.appController) return
                        var ruleId = inspector._currentRuleId
                        if (ruleId === "") return
                        var success = inspector.appController.removeBinding(ruleId)
                        if (success) {
                            var saved = inspector.appController.profileSaveSeverity === "normal"
                            applyFeedback.show(
                                saved ? "Cleared and saved" : "Cleared, save failed",
                                saved ? inspector.theme.muted : inspector.theme.warning)
                        } else {
                            applyFeedback.show("Clear failed", inspector.theme.warning)
                        }
                    }
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: inspector.theme
                    label: "Choose..."
                    enabled: inspector._canChoose
                    onClicked: {
                        mappingPickerDialog.openFor(
                            inspector.selectedControl,
                            inspector._selectedActionKind,
                            inspector._selectedActionValue)
                    }
                }
            }

            // 禁用原因提示
            Text {
                visible: inspector._disableHint !== ""
                    && applyFeedback.text === ""
                width: parent.width
                text: inspector._disableHint
                color: inspector.theme.muted
                font.pixelSize: 11
            }

            // 操作反馈
            InlineMessage {
                id: applyFeedback

                theme: inspector.theme
            }

            // 分割线
            Rectangle {
                width: parent.width
                height: 1
                color: inspector.theme.border
            }

            Text {
                width: parent.width
                text: "Click a row to select. Double-click to edit binding."
                color: inspector.theme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
