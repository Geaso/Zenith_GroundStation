import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: root
    property color fillColor: "#2F6BFF"
    property color textColor: "#FFFFFF"

    implicitHeight: 42
    implicitWidth: 132

    opacity: root.enabled ? 1.0 : 0.4

    background: Rectangle {
        radius: 12
        color: root.down ? Qt.darker(root.fillColor, 1.08) : root.fillColor
        border.color: Qt.darker(root.fillColor, 1.12)
    }

    contentItem: Text {
        text: root.text
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.bold: true
    }
}
