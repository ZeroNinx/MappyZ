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

        // 卡片列宽度：占可用宽度 24%，限制在 155~200 之间
        readonly property real cardColumnWidth: Math.max(
            Math.min(width * 0.24, 200), 155)

        // 手柄图示可用宽高（左右留出卡片 + 间隙）
        readonly property real bodyAvailWidth: width - 2 * cardColumnWidth - 16
        // 手柄保持横向比例，上限 600
        readonly property real bodyWidth: Math.min(
            bodyAvailWidth,
            gamepadMappingView.defaultGamepadLayout.glyph.maxWidth)
        readonly property real bodyHeight: bodyWidth / 1.8

        // ── 中间：手柄图示（上移，作为中心视觉锚点）──

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
            // 手柄整体上移：中心位于面板 38% 高度处
            y: parent.height * 0.38 - height / 2

            onControlSelected: function(cid) {
                gamepadMappingView.controlSelected(cid)
            }
        }

        // ── 顶部：Back / Guide / Start（紧贴手柄上方）──

        MappingGroupCard {
            id: topCard
            z: 1
            theme: gamepadMappingView.theme
            mappingRuleModel: gamepadMappingView._mappingRuleModel
            selectedControl: gamepadMappingView.selectedControl
            mappingRevision: gamepadMappingView._mappingRevision
            title: gamepadMappingView.defaultGamepadLayout.topCard.title
            controls: gamepadMappingView.defaultGamepadLayout.topCard.controls
            width: layout.cardColumnWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: gamepadGlyph.top
            anchors.bottomMargin: 6
            onControlClicked: function(cid) { gamepadMappingView.controlSelected(cid) }
            onControlDoubleClicked: function(cid) { gamepadMappingView.controlDoubleClicked(cid) }
        }

        // ── 左列：LB/LT + Left Stick（紧贴手柄左侧）──

        Column {
            id: leftColumn
            z: 1
            width: layout.cardColumnWidth
            anchors.right: gamepadGlyph.left
            anchors.rightMargin: 6
            anchors.verticalCenter: gamepadGlyph.verticalCenter
            spacing: 6

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
            anchors.left: gamepadGlyph.right
            anchors.leftMargin: 6
            anchors.verticalCenter: gamepadGlyph.verticalCenter
            spacing: 6

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

        // ── 底部行：D-Pad + Right Stick（紧贴手柄下方，以中心线对称）──

        Row {
            id: bottomRow
            z: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: gamepadGlyph.bottom
            anchors.topMargin: 6
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
