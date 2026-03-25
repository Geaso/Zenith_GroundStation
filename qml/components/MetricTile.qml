import QtQuick 2.15

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property color tone: "#1F7A5A"

    radius: 14
    color: "#FFFFFF"
    border.color: "#E6ECF5"
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Text {
            text: root.label
            color: "#6E8095"
            font.pixelSize: 12
        }

        Row {
            spacing: 6

            Text {
                text: root.value
                color: "#16202D"
                font.pixelSize: 26
                font.bold: true
            }

            Text {
                text: root.unit
                color: root.tone
                font.pixelSize: 12
                anchors.baseline: parent.children[0].baseline
            }
        }
    }
}
