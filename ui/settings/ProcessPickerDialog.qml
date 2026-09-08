import QtQuick

// 运行应用选择器：页面内 modal overlay，展示当前可见顶层应用列表。
// 列表来自 foregroundService.runningApplications()（每项含 processName / displayName）。
// 单击选中一行，双击或 Confirm 回填进程名；Escape 只关闭本层。
Rectangle {
    id: picker

    required property var theme
    property var foregroundService: null

    // 关闭时把键盘焦点交还给上层（Add 对话框），保证 Escape 逐层关闭。
    property var focusOnClose: null

    property bool _bOpen: false

    // 当前选中的进程名（空表示未选中）。
    property string selectedProcessName: ""

    // 确认选择一个应用的进程名。
    signal picked(string processName)

    // 打开时刷新一次运行应用快照（枚举是即时查询，不常驻），并清除既有选择。
    function open() {
        if (foregroundService) {
            appList.model = foregroundService.runningApplications()
        } else {
            appList.model = []
        }
        selectedProcessName = ""
        _bOpen = true
        picker.forceActiveFocus()
    }

    function close() {
        _bOpen = false
        // 焦点交还上层，使随后的 Escape 关闭 Add 对话框而非再次触发本层。
        if (focusOnClose) {
            focusOnClose.forceActiveFocus()
        }
    }

    // 提交当前选中项。
    function confirm() {
        if (selectedProcessName === "") {
            return
        }
        picker.picked(selectedProcessName)
        picker.close()
    }

    visible: _bOpen
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.6)
    z: 400

    focus: _bOpen
    // Escape 只关闭本层（进程选择器），不冒泡到父级添加对话框。
    Keys.onEscapePressed: function(event) {
        event.accepted = true
        picker.close()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: picker.close()
    }

    Rectangle {
        id: panel

        objectName: "processPickerPanel"
        anchors.centerIn: parent
        width: Math.min(parent.width - 60, 420)
        height: Math.min(parent.height - 100, 460)
        radius: 8
        color: picker.theme.panel
        border.color: picker.theme.border
        border.width: 1

        MouseArea {
            anchors.fill: parent
            onClicked: function(event) { event.accepted = true }
        }

        // 标题栏。
        Item {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 48

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "Running applications"
                color: "#ffffff"
                font.pixelSize: 15
                font.bold: true
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: picker.theme.border
            }
        }

        // 底部操作栏。
        Item {
            id: footer

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 56

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: picker.theme.border
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                ActionButton {
                    objectName: "processPickerCancelButton"
                    theme: picker.theme
                    label: "Cancel"
                    onClicked: picker.close()
                }

                ActionButton {
                    objectName: "processPickerConfirmButton"
                    theme: picker.theme
                    label: "Confirm"
                    primary: true
                    // 未选中任何行时禁用。
                    enabled: picker.selectedProcessName !== ""
                    onClicked: picker.confirm()
                }
            }
        }

        // 空态。
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.margins: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            visible: appList.count === 0
            text: "No visible applications found. Enter a process name manually."
            color: picker.theme.muted
            font.pixelSize: 12
        }

        ListView {
            id: appList

            objectName: "processPickerList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.margins: 8
            clip: true
            model: []

            delegate: Rectangle {
                id: appRow

                required property var modelData

                width: appList.width
                height: 44
                radius: 4
                // 选中高亮以 processName 判定。
                color: appRow.modelData.processName === picker.selectedProcessName
                    ? picker.theme.accentHover
                    : (rowMouse.containsMouse ? picker.theme.surface : "transparent")

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        width: parent.width
                        text: appRow.modelData.displayName
                        color: picker.theme.text
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: appRow.modelData.processName
                        color: picker.theme.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // 单击选中，双击直接确认回填。
                    onClicked: picker.selectedProcessName = appRow.modelData.processName
                    onDoubleClicked: {
                        picker.selectedProcessName = appRow.modelData.processName
                        picker.confirm()
                    }
                }
            }
        }
    }
}
