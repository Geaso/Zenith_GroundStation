import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    property color accent: "#2F6BFF"
    property string title: ""
    property string subtitle: ""
    default property alias contentData: content.data

    radius: 18
    color: "#F8FAFD"
    border.color: "#DCE4F2"
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Column {
            spacing: 4

            Text {
                text: root.title
                color: "#1D2A3A"
                font.pixelSize: 16
                font.bold: true
            }

            Text {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: "#70839A"
                font.pixelSize: 12
            }
        }

        Rectangle {
            width: 48
            height: 4
            radius: 2
            color: root.accent
        }

        Column {
            id: content
            width: parent.width
            spacing: 12
        }
    }
}
