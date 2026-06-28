import QtQuick

// 圆角标签胶囊，用于状态指示和类别标记
Rectangle {
    required property var theme

    property string label: ""
    property color tone: theme.accentSoft

    width: tagText.implicitWidth + 16
    height: 22
    radius: 11
    color: tone

    Text {
        id: tagText

        anchors.centerIn: parent
        text: parent.label
        color: "#ffffff"
        font.pixelSize: 11
    }
}
