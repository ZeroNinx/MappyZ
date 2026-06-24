import QtQuick

// 内联反馈消息：显示操作结果，短暂停留后自动消失
// 用法：调用 show(text, tone) 显示消息，tone 为颜色值
Rectangle {
    id: inlineMessage

    required property var theme

    property string text: ""
    property color tone: theme.success

    visible: text !== ""
    width: parent ? parent.width : 0
    height: visible ? msgText.implicitHeight + 12 : 0
    radius: 3
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.15)
    border.color: tone
    border.width: 1

    Text {
        id: msgText

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: inlineMessage.text
        color: inlineMessage.tone
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }

    Timer {
        id: autoClearTimer

        interval: 3000
        repeat: false
        onTriggered: inlineMessage.text = ""
    }

    // 显示反馈消息，自动在 3 秒后清除
    function show(message, messageTone) {
        text = message
        tone = messageTone || theme.success
        autoClearTimer.restart()
    }
}
