import QtQuick

// 键盘映射选择页：按 US ANSI 标准布局展示键盘
// 采用区域化定位：主键区、导航区、方向键、小键盘各自独立对齐
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

    implicitHeight: keyboardGrid.height

    // ── 尺寸单位系统 ──
    // 总宽 = 21*_ku + 17*_kg + 2*_ks = _ku*(21 + 17/12 + 2/3) = _ku * (21+25/12)
    // 用精确比例避免 Math.round 累计误差导致溢出

    readonly property real _ku: {
        var scale = 21.0 + 25.0 / 12.0
        var available = width > 0 ? width : 980
        return Math.min(48, Math.max(28, available / scale))
    }
    readonly property real _kg: _ku / 12.0
    readonly property real _ks: _ku / 3.0
    readonly property real _kh: _ku * 5.0 / 6.0

    // ── 区域 x 坐标 ──

    readonly property real _mainWidth: 15 * _ku + 13 * _kg
    readonly property real _navX: _mainWidth + _ks
    readonly property real _navWidth: 3 * _ku + 2 * _kg
    readonly property real _numX: _navX + _navWidth + _ks
    readonly property real _totalWidth: _numX + 3 * _ku + 2 * _kg

    Item {
        id: keyboardGrid

        width: keyboardPicker._totalWidth
        anchors.horizontalCenter: parent.horizontalCenter
        height: funcRow.height + _funcBodyGap + 5 * keyboardPicker._kh + 4 * keyboardPicker._kg

        readonly property real _funcBodyGap: Math.round(keyboardPicker._ku * 0.25)

        // ══════════════════════════════════════════════════════
        // Function Row
        // ══════════════════════════════════════════════════════
        Item {
            id: funcRow

            width: parent.width
            height: keyboardPicker._kh

            PickerKey {
                x: 0; theme: keyboardPicker.theme; label: "Esc"; keyUnit: keyboardPicker._ku
                kind: "Keyboard"; value: "Escape"
                enabled: keyboardPicker._isSupported("Escape")
                selected: keyboardPicker.pendingValue === "Escape"
                onClicked: keyboardPicker.keySelected("Escape")
            }

            // F1-F4
            Row {
                x: keyboardPicker._ku + keyboardPicker._ks
                spacing: keyboardPicker._kg
                Repeater {
                    model: ["F1", "F2", "F3", "F4"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }

            // F5-F8
            Row {
                x: keyboardPicker._ku + keyboardPicker._ks
                    + 4 * keyboardPicker._ku + 3 * keyboardPicker._kg
                    + Math.round(keyboardPicker._ks * 0.5)
                spacing: keyboardPicker._kg
                Repeater {
                    model: ["F5", "F6", "F7", "F8"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }

            // F9-F12
            Row {
                x: keyboardPicker._ku + keyboardPicker._ks
                    + 8 * keyboardPicker._ku + 6 * keyboardPicker._kg
                    + keyboardPicker._ks
                spacing: keyboardPicker._kg
                Repeater {
                    model: ["F9", "F10", "F11", "F12"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }

            // PrtSc / ScrLk / Pause（导航区列位置）
            Row {
                x: keyboardPicker._navX
                spacing: keyboardPicker._kg
                Repeater {
                    model: ["PrtSc", "ScrLk", "Pause"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: false
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════
        // Body rows（数字行 ~ 底部行）
        // ══════════════════════════════════════════════════════
        readonly property real _bodyY: funcRow.height + _funcBodyGap

        // ── 第 1 行：数字行 ──
        Item {
            y: keyboardGrid._bodyY
            width: parent.width
            height: keyboardPicker._kh

            Row {
                spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "` ~"; keyUnit: keyboardPicker._ku; enabled: false }
                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
                PickerKey { theme: keyboardPicker.theme; label: "- _"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "= +"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Bksp"; keyUnit: keyboardPicker._ku; keyWidth: 2.0; enabled: false }
            }
            Row {
                x: keyboardPicker._navX; spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Insert"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Home"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "PgUp"; keyUnit: keyboardPicker._ku; enabled: false }
            }
            Row {
                x: keyboardPicker._numX; spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Num"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "/"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "*"; keyUnit: keyboardPicker._ku; enabled: false }
            }
        }

        // ── 第 2 行：Tab 行 ──
        Item {
            y: keyboardGrid._bodyY + 1 * (keyboardPicker._kh + keyboardPicker._kg)
            width: parent.width
            height: keyboardPicker._kh

            Row {
                spacing: keyboardPicker._kg
                PickerKey {
                    theme: keyboardPicker.theme; label: "Tab"; keyUnit: keyboardPicker._ku; keyWidth: 1.5
                    kind: "Keyboard"; value: "Tab"
                    enabled: keyboardPicker._isSupported("Tab")
                    selected: keyboardPicker.pendingValue === "Tab"
                    onClicked: keyboardPicker.keySelected("Tab")
                }
                Repeater {
                    model: ["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
                PickerKey { theme: keyboardPicker.theme; label: "[ {"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "] }"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "\\ |"; keyUnit: keyboardPicker._ku; keyWidth: 1.5; enabled: false }
            }
            Row {
                x: keyboardPicker._navX; spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Delete"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "End"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "PgDn"; keyUnit: keyboardPicker._ku; enabled: false }
            }
            Row {
                x: keyboardPicker._numX; spacing: keyboardPicker._kg
                Repeater {
                    model: ["7", "8", "9"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }
        }

        // ── 第 3 行：Caps Lock 行 ──
        Item {
            y: keyboardGrid._bodyY + 2 * (keyboardPicker._kh + keyboardPicker._kg)
            width: parent.width
            height: keyboardPicker._kh

            Row {
                spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Caps"; keyUnit: keyboardPicker._ku; keyWidth: 1.75; enabled: false }
                Repeater {
                    model: ["A", "S", "D", "F", "G", "H", "J", "K", "L"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
                PickerKey { theme: keyboardPicker.theme; label: "; :"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "' \""; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey {
                    theme: keyboardPicker.theme; label: "Enter"; keyUnit: keyboardPicker._ku; keyWidth: 2.25
                    kind: "Keyboard"; value: "Enter"
                    enabled: keyboardPicker._isSupported("Enter")
                    selected: keyboardPicker.pendingValue === "Enter"
                    onClicked: keyboardPicker.keySelected("Enter")
                }
            }
            // 导航区本行无键
            Row {
                x: keyboardPicker._numX; spacing: keyboardPicker._kg
                Repeater {
                    model: ["4", "5", "6"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }
        }

        // ── 第 4 行：Shift 行 ──
        Item {
            y: keyboardGrid._bodyY + 3 * (keyboardPicker._kh + keyboardPicker._kg)
            width: parent.width
            height: keyboardPicker._kh

            Row {
                spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Shift"; keyUnit: keyboardPicker._ku; keyWidth: 2.25; enabled: false }
                Repeater {
                    model: ["Z", "X", "C", "V", "B", "N", "M"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
                PickerKey { theme: keyboardPicker.theme; label: ", <"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: ". >"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "/ ?"; keyUnit: keyboardPicker._ku; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Shift"; keyUnit: keyboardPicker._ku; keyWidth: 2.75; enabled: false }
            }
            // 方向键：▲ 居中于导航区第二列位置
            PickerKey {
                x: keyboardPicker._navX + keyboardPicker._ku + keyboardPicker._kg
                theme: keyboardPicker.theme; label: "▲"; keyUnit: keyboardPicker._ku
                kind: "Keyboard"; value: "ArrowUp"
                enabled: keyboardPicker._isSupported("ArrowUp")
                selected: keyboardPicker.pendingValue === "ArrowUp"
                onClicked: keyboardPicker.keySelected("ArrowUp")
            }
            Row {
                x: keyboardPicker._numX; spacing: keyboardPicker._kg
                Repeater {
                    model: ["1", "2", "3"]
                    PickerKey {
                        required property string modelData
                        theme: keyboardPicker.theme; label: modelData; keyUnit: keyboardPicker._ku
                        kind: "Keyboard"; value: modelData
                        enabled: keyboardPicker._isSupported(modelData)
                        selected: keyboardPicker.pendingValue === modelData
                        onClicked: keyboardPicker.keySelected(modelData)
                    }
                }
            }
        }

        // ── 第 5 行：底部行 ──
        Item {
            y: keyboardGrid._bodyY + 4 * (keyboardPicker._kh + keyboardPicker._kg)
            width: parent.width
            height: keyboardPicker._kh

            Row {
                spacing: keyboardPicker._kg
                PickerKey { theme: keyboardPicker.theme; label: "Ctrl"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Win"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Alt"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
                PickerKey {
                    theme: keyboardPicker.theme; label: "Space"; keyUnit: keyboardPicker._ku; keyWidth: 6.25
                    kind: "Keyboard"; value: "Space"
                    enabled: keyboardPicker._isSupported("Space")
                    selected: keyboardPicker.pendingValue === "Space"
                    onClicked: keyboardPicker.keySelected("Space")
                }
                PickerKey { theme: keyboardPicker.theme; label: "Alt"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Fn"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
                PickerKey { theme: keyboardPicker.theme; label: "Ctrl"; keyUnit: keyboardPicker._ku; keyWidth: 1.25; enabled: false }
            }
            // 方向键：◄ ▼ ►
            Row {
                x: keyboardPicker._navX; spacing: keyboardPicker._kg
                PickerKey {
                    theme: keyboardPicker.theme; label: "◄"; keyUnit: keyboardPicker._ku
                    kind: "Keyboard"; value: "ArrowLeft"
                    enabled: keyboardPicker._isSupported("ArrowLeft")
                    selected: keyboardPicker.pendingValue === "ArrowLeft"
                    onClicked: keyboardPicker.keySelected("ArrowLeft")
                }
                PickerKey {
                    theme: keyboardPicker.theme; label: "▼"; keyUnit: keyboardPicker._ku
                    kind: "Keyboard"; value: "ArrowDown"
                    enabled: keyboardPicker._isSupported("ArrowDown")
                    selected: keyboardPicker.pendingValue === "ArrowDown"
                    onClicked: keyboardPicker.keySelected("ArrowDown")
                }
                PickerKey {
                    theme: keyboardPicker.theme; label: "►"; keyUnit: keyboardPicker._ku
                    kind: "Keyboard"; value: "ArrowRight"
                    enabled: keyboardPicker._isSupported("ArrowRight")
                    selected: keyboardPicker.pendingValue === "ArrowRight"
                    onClicked: keyboardPicker.keySelected("ArrowRight")
                }
            }
            Row {
                x: keyboardPicker._numX; spacing: keyboardPicker._kg
                PickerKey {
                    theme: keyboardPicker.theme; label: "0"; keyUnit: keyboardPicker._ku; keyWidth: 2.0
                    kind: "Keyboard"; value: "0"
                    enabled: keyboardPicker._isSupported("0")
                    selected: keyboardPicker.pendingValue === "0"
                    onClicked: keyboardPicker.keySelected("0")
                }
                PickerKey { theme: keyboardPicker.theme; label: "."; keyUnit: keyboardPicker._ku; enabled: false }
            }
        }

        // ══════════════════════════════════════════════════════
        // 第 6 行占位（只有底部行，无第6行——但上面只到第5行=index 4）
        // 如果需要加 minus 行可在这里扩展
        // ══════════════════════════════════════════════════════
    }
}
