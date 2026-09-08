import QtQuick
import QtQuick.Controls

// 添加自动切换规则对话框：页面内 modal overlay。
// 输入应用可执行名 + 选择目标 profile，可从运行中的应用选择器回填进程名。
// 提交经 appController.addAutomaticProfileRule；进程名规范化、去重由 C++ 负责。
Rectangle {
    id: dialog

    required property var theme
    required property var appController
    property var foregroundService: null

    // 关闭时把键盘焦点交还给上层（Settings 对话框），保证 Escape 逐层关闭。
    property var focusOnClose: null

    property bool _bOpen: false

    function open() {
        processField.text = ""
        errorMessage.text = ""
        _selectDefaultProfile()
        _bOpen = true
        dialog.forceActiveFocus()
    }

    // 目标 profile 默认选中当前激活 profile；若其不在列表中则回退到 Default。
    function _selectDefaultProfile() {
        profileCombo.currentIndex = 0
        if (!appController) {
            return
        }
        var entries = appController.profileEntries
        if (!entries || entries.length === 0) {
            return
        }
        var activeId = appController.activeProfileId
        var defaultRow = -1
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].id === activeId) {
                profileCombo.currentIndex = i
                return
            }
            if (entries[i].id === "default") {
                defaultRow = i
            }
        }
        // 激活 profile 不在列表（异常）时退回 Default。
        profileCombo.currentIndex = defaultRow >= 0 ? defaultRow : 0
    }

    function close() {
        _bOpen = false
        // 焦点交还 Settings 对话框，使随后的 Escape 关闭 Settings 而非本层。
        if (focusOnClose) {
            focusOnClose.forceActiveFocus()
        }
    }

    // 提交当前输入。成功关闭；失败显示内联错误并保留输入。
    function submit() {
        if (!appController) {
            return
        }
        var entries = appController.profileEntries
        if (!entries || profileCombo.currentIndex < 0
            || profileCombo.currentIndex >= entries.length) {
            errorMessage.show("Please select a profile.", dialog.theme.danger)
            return
        }
        var profileId = entries[profileCombo.currentIndex].id
        var ok = appController.addAutomaticProfileRule(processField.text, profileId)
        if (ok) {
            dialog.close()
        } else {
            errorMessage.show(
                "Could not add rule. The application name may be empty or already used.",
                dialog.theme.danger)
        }
    }

    visible: _bOpen
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.6)
    z: 300

    focus: _bOpen
    Keys.onEscapePressed: function(event) {
        event.accepted = true
        dialog.close()
    }

    // 点击遮罩关闭。
    MouseArea {
        anchors.fill: parent
        onClicked: dialog.close()
    }

    Rectangle {
        id: panel

        objectName: "addRulePanel"
        anchors.centerIn: parent
        width: Math.min(parent.width - 60, 460)
        height: contentColumn.implicitHeight + 40
        radius: 8
        color: dialog.theme.panel
        border.color: dialog.theme.border
        border.width: 1

        // 阻止点击穿透到遮罩。
        MouseArea {
            anchors.fill: parent
            onClicked: function(event) { event.accepted = true }
        }

        Column {
            id: contentColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 14

            Text {
                text: "Add rule"
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            // 应用可执行名 + 从运行应用选择。
            Column {
                width: parent.width
                spacing: 6

                Text {
                    text: "Application executable"
                    color: dialog.theme.text
                    font.pixelSize: 12
                }

                Row {
                    width: parent.width
                    spacing: 8

                    TextField {
                        id: processField

                        objectName: "addRuleProcessField"
                        width: parent.width - pickButton.width - parent.spacing
                        placeholderText: "e.g. eldenring.exe"
                        color: dialog.theme.text
                        font.pixelSize: 13
                        onAccepted: dialog.submit()

                        background: Rectangle {
                            color: dialog.theme.surface
                            border.color: dialog.theme.border
                            border.width: 1
                            radius: 3
                        }
                    }

                    ActionButton {
                        id: pickButton

                        objectName: "addRulePickButton"
                        theme: dialog.theme
                        label: "Browse"
                        anchors.verticalCenter: parent.verticalCenter
                        // 无前台服务时禁用（离屏测试场景）。
                        enabled: dialog.foregroundService !== null
                        onClicked: processPicker.open()
                    }
                }
            }

            // 目标 profile 选择。
            Column {
                width: parent.width
                spacing: 6

                Text {
                    text: "Switch to profile"
                    color: dialog.theme.text
                    font.pixelSize: 12
                }

                ComboBox {
                    id: profileCombo

                    objectName: "addRuleProfileCombo"
                    width: parent.width
                    model: dialog.appController ? dialog.appController.profileEntries : []
                    textRole: "name"
                    font.pixelSize: 13
                }
            }

            InlineMessage {
                id: errorMessage

                theme: dialog.theme
            }

            // 操作按钮。
            Row {
                anchors.right: parent.right
                spacing: 8

                ActionButton {
                    objectName: "addRuleCancelButton"
                    theme: dialog.theme
                    label: "Cancel"
                    onClicked: dialog.close()
                }

                ActionButton {
                    objectName: "addRuleConfirmButton"
                    theme: dialog.theme
                    label: "Add"
                    primary: true
                    // 进程名为空时禁用（去除首尾空白后判断）。
                    enabled: processField.text.trim().length > 0
                    onClicked: dialog.submit()
                }
            }
        }
    }

    // ── 运行应用选择器 ──
    ProcessPickerDialog {
        id: processPicker

        objectName: "processPickerDialog"
        theme: dialog.theme
        foregroundService: dialog.foregroundService
        // 关闭时把焦点交还本 Add 对话框，实现 Escape 逐层关闭。
        focusOnClose: dialog
        onPicked: function(processName) {
            processField.text = processName
        }
    }
}
