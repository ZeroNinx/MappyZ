import QtQuick

// 手柄控制组映射卡片：显示一组控件的当前绑定，支持选中和双击编辑
Rectangle {
    id: card

    required property var theme
    required property var mappingRuleModel
    required property string selectedControl
    required property string title
    required property var controls
    // 递增 revision 触发 output 绑定刷新
    property int mappingRevision: 0

    signal controlClicked(string controlId)
    signal controlDoubleClicked(string controlId)

    implicitWidth: 170
    implicitHeight: cardColumn.implicitHeight + 14
    radius: 6
    color: Qt.rgba(0.12, 0.13, 0.14, 0.92)
    border.color: card.theme.border
    border.width: 1

    Column {
        id: cardColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 1

        // 标题
        Text {
            text: card.title
            color: card.theme.text
            font.pixelSize: 12
            font.bold: true
            leftPadding: 4
            bottomPadding: 3
        }

        // 映射行
        Repeater {
            model: card.controls

            Rectangle {
                id: rowRect

                required property var modelData
                required property int index

                width: cardColumn.width
                height: 24
                radius: 3
                color: {
                    if (card.selectedControl === modelData.controlId)
                        return card.theme.accentHover
                    if (rowMouse.containsMouse)
                        return "#2a2a2a"
                    return "transparent"
                }
                border.color: card.selectedControl === modelData.controlId
                    ? card.theme.accent : "transparent"
                border.width: card.selectedControl === modelData.controlId ? 1 : 0

                // 输入标签
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: card.selectedControl === modelData.controlId
                        ? "#ffffff" : card.theme.text
                    font.pixelSize: 12
                    font.bold: card.selectedControl === modelData.controlId
                }

                // 输出值
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.left: parent.left
                    anchors.leftMargin: parent.width * 0.42
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: {
                        void(card.mappingRevision)
                        if (!card.mappingRuleModel) return "Unassigned"
                        var output = card.mappingRuleModel.displayOutputForInput(modelData.controlId)
                        return output !== "" ? output : "Unassigned"
                    }
                    color: {
                        void(card.mappingRevision)
                        if (!card.mappingRuleModel) return card.theme.muted
                        var output = card.mappingRuleModel.displayOutputForInput(modelData.controlId)
                        return output !== "" ? card.theme.accent : card.theme.muted
                    }
                    font.pixelSize: 12
                    font.italic: {
                        void(card.mappingRevision)
                        if (!card.mappingRuleModel) return true
                        return card.mappingRuleModel.displayOutputForInput(modelData.controlId) === ""
                    }
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: card.controlClicked(modelData.controlId)
                    onDoubleClicked: card.controlDoubleClicked(modelData.controlId)
                }
            }
        }
    }
}
