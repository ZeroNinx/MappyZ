import QtQuick

// 可编辑手柄映射视图：手柄图示居中 + 围绕布局映射卡片
Panel {
    id: gamepadMappingView

    required property var appController
    required property string selectedDevice
    required property string selectedControl

    heading: "Gamepad Mapping View"

    signal controlSelected(string controlId)
    signal controlDoubleClicked(string controlId)

    readonly property var _mappingRuleModel: appController
        ? appController.mappingRuleModel : null
    readonly property var _inputStateModel: appController
        ? appController.inputStateModel : null

    // model reset 时递增，强制卡片 output 绑定刷新
    property int _mappingRevision: 0

    Connections {
        target: gamepadMappingView._mappingRuleModel
        function onModelReset() { gamepadMappingView._mappingRevision++ }
    }

    // ── Layout Preset 数据 ──

    readonly property var defaultGamepadLayout: ({
        glyph: {
            widthRatio: 0.50,
            maxWidth: 600,
            aspectRatio: 1.8
        },
        topCard: {
            title: "Back / Guide / Start",
            controls: [
                {controlId: "button_back", label: "Back"},
                {controlId: "button_guide", label: "Guide"},
                {controlId: "button_start", label: "Start"}
            ]
        },
        leftCards: [
            {
                title: "LB / LT",
                controls: [
                    {controlId: "left_shoulder", label: "LB"},
                    {controlId: "left_trigger", label: "LT"}
                ]
            },
            {
                title: "Left Stick",
                controls: [
                    {controlId: "left_stick_button", label: "Click"},
                    {controlId: "left_stick_up", label: "Up"},
                    {controlId: "left_stick_left", label: "Left"},
                    {controlId: "left_stick_down", label: "Down"},
                    {controlId: "left_stick_right", label: "Right"}
                ]
            }
        ],
        rightCards: [
            {
                title: "RB / RT",
                controls: [
                    {controlId: "right_shoulder", label: "RB"},
                    {controlId: "right_trigger", label: "RT"}
                ]
            },
            {
                title: "ABXY",
                controls: [
                    {controlId: "button_south", label: "A"},
                    {controlId: "button_east", label: "B"},
                    {controlId: "button_west", label: "X"},
                    {controlId: "button_north", label: "Y"}
                ]
            }
        ],
        bottomCards: [
            {
                title: "D-Pad",
                controls: [
                    {controlId: "dpad_up", label: "Up"},
                    {controlId: "dpad_down", label: "Down"},
                    {controlId: "dpad_left", label: "Left"},
                    {controlId: "dpad_right", label: "Right"}
                ]
            },
            {
                title: "Right Stick",
                controls: [
                    {controlId: "right_stick_button", label: "Click"},
                    {controlId: "right_stick_up", label: "Up"},
                    {controlId: "right_stick_down", label: "Down"},
                    {controlId: "right_stick_left", label: "Left"},
                    {controlId: "right_stick_right", label: "Right"}
                ]
            }
        ]
    })

    Item {
        id: layout
        anchors.fill: parent

        readonly property real gap: 8
        readonly property real cardColumnWidth: Math.max(
            Math.min(width * 0.235, 210), 148)
        readonly property real horizontalGlyphSpace: Math.max(
            width - 2 * cardColumnWidth - 2 * gap, 0)
        readonly property real glyphTop: topCard.y + topCard.height + gap
        readonly property real glyphBottom: bottomRow.y - gap
        readonly property real verticalGlyphSpace: Math.max(
            glyphBottom - glyphTop, 0)
        readonly property real bodyWidth: Math.max(0, Math.min(
            horizontalGlyphSpace,
            verticalGlyphSpace * gamepadMappingView.defaultGamepadLayout.glyph.aspectRatio,
            gamepadMappingView.defaultGamepadLayout.glyph.maxWidth))
        readonly property real bodyHeight: bodyWidth
            / gamepadMappingView.defaultGamepadLayout.glyph.aspectRatio

        // The cards own the panel's outer bands. The glyph only occupies the
        // remaining center safe area, so resizing it cannot push cards into
        // the title bar or beyond the content bounds.

        GamepadGlyph {
            id: gamepadGlyph
            z: 0

            theme: gamepadMappingView.theme
            inputStateModel: gamepadMappingView._inputStateModel
            selectedDevice: gamepadMappingView.selectedDevice
            selectedControl: gamepadMappingView.selectedControl

            width: layout.bodyWidth
            height: layout.bodyHeight
            anchors.horizontalCenter: parent.horizontalCenter
            y: layout.glyphTop
                + Math.max((layout.verticalGlyphSpace - height) / 2 - 3, 0)

            onControlSelected: function(cid) {
                gamepadMappingView.controlSelected(cid)
            }
        }

        // ── 顶部：功能键，固定在内容区顶部 ──

        MappingGroupCard {
            id: topCard
            z: 1
            theme: gamepadMappingView.theme
            mappingRuleModel: gamepadMappingView._mappingRuleModel
            selectedControl: gamepadMappingView.selectedControl
            mappingRevision: gamepadMappingView._mappingRevision
            title: gamepadMappingView.defaultGamepadLayout.topCard.title
            controls: gamepadMappingView.defaultGamepadLayout.topCard.controls
            width: Math.max(Math.min(layout.width * 0.34, 250), 190)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
            onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
        }

        // ── 左右列共享同一条水平中心线 ──

        Column {
            id: leftColumn
            z: 1
            width: layout.cardColumnWidth
            anchors.left: parent.left
            anchors.verticalCenter: gamepadGlyph.verticalCenter
            spacing: layout.gap

            Repeater {
                model: gamepadMappingView.defaultGamepadLayout.leftCards

                MappingGroupCard {
                    required property var modelData
                    width: leftColumn.width
                    theme: gamepadMappingView.theme
                    mappingRuleModel: gamepadMappingView._mappingRuleModel
                    selectedControl: gamepadMappingView.selectedControl
                    mappingRevision: gamepadMappingView._mappingRevision
                    title: modelData.title
                    controls: modelData.controls
                    onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                    onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
                }
            }
        }

        // ── 右列：RB/RT + ABXY（紧贴手柄右侧）──

        Column {
            id: rightColumn
            z: 1
            width: layout.cardColumnWidth
            anchors.right: parent.right
            anchors.verticalCenter: gamepadGlyph.verticalCenter
            spacing: layout.gap

            Repeater {
                model: gamepadMappingView.defaultGamepadLayout.rightCards

                MappingGroupCard {
                    required property var modelData
                    width: rightColumn.width
                    theme: gamepadMappingView.theme
                    mappingRuleModel: gamepadMappingView._mappingRuleModel
                    selectedControl: gamepadMappingView.selectedControl
                    mappingRevision: gamepadMappingView._mappingRevision
                    title: modelData.title
                    controls: modelData.controls
                    onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                    onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
                }
            }
        }

        // ── 底部行固定在内容区底边，两张卡片关于中心轴对称 ──

        Row {
            id: bottomRow
            z: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            spacing: 12

            Repeater {
                model: gamepadMappingView.defaultGamepadLayout.bottomCards

                MappingGroupCard {
                    required property var modelData
                    width: layout.cardColumnWidth
                    theme: gamepadMappingView.theme
                    mappingRuleModel: gamepadMappingView._mappingRuleModel
                    selectedControl: gamepadMappingView.selectedControl
                    mappingRevision: gamepadMappingView._mappingRevision
                    title: modelData.title
                    controls: modelData.controls
                    onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
                    onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
                }
            }
        }
    }
}
