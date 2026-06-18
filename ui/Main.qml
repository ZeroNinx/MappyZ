import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 640
    height: 420
    visible: true
    title: "MappyZ"
    color: "#f5f7fb"

    Rectangle {
        anchors.centerIn: parent
        width: 320
        height: 160
        radius: 8
        color: "#ffffff"
        border.color: "#d7dde8"

        Text {
            anchors.centerIn: parent
            text: "Hello World"
            color: "#20242c"
            font.pixelSize: 34
            font.weight: Font.DemiBold
        }
    }
}
