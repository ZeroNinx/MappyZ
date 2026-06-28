import QtQuick

// 键盘映射选择页：按 US 标准布局展示键盘，支持点击和物理按键高亮
Item {
    id: keyboardPicker

    required property var theme
    required property var appController

    property string pendingValue: ""

    signal keySelected(string value)

    // 检查 catalog 中是否存在该键
    function _isSupported(value) {
        if (!appController || !appController.actionCatalogModel) return false
        return appController.actionCatalogModel.findIndex("Keyboard", value) >= 0
    }

    // 物理键盘事件处理，返回是否匹配到支持的键
    function handleQtKey(key, modifiers) {
        var value = ""
        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
            value = String.fromCharCode(key)
        } else if (key >= Qt.Key_0 && key <= Qt.Key_9) {
            value = String.fromCharCode(key)
        } else if (key === Qt.Key_Space) {
            value = "Space"
        } else if (key === Qt.Key_Tab) {
            value = "Tab"
        } else if (key === Qt.Key_Up) {
            value = "ArrowUp"
        } else if (key === Qt.Key_Down) {
            value = "ArrowDown"
        } else if (key === Qt.Key_Left) {
            value = "ArrowLeft"
        } else if (key === Qt.Key_Right) {
            value = "ArrowRight"
        }

        if (value !== "" && _isSupported(value)) {
            keySelected(value)
            return true
        }
        return false
    }

    implicitHeight: keyboardLayout.implicitHeight

    // 单位尺寸，所有 key 宽度基于此倍数
    readonly property real _u: 48
    readonly property real _gap: 4
    readonly property real _sectionGap: 16

    Column {
        id: keyboardLayout

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: keyboardPicker._gap

        // ── Function Row：Esc | F1-F4 | F5-F8 | F9-F12 | PrtSc ScrLk Pause ──
        Row {
            spacing: keyboardPicker._gap

            PickerKey {
                theme: keyboardPicker.theme; label: "Esc"
                kind: "Keyboard"; value: "Escape"
                enabled: keyboardPicker._isSupported("Escape")
                selected: keyboardPicker.pendingValue === "Escape"
                onClicked: keyboardPicker.keySelected("Escape")
            }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            Repeater {
                model: ["F1", "F2", "F3", "F4"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            Item { width: keyboardPicker._sectionGap * 0.5; height: 1 }

            Repeater {
                model: ["F5", "F6", "F7", "F8"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            Item { width: keyboardPicker._sectionGap * 0.5; height: 1 }

            Repeater {
                model: ["F9", "F10", "F11", "F12"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            Repeater {
                model: ["PrtSc", "ScrLk", "Pause"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: false
                    selected: false
                }
            }
        }

        Item { width: 1; height: 6 }

        // ── 数字行 + 导航区 + 数字小键盘 ──
        Row {
            spacing: keyboardPicker._gap

            // ` 1 2 3 4 5 6 7 8 9 0 - = Backspace
            PickerKey { theme: keyboardPicker.theme; label: "` ~"; enabled: false }
            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme
                    label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }
            PickerKey { theme: keyboardPicker.theme; label: "- _"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "= +"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Backspace"; keyWidth: 1.5; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            PickerKey { theme: keyboardPicker.theme; label: "Insert"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Home"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "PgUp"; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            PickerKey { theme: keyboardPicker.theme; label: "Num\nLock"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "/"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "*"; enabled: false }
        }

        // ── Tab 行 ──
        Row {
            spacing: keyboardPicker._gap

            PickerKey {
                theme: keyboardPicker.theme; label: "Tab"; keyWidth: 1.3
                kind: "Keyboard"; value: "Tab"
                enabled: keyboardPicker._isSupported("Tab")
                selected: keyboardPicker.pendingValue === "Tab"
                onClicked: keyboardPicker.keySelected("Tab")
            }

            Repeater {
                model: ["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            PickerKey { theme: keyboardPicker.theme; label: "[ {"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "] }"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "\\ |"; keyWidth: 1.2; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            PickerKey { theme: keyboardPicker.theme; label: "Delete"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "End"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "PgDn"; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            Repeater {
                model: ["7", "8", "9"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }
        }

        // ── Caps Lock 行 ──
        Row {
            spacing: keyboardPicker._gap

            PickerKey { theme: keyboardPicker.theme; label: "● Caps Lock"; keyWidth: 1.6; enabled: false }

            Repeater {
                model: ["A", "S", "D", "F", "G", "H", "J", "K", "L"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            PickerKey { theme: keyboardPicker.theme; label: "; :"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "' \""; enabled: false }
            PickerKey {
                theme: keyboardPicker.theme; label: "Enter"; keyWidth: 1.8
                kind: "Keyboard"; value: "Enter"
                enabled: keyboardPicker._isSupported("Enter")
                selected: keyboardPicker.pendingValue === "Enter"
                onClicked: keyboardPicker.keySelected("Enter")
            }

            // 导航区占位（本行无导航键）
            Item { width: keyboardPicker._sectionGap + 3 * 48 + 2 * keyboardPicker._gap; height: 1 }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            Repeater {
                model: ["4", "5", "6"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }
        }

        // ── Shift 行 ──
        Row {
            spacing: keyboardPicker._gap

            PickerKey { theme: keyboardPicker.theme; label: "Shift"; keyWidth: 2.0; enabled: false }

            Repeater {
                model: ["Z", "X", "C", "V", "B", "N", "M"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }

            PickerKey { theme: keyboardPicker.theme; label: ", <"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: ". >"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "/ ?"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Shift"; keyWidth: 2.0; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            // 导航区本行只有上箭头（居中）
            Item { width: 48; height: 1 }
            PickerKey {
                theme: keyboardPicker.theme; label: "▲"
                kind: "Keyboard"; value: "ArrowUp"
                enabled: keyboardPicker._isSupported("ArrowUp")
                selected: keyboardPicker.pendingValue === "ArrowUp"
                onClicked: keyboardPicker.keySelected("ArrowUp")
            }
            Item { width: 48; height: 1 }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            Repeater {
                model: ["1", "2", "3"]
                PickerKey {
                    required property string modelData
                    theme: keyboardPicker.theme; label: modelData
                    kind: "Keyboard"; value: modelData
                    enabled: keyboardPicker._isSupported(modelData)
                    selected: keyboardPicker.pendingValue === modelData
                    onClicked: keyboardPicker.keySelected(modelData)
                }
            }
        }

        // ── 底部行：Ctrl Win Alt Space Alt Fn Ctrl | ◄ ▼ ► | 0 . ──
        Row {
            spacing: keyboardPicker._gap

            PickerKey { theme: keyboardPicker.theme; label: "Ctrl"; keyWidth: 1.2; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Win"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Alt"; enabled: false }

            PickerKey {
                theme: keyboardPicker.theme; label: "Space"; keyWidth: 5.2
                kind: "Keyboard"; value: "Space"
                enabled: keyboardPicker._isSupported("Space")
                selected: keyboardPicker.pendingValue === "Space"
                onClicked: keyboardPicker.keySelected("Space")
            }

            PickerKey { theme: keyboardPicker.theme; label: "Alt"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Fn"; enabled: false }
            PickerKey { theme: keyboardPicker.theme; label: "Ctrl"; enabled: false }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            PickerKey {
                theme: keyboardPicker.theme; label: "◄"
                kind: "Keyboard"; value: "ArrowLeft"
                enabled: keyboardPicker._isSupported("ArrowLeft")
                selected: keyboardPicker.pendingValue === "ArrowLeft"
                onClicked: keyboardPicker.keySelected("ArrowLeft")
            }
            PickerKey {
                theme: keyboardPicker.theme; label: "▼"
                kind: "Keyboard"; value: "ArrowDown"
                enabled: keyboardPicker._isSupported("ArrowDown")
                selected: keyboardPicker.pendingValue === "ArrowDown"
                onClicked: keyboardPicker.keySelected("ArrowDown")
            }
            PickerKey {
                theme: keyboardPicker.theme; label: "►"
                kind: "Keyboard"; value: "ArrowRight"
                enabled: keyboardPicker._isSupported("ArrowRight")
                selected: keyboardPicker.pendingValue === "ArrowRight"
                onClicked: keyboardPicker.keySelected("ArrowRight")
            }

            Item { width: keyboardPicker._sectionGap; height: 1 }

            PickerKey {
                theme: keyboardPicker.theme; label: "0"; keyWidth: 2.0
                kind: "Keyboard"; value: "0"
                enabled: keyboardPicker._isSupported("0")
                selected: keyboardPicker.pendingValue === "0"
                onClicked: keyboardPicker.keySelected("0")
            }
            PickerKey { theme: keyboardPicker.theme; label: "."; enabled: false }
        }
    }
}
