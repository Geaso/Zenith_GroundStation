import QtQuick 2.15

Row {
    id: root
    property string title: ""
    property string caption: ""
    spacing: 10

    Rectangle {
        width: 5
        height: 26
        radius: 3
        color: "#2F6BFF"
    }

    Column {
        spacing: 2

        Text {
            text: root.title
            color: "#16202D"
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            visible: root.caption.length > 0
            text: root.caption
            color: "#71849A"
            font.pixelSize: 12
        }
    }
}
