import QtQuick

// 浅色小字标签，用于设备信息行的左侧字段名
Text {
    required property var theme

    color: theme.muted
    font.pixelSize: 11
}
