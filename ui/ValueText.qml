import QtQuick

// 标准值文本，用于设备信息行的右侧内容
Text {
    required property var theme

    color: theme.text
    font.pixelSize: 12
    elide: Text.ElideRight
}
