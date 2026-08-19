import QtQuick 2.15
import QtQuick.Controls 2.15
import ZenithUI 1.0

Button {
    id: root
    property color fillColor: "#2F6BFF"
    property color textColor: "#FFFFFF"

    implicitHeight: 32
    implicitWidth: 120

    opacity: root.enabled ? 1.0 : 0.35

    background: Rectangle {
        radius: 6
        color: root.down ? Qt.darker(root.fillColor, 1.12) : root.hovered ? Qt.lighter(root.fillColor, 1.08) : root.fillColor
        border.color: Qt.darker(root.fillColor, 1.15)
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Text {
        text: root.text
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
        font.bold: true
    }
}
