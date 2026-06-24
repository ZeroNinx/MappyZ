import QtQuick

// 绑定编辑面板：控件选择、Capture、Action 输出和 mapping 列表
Panel {
    id: bindingEditor

    required property var appController
    required property string selectedDevice
    required property string selectedControl
    required property string selectedAction

    heading: "Binding Editor"

    // Clear 按钮触发，父级应清空 selectedControl
    signal clearControlRequested()

    // Keyboard / Mouse 按钮触发，父级应更新 selectedAction
    signal selectedActionChangedByUi(string actionText)

    readonly property bool captureMode: appController
        ? appController.inputCapture.active : false

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
                    text: bindingEditor.selectedAction
                    color: bindingEditor.theme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Keyboard"
                    onClicked: bindingEditor.selectedActionChangedByUi("Keyboard: Space")
                }

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Mouse"
                    onClicked: bindingEditor.selectedActionChangedByUi("Mouse: Left Click")
                }

                ActionButton {
                    theme: bindingEditor.theme
                    label: "Apply"
                    primary: true
                    onClicked: {
                        if (!bindingEditor.appController) return
                        bindingEditor.appController.applySelectedBinding(
                            bindingEditor.selectedControl,
                            bindingEditor.selectedAction)
                    }
                }
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
                model: bindingEditor.appController
                    ? bindingEditor.appController.mappingRuleModel : null

                Rectangle {
                    required property string input
                    required property string output
                    required property string actionKind

                    width: contentColumn.width
                    height: 40
                    radius: 4
                    color: "#1f1f1f"
                    border.color: bindingEditor.theme.border

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
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        label: actionKind
                        tone: actionKind === "Mouse" ? "#c4710c" : bindingEditor.theme.accentSoft
                    }
                }
            }
        }
    }
}
