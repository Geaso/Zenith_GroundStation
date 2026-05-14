import QtQuick 2.15
import QtQuick.Controls 2.15
import "../components"

Item {
    id: root

    property var paramStoreModel: appState.paramStore()

    Row {
        anchors.fill: parent
        spacing: 10

        // ═══════════════════════════════════════
        //  Left: Parameter Table
        // ═══════════════════════════════════════
        Rectangle {
            width: parent.width - 240 - 10
            height: parent.height
            radius: 12
            color: "#0D1117"
            border.color: "#21262D"

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Row {
                    width: parent.width; spacing: 10
                    Text { text: "参数列表"; color: "#8B949E"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: paramStoreModel ? ("共 " + paramStoreModel.count + " 项") : "未加载"
                        color: "#6E7681"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Table header
                Rectangle {
                    width: parent.width; height: 28; radius: 4; color: "#161B22"
                    Row {
                        anchors.fill: parent; anchors.margins: 8; spacing: 0
                        Text { width: parent.width * 0.55; text: "参数名"; color: "#6E7681"; font.pixelSize: 10; font.bold: true }
                        Text { width: parent.width * 0.30; text: "值"; color: "#6E7681"; font.pixelSize: 10; font.bold: true }
                        Text { width: parent.width * 0.15; text: "类型"; color: "#6E7681"; font.pixelSize: 10; font.bold: true }
                    }
                }

                // Parameter list
                ListView {
                    id: paramList
                    width: parent.width
                    height: parent.height - 50
                    model: paramStoreModel
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: paramList.width
                        height: 34
                        radius: 4
                        color: index % 2 === 0 ? "#161B22" : "#0D1117"
                        border.color: editField.activeFocus ? "#1F6FEB" : "transparent"

                        Row {
                            anchors.fill: parent; anchors.margins: 8; spacing: 0

                            // Param name (shortened — remove common prefix)
                            Text {
                                width: parent.width * 0.55
                                text: {
                                    var n = model.paramName || ""
                                    var parts = n.split("/")
                                    return parts.length > 2 ? parts.slice(-2).join("/") : n
                                }
                                color: "#C9D1D9"; font.pixelSize: 11; font.family: "Consolas"
                                elide: Text.ElideLeft
                                anchors.verticalCenter: parent.verticalCenter

                                ToolTip.text: model.paramName || ""
                                ToolTip.visible: nameMouseArea.containsMouse
                                ToolTip.delay: 500
                                MouseArea { id: nameMouseArea; anchors.fill: parent; hoverEnabled: true }
                            }

                            // Editable value
                            TextField {
                                id: editField
                                width: parent.width * 0.30
                                height: 26
                                text: model.paramValue || ""
                                color: "#E6EDF3"; font.pixelSize: 11; font.family: "Consolas"
                                anchors.verticalCenter: parent.verticalCenter
                                background: Rectangle { radius: 4; color: "#21262D"; border.color: editField.activeFocus ? "#1F6FEB" : "#30363D" }
                                onEditingFinished: {
                                    if (paramStoreModel && text !== model.paramValue)
                                        paramStoreModel.setValue(index, text)
                                }
                            }

                            // Type label
                            Text {
                                width: parent.width * 0.15
                                text: {
                                    var t = model.paramType || 0
                                    return ["?", "INT", "LONG", "FLOAT", "DBL", "STR", "BOOL"][t] || "?"
                                }
                                color: "#6E7681"; font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }

        // ═══════════════════════════════════════
        //  Right: Controls
        // ═══════════════════════════════════════
        Column {
            width: 240
            height: parent.height
            spacing: 8

            // Download params
            Rectangle {
                width: parent.width; height: 180
                radius: 10; color: "#161B22"; border.color: "#30363D"
                Column {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Text { text: "下载参数"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                    Rectangle { width: parent.width; height: 1; color: "#262C36" }
                    PrimaryButton {
                        width: parent.width; height: 30; text: "飞控参数"; fillColor: "#1F4E8C"
                        onClicked: appState.requestParams(1)
                    }
                    PrimaryButton {
                        width: parent.width; height: 30; text: "通信参数"; fillColor: "#1F4E8C"
                        onClicked: appState.requestParams(2)
                    }
                    PrimaryButton {
                        width: parent.width; height: 30; text: "集群参数"; fillColor: "#1F4E8C"
                        onClicked: appState.requestParams(3)
                    }
                }
            }

            // Upload modified params
            Rectangle {
                width: parent.width; height: 100
                radius: 10; color: "#161B22"; border.color: "#30363D"
                Column {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Text { text: "上传修改"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                    Rectangle { width: parent.width; height: 1; color: "#262C36" }
                    PrimaryButton {
                        width: parent.width; height: 32; text: "上传已修改参数"; fillColor: "#1A5C30"
                        onClicked: appState.uploadDirtyParams()
                    }
                }
            }

            // Info
            Rectangle {
                width: parent.width
                height: parent.height - 180 - 100 - 16
                radius: 10; color: "#161B22"; border.color: "#30363D"
                Column {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Text { text: "说明"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                    Rectangle { width: parent.width; height: 1; color: "#262C36" }
                    Text {
                        width: parent.width
                        text: "1. 点击左侧按钮下载对应模块参数\n2. 在表格中直接编辑参数值\n3. 编辑后点击「上传已修改参数」\n4. 修改 location_source 会自动切换定位源"
                        color: "#6E7681"; font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }
                }
            }
        }
    }
}
