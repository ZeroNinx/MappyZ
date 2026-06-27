import QtQuick
import QtQuick.Controls.Basic

// 绑定编辑面板：控件选择、Capture、Action 输出和 mapping 列表
// P6：使用 actionCatalogModel 下拉选择，结构化 action apply
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

    // action 选择状态由 BindingEditor 内部管理
    property int _selectedActionIndex: 0

    readonly property string _selectedActionKind: appController && appController.actionCatalogModel
        ? appController.actionCatalogModel.kindAt(_selectedActionIndex) : ""

    readonly property string _selectedActionValue: appController && appController.actionCatalogModel
        ? appController.actionCatalogModel.valueAt(_selectedActionIndex) : ""

    readonly property string _selectedActionDisplayText: appController && appController.actionCatalogModel
        ? appController.actionCatalogModel.displayTextAt(_selectedActionIndex) : ""

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

    // Apply 可用条件：设备 + 控件 + 动作 + runtime 就绪 + 非 capture 模式
    readonly property bool _canApply: _runtimeAllowsApply
        && selectedDevice !== ""
        && selectedControl !== ""
        && _selectedActionKind !== ""
        && !captureMode

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

            // action 选择下拉
            ComboBox {
                id: actionComboBox

                width: parent.width
                height: 44

                model: bindingEditor.appController
                    ? bindingEditor.appController.actionCatalogModel : null

                textRole: "displayText"
                currentIndex: bindingEditor._selectedActionIndex

                onActivated: function(index) {
                    bindingEditor._selectedActionIndex = index
                }

                contentItem: Text {
                    leftPadding: 10
                    rightPadding: actionComboBox.indicator.width + 10
                    text: actionComboBox.displayText
                    color: bindingEditor.theme.text
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 4
                    color: "#1f1f1f"
                    border.color: actionComboBox.pressed
                        ? bindingEditor.theme.accent : bindingEditor.theme.border
                }

                indicator: Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▼"
                    color: bindingEditor.theme.muted
                    font.pixelSize: 10
                }

                popup: Popup {
                    y: actionComboBox.height
                    width: actionComboBox.width
                    implicitHeight: Math.min(contentItem.implicitHeight + 2, 300)
                    padding: 1

                    background: Rectangle {
                        color: "#1f1f1f"
                        border.color: bindingEditor.theme.border
                        radius: 4
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: actionComboBox.popup.visible
                            ? actionComboBox.delegateModel : null
                        currentIndex: actionComboBox.highlightedIndex
                        ScrollBar.vertical: ScrollBar {}
                    }
                }

                delegate: ItemDelegate {
                    required property int index
                    required property string displayText

                    width: actionComboBox.width
                    height: 36

                    contentItem: Text {
                        text: displayText
                        color: highlighted
                            ? bindingEditor.theme.accent : bindingEditor.theme.text
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }

                    highlighted: actionComboBox.highlightedIndex === index

                    background: Rectangle {
                        color: highlighted ? "#2a2a2a" : "transparent"
                    }
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Apply"
                    primary: true
                    enabled: bindingEditor._canApply
                    onClicked: {
                        if (!bindingEditor.appController) return
                        var success = bindingEditor.appController.applySelectedBinding(
                            bindingEditor.selectedControl,
                            bindingEditor._selectedActionKind,
                            bindingEditor._selectedActionValue)
                        if (success) {
                            var saved = bindingEditor.appController.profileSaveState === "clean"
                            var state = bindingEditor.appController.runtimeState
                            if (state === "running") {
                                applyFeedback.show(
                                    saved ? "Applied and saved" : "Applied, save failed",
                                    saved ? bindingEditor.theme.success : bindingEditor.theme.warning)
                            } else {
                                applyFeedback.show(
                                    saved ? "Applied and saved — will dispatch after runtime starts"
                                          : "Applied, save failed",
                                    saved ? bindingEditor.theme.accent : bindingEditor.theme.warning)
                            }
                        } else {
                            applyFeedback.show(
                                "Apply failed",
                                bindingEditor.theme.warning)
                        }
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

                    width: contentColumn.width
                    height: 40
                    radius: 4
                    color: rowMouseArea.containsMouse ? "#262626" : "#1f1f1f"
                    border.color: bindingEditor.theme.border

                    // 点击行：回填 action picker + 通知父级选中控件
                    MouseArea {
                        id: rowMouseArea

                        anchors.fill: parent
                        anchors.rightMargin: deleteButton.width + 4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!bindingEditor.appController) return
                            var catalog = bindingEditor.appController.actionCatalogModel
                            var idx = catalog.findIndex(actionKind, actionValue)
                            if (idx >= 0) {
                                bindingEditor._selectedActionIndex = idx
                                actionComboBox.currentIndex = idx
                            }
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
                        anchors.right: deleteButton.left
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        label: displayKind
                        tone: displayKind === "Mouse" ? "#c4710c" : bindingEditor.theme.accentSoft
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
                                    var saved = bindingEditor.appController.profileSaveState === "clean"
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
        }
    }
}
