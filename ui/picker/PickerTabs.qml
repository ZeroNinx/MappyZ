import QtQuick

// 映射选择器页签栏：键盘 / 鼠标 / DInput
Row {
    id: pickerTabs

    required property var theme

    property int currentIndex: 0

    signal tabClicked(int index)

    spacing: 0

    Repeater {
        model: ["键盘", "鼠标", "DInput"]

        Rectangle {
            required property int index
            required property string modelData

            width: 100
            height: 40
            color: "transparent"

            Text {
                anchors.centerIn: parent
                text: modelData
                color: pickerTabs.currentIndex === index
                    ? "#ffffff" : pickerTabs.theme.muted
                font.pixelSize: 14
                font.bold: pickerTabs.currentIndex === index
            }

            // 蓝色底边指示当前页签
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 3
                color: pickerTabs.theme.accent
                visible: pickerTabs.currentIndex === index
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    pickerTabs.currentIndex = index
                    pickerTabs.tabClicked(index)
                }
            }
        }
    }
}
