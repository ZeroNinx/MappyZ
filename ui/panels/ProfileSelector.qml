import QtQuick
import QtQuick.Controls

// 顶部配置选择器：显示当前配置名、下拉切换列表，以及 + / Rename / Delete。
// 组件只渲染 appController 暴露的状态并调用其 invokable，不保存独立配置列表。
Item {
    id: selector

    required property var theme
    required property var appController

    // 仅保留交互状态：是否处于重命名编辑态与草稿文本。
    property bool renaming: false
    property string renameDraft: ""

    // 记录进入编辑态时的配置 ID：编辑期间若当前配置被外部（托盘等）切换，
    // 用它判断草稿是否仍对应正在编辑的配置，避免把旧草稿改到新配置上。
    property string renameProfileId: ""

    // 防止 Enter 触发 editingFinished 后再次提交的组件内提交 guard。
    property bool _committing: false

    implicitWidth: layout.implicitWidth
    implicitHeight: 30
    height: 30

    // 把当前 activeProfileId 映射为 profileEntries 中的下标，供下拉定位使用。
    function _activeIndex() {
        var entries = appController ? appController.profileEntries : []
        var activeId = appController ? appController.activeProfileId : ""
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].id === activeId) return i
        }
        return -1
    }

    // 进入重命名编辑态：关闭下拉，记录当前配置 ID，填入当前权威名称，
    // 下一事件循环聚焦并全选。
    function beginRename() {
        profileDropdown.close()
        renameProfileId = appController ? appController.activeProfileId : ""
        renameDraft = appController ? appController.activeProfileName : ""
        renaming = true
        Qt.callLater(function() {
            renameInput.forceActiveFocus()
            renameInput.selectAll()
        })
    }

    // 提交重命名：成功后退出编辑态，失败保持编辑态并重新聚焦。
    function commitRename() {
        // 已退出编辑态（成功提交或取消）时，Enter/失焦/隐藏引发的二次回调直接忽略：
        // 既阻止 Enter 依次触发 accepted + editingFinished 的重复提交，
        // 也保证 Escape 取消后隐藏输入框引发的 editingFinished 不会调用 C++。
        if (_committing || !renaming) return
        // 编辑期间当前配置被外部切换：草稿已不属于正在编辑的配置，放弃本次提交。
        if (!appController || appController.activeProfileId !== renameProfileId) {
            cancelRename()
            return
        }
        _committing = true
        var ok = appController.renameActiveProfile(renameDraft)
        if (ok) {
            renaming = false
        } else {
            renameInput.forceActiveFocus()
            renameInput.selectAll()
        }
        // 同一次 Enter 会在当前调用栈内同步依次发出 accepted 与 editingFinished，
        // 因此延迟到下一事件循环再清理 guard，才能挡住第二个信号的重复提交。
        Qt.callLater(function() { selector._committing = false })
    }

    // 取消重命名：恢复当前权威名称，不调用 C++。
    function cancelRename() {
        renaming = false
        renameDraft = appController ? appController.activeProfileName : ""
    }

    // 编辑期间当前配置被外部切换（托盘点击等）：立即结束编辑，避免旧草稿
    // 在后续提交时被应用到新配置。重命名自身不改变 ID，因此不会误触发。
    Connections {
        target: selector.appController
        function onActiveProfileChanged() {
            if (selector.renaming && selector.appController
                && selector.appController.activeProfileId !== selector.renameProfileId) {
                selector.cancelRename()
            }
        }
    }

    Row {
        id: layout

        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        // ── Profile field（显示态 / 重命名编辑态复用同一尺寸）──
        Rectangle {
            id: field

            width: 220
            height: 30
            radius: 3
            color: selector.theme.surface
            border.width: 1
            // clean/dirty/error 通过 profileSaveSeverity 映射到边框色。
            border.color: {
                if (!selector.appController) return selector.theme.border
                var severity = selector.appController.profileSaveSeverity
                if (severity === "danger") return selector.theme.warning
                if (severity === "caution") return selector.theme.accentSoft
                return selector.theme.border
            }

            // 显示态文本
            Text {
                id: fieldLabel

                visible: !selector.renaming
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: fieldArrow.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: "Profile: " + (selector.appController
                    ? selector.appController.profileDisplayText : "Default")
                color: selector.theme.text
                font.pixelSize: 13
            }

            // 下拉箭头
            Text {
                id: fieldArrow

                visible: !selector.renaming
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"
                color: selector.theme.muted
                font.pixelSize: 12
            }

            // 重命名编辑态输入框，尺寸与显示态一致。
            TextInput {
                id: renameInput

                objectName: "profileRenameInput"
                visible: selector.renaming
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                text: selector.renameDraft
                color: selector.theme.text
                font.pixelSize: 13
                selectByMouse: true
                selectionColor: selector.theme.accentSoft

                onTextChanged: selector.renameDraft = text
                // Enter 与失焦复用同一提交入口；提交 guard 防止二次触发。
                onAccepted: selector.commitRename()
                onEditingFinished: selector.commitRename()
                Keys.onEscapePressed: selector.cancelRename()
            }

            // 显示态整块可点击打开下拉；重命名态禁止打开。
            MouseArea {
                anchors.fill: parent
                enabled: !selector.renaming
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (selector.renaming) return
                    if (profileDropdown.opened) {
                        profileDropdown.close()
                    } else {
                        profileDropdown.open()
                    }
                }
            }

            // ── 配置下拉列表 ──
            Popup {
                id: profileDropdown

                objectName: "profileDropdown"
                y: field.height + 2
                width: Math.max(field.width, 220)
                padding: 1
                // 内容高度受 280 上限约束，超出由 ListView 滚动。
                implicitHeight: Math.min(280, listView.contentHeight + 2)

                background: Rectangle {
                    color: selector.theme.panel
                    border.color: selector.theme.border
                    border.width: 1
                    radius: 3
                }

                // 每次打开定位到当前项。
                onAboutToShow: {
                    var idx = selector._activeIndex()
                    if (idx >= 0) {
                        listView.currentIndex = idx
                        listView.positionViewAtIndex(idx, ListView.Contain)
                    }
                }

                contentItem: ListView {
                    id: listView

                    objectName: "profileListView"
                    clip: true
                    model: selector.appController ? selector.appController.profileEntries : []
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        id: entry

                        required property var modelData
                        required property int index

                        width: ListView.view.width
                        height: 28
                        color: entryMouse.containsMouse
                            ? selector.theme.accentHover
                            : "transparent"

                        readonly property bool isActive:
                            selector.appController
                            && modelData.id === selector.appController.activeProfileId

                        // 勾选标记占固定宽度，非当前项保留空位以对齐文字。
                        Text {
                            id: check

                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            text: entry.isActive ? "✓" : ""
                            color: selector.theme.success
                            font.pixelSize: 12
                        }

                        Text {
                            anchors.left: check.right
                            anchors.leftMargin: 4
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            text: entry.modelData.name
                            color: selector.theme.text
                            font.pixelSize: 13
                        }

                        MouseArea {
                            id: entryMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                // 点击当前项只关闭；点击其他项切换，仅成功时关闭。
                                if (entry.isActive) {
                                    profileDropdown.close()
                                    return
                                }
                                var ok = selector.appController.switchProfile(entry.modelData.id)
                                if (ok) profileDropdown.close()
                            }
                        }
                    }
                }
            }
        }

        // ── 操作按钮 ──
        ActionButton {
            objectName: "profileCreateButton"
            theme: selector.theme
            label: "+"
            width: 30
            enabled: !selector.renaming
            onClicked: {
                profileDropdown.close()
                selector.appController.createProfile()
            }
        }

        ActionButton {
            objectName: "profileRenameButton"
            theme: selector.theme
            label: "Rename"
            width: 70
            enabled: !selector.renaming
            onClicked: selector.beginRename()
        }

        ActionButton {
            objectName: "profileDeleteButton"
            theme: selector.theme
            label: "Delete"
            width: 60
            enabled: selector.appController
                && selector.appController.canDeleteProfile
                && !selector.renaming
            onClicked: {
                profileDropdown.close()
                selector.appController.deleteActiveProfile()
            }
        }
    }
}
