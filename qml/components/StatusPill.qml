import QtQuick 2.15
import ZenithUI 1.0

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property color tone: "#16A34A"
    property color valueColor: "#17212E"
    property color borderTone: "#E3EAF3"

    radius: 14
    color: "#FFFFFF"
    border.color: root.borderTone
    border.width: 1
    implicitHeight: 46

    Row {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: root.tone
            anchors.verticalCenter: parent.verticalCenter
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.label
                color: "#73859A"
                font.pixelSize: 11
            }

            Text {
                text: root.value
                color: root.valueColor
                font.pixelSize: 13
                font.bold: true
            }
        }
    }
}
