import QtQuick

// 映射选择器弹窗：modal overlay，包含键盘/鼠标/DInput 三页选择
// 由 BindingEditor 的 Choose 按钮打开，选择完成后发出 accepted 信号
Rectangle {
    id: pickerDialog

    required property var theme
    required property var appController

    // 当前正在为哪个控件选择映射
    property string selectedControl: ""

    // 初始 kind / value（打开时回填高亮）
    property string initialKind: "Keyboard"
    property string initialValue: "Space"

    // ── 内部状态 ──

    property string pendingKind: ""
    property string pendingValue: ""
    property string pendingDisplayText: ""
    property int currentTab: 0
    property bool _bDialogOpen: false

    // ── 信号 ──

    signal accepted(string kind, string value)
    signal cancelled()

    // ── 外观 ──

    visible: _bDialogOpen
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.6)
    z: 100

    // 打开弹窗：初始化状态、根据当前 pending action 自动选页
    function openFor(controlId, kind, value) {
        selectedControl = controlId
        initialKind = kind
        initialValue = value

        pendingKind = kind
        pendingValue = value
        _refreshDisplayText()

        // 根据当前 action 类型自动选择页签
        if (kind === "MouseButton" || kind === "MouseMove") {
            currentTab = 1
        } else {
            currentTab = 0
        }

        dialogErrorMessage.text = ""
        _bDialogOpen = true
        pickerDialog.forceActiveFocus()
    }

    function close() {
        _bDialogOpen = false
    }

    // 根据 pendingKind + pendingValue 刷新 displayText
    function _refreshDisplayText() {
        if (pendingKind === "" || pendingValue === "") {
            pendingDisplayText = ""
            return
        }
        if (!appController || !appController.actionCatalogModel) {
            pendingDisplayText = pendingKind + ": " + pendingValue
            return
        }
        var idx = appController.actionCatalogModel.findIndex(pendingKind, pendingValue)
        if (idx >= 0) {
            pendingDisplayText = appController.actionCatalogModel.displayTextAt(idx)
        } else {
            pendingDisplayText = pendingKind + ": " + pendingValue
        }
    }

    // 设置 pending action（由子页 tile 点击触发）
    function _setPending(kind, value) {
        pendingKind = kind
        pendingValue = value
        _refreshDisplayText()
    }

    // 显示 dialog 内部错误提示（供外部 accepted handler 在 apply 失败时调用）
    function showError(message, tone) {
        dialogErrorMessage.show(message, tone)
    }

    // 确定按钮是否可用
    readonly property bool _bCanConfirm: {
        if (pendingKind === "" || pendingValue === "") return false
        // DInput 页不可确认
        if (currentTab === 2) return false
        // MouseMove 本轮不可确认（backout policy）
        if (pendingKind === "MouseMove") return false
        return true
    }

    // ── 物理键盘事件处理 ──

    focus: _bDialogOpen
    Keys.onPressed: function(event) {
        if (!_bDialogOpen) return

        if (event.key === Qt.Key_Escape) {
            event.accepted = true
            pickerDialog.cancelled()
            pickerDialog.close()
            return
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            event.accepted = true
            if (_bCanConfirm) {
                pickerDialog.accepted(pendingKind, pendingValue)
            }
            return
        }

        // 键盘页时，尝试匹配物理键
        if (currentTab === 0 && keyboardPage.handleQtKey(event.key, event.modifiers)) {
            event.accepted = true
        }
    }

    // 点击遮罩背景关闭弹窗
    MouseArea {
        anchors.fill: parent
        onClicked: {
            pickerDialog.cancelled()
            pickerDialog.close()
        }
    }

    // ── 弹窗内容面板 ──
    Rectangle {
        id: dialogPanel

        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 1020)
        height: Math.min(parent.height - 40, 600)
        radius: 8
        color: pickerDialog.theme.panel
        border.color: pickerDialog.theme.border
        border.width: 1

        // 阻止点击穿透到遮罩
        MouseArea {
            anchors.fill: parent
            onClicked: function(event) { event.accepted = true }
        }

        Column {
            id: dialogContent

            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // ── 标题行 ──
            Text {
                text: "选择映射目标"
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            // ── 上下文提示 ──
            Text {
                text: pickerDialog.selectedControl !== ""
                    ? "为 '" + pickerDialog.selectedControl + "' 选择映射目标"
                    : "选择映射目标"
                color: pickerDialog.theme.muted
                font.pixelSize: 12
            }

            // ── 页签栏 ──
            PickerTabs {
                id: pickerTabs

                theme: pickerDialog.theme
                currentIndex: pickerDialog.currentTab
                onTabClicked: function(index) {
                    pickerDialog.currentTab = index
                }
            }

            // ── 当前 pending action 显示 ──
            Row {
                spacing: 8

                Text {
                    text: "当前选择："
                    color: pickerDialog.theme.muted
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: pendingText.implicitWidth + 16
                    height: 26
                    radius: 3
                    color: pickerDialog.pendingDisplayText !== ""
                        ? Qt.rgba(pickerDialog.theme.accent.r,
                            pickerDialog.theme.accent.g,
                            pickerDialog.theme.accent.b, 0.15)
                        : "transparent"
                    border.color: pickerDialog.pendingDisplayText !== ""
                        ? pickerDialog.theme.accent : pickerDialog.theme.border
                    border.width: 1

                    Text {
                        id: pendingText

                        anchors.centerIn: parent
                        text: pickerDialog.pendingDisplayText !== ""
                            ? pickerDialog.pendingDisplayText : "未选择"
                        color: pickerDialog.pendingDisplayText !== ""
                            ? pickerDialog.theme.accent : pickerDialog.theme.muted
                        font.pixelSize: 12
                    }
                }

                // MouseMove 不可用提示
                Text {
                    visible: pickerDialog.pendingKind === "MouseMove"
                    text: "(不支持：MouseMove 将在未来版本中由独立模块提供)"
                    color: pickerDialog.theme.warning
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // ── 页面内容区（可滚动）──
            Flickable {
                width: parent.width
                height: parent.height - y + dialogContent.anchors.margins
                    - bottomRow.height - dialogErrorMessage.height - 16
                clip: true
                contentWidth: width
                contentHeight: pageContainer.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                Item {
                    id: pageContainer

                    width: parent.width
                    implicitHeight: {
                        if (pickerDialog.currentTab === 0) return keyboardPage.implicitHeight
                        if (pickerDialog.currentTab === 1) return mousePage.implicitHeight
                        return dinputPage.implicitHeight
                    }

                    KeyboardPicker {
                        id: keyboardPage

                        visible: pickerDialog.currentTab === 0
                        theme: pickerDialog.theme
                        appController: pickerDialog.appController
                        pendingValue: pickerDialog.pendingKind === "Keyboard"
                            ? pickerDialog.pendingValue : ""
                        anchors.left: parent.left
                        anchors.right: parent.right

                        onKeySelected: function(value) {
                            pickerDialog._setPending("Keyboard", value)
                        }
                    }

                    MousePicker {
                        id: mousePage

                        visible: pickerDialog.currentTab === 1
                        theme: pickerDialog.theme
                        pendingKind: pickerDialog.pendingKind
                        pendingValue: pickerDialog.pendingValue
                        anchors.left: parent.left
                        anchors.right: parent.right

                        onMouseActionSelected: function(kind, value) {
                            pickerDialog._setPending(kind, value)
                        }
                    }

                    DInputPicker {
                        id: dinputPage

                        visible: pickerDialog.currentTab === 2
                        theme: pickerDialog.theme
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            // ── Dialog 内错误提示 ──
            InlineMessage {
                id: dialogErrorMessage

                theme: pickerDialog.theme
            }

            // ── 底部操作栏 ──
            Row {
                id: bottomRow

                anchors.right: parent.right
                spacing: 8

                // 不支持键提示
                Text {
                    visible: pickerDialog.currentTab === 0
                    text: "提示：按下物理键盘按键可快速选择"
                    color: pickerDialog.theme.muted
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item { width: 16; height: 1 }

                ActionButton {
                    theme: pickerDialog.theme
                    label: "取消"
                    onClicked: {
                        pickerDialog.cancelled()
                        pickerDialog.close()
                    }
                }

                ActionButton {
                    theme: pickerDialog.theme
                    label: "确定"
                    primary: true
                    enabled: pickerDialog._bCanConfirm
                    onClicked: {
                        pickerDialog.accepted(pickerDialog.pendingKind, pickerDialog.pendingValue)
                    }
                }
            }
        }
    }
}
