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
        // 自适应高度：根据内容计算，clamp 在 400~parent-40 之间
        height: Math.min(parent.height - 40, Math.max(400,
            headerSection.height + 12
            + tabsSection.height + 10
            + selectionSection.height + 10
            + pageContainer.implicitHeight + 8
            + footerSection.height + 40 + 20))
        radius: 8
        color: pickerDialog.theme.panel
        border.color: pickerDialog.theme.border
        border.width: 1

        // 阻止点击穿透到遮罩
        MouseArea {
            anchors.fill: parent
            onClicked: function(event) { event.accepted = true }
        }

        // ── Section 1: Header（标题 + 上下文提示）──
        Column {
            id: headerSection

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 4

            Text {
                text: "选择映射目标"
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            Text {
                text: pickerDialog.selectedControl !== ""
                    ? "为 '" + pickerDialog.selectedControl + "' 选择映射目标"
                    : "选择映射目标"
                color: pickerDialog.theme.muted
                font.pixelSize: 12
            }
        }

        // ── Section 2: Tabs ──
        PickerTabs {
            id: tabsSection

            theme: pickerDialog.theme
            currentIndex: pickerDialog.currentTab
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.top: headerSection.bottom
            anchors.topMargin: 12

            onTabClicked: function(index) {
                pickerDialog.currentTab = index
            }
        }

        // ── Section 3: Selection Summary（当前选择 pill）──
        Row {
            id: selectionSection

            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: tabsSection.bottom
            anchors.topMargin: 10
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

        // ── Section 4: Content（页面内容区，填充中间剩余空间）──
        Flickable {
            id: contentSection

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: selectionSection.bottom
            anchors.bottom: footerSection.top
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 10
            anchors.bottomMargin: 8
            clip: true
            contentWidth: width
            contentHeight: pageContainer.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Item {
                id: pageContainer

                width: parent.width
                // 内容不足时垂直居中，超出时顶部对齐
                y: implicitHeight < contentSection.height
                    ? Math.round((contentSection.height - implicitHeight) / 2)
                    : 0
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

        // ── Section 5: Footer（错误消息 + 操作按钮，锚定底部）──
        Column {
            id: footerSection

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 20
            spacing: 8

            InlineMessage {
                id: dialogErrorMessage

                theme: pickerDialog.theme
            }

            Row {
                id: bottomRow

                anchors.right: parent.right
                spacing: 8

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
