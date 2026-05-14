import QtQuick 2.15
import QtQuick.Controls 2.15
import "../components"

Item {
    Column {
        anchors.fill: parent
        spacing: 10

        // 标题行
        Row {
            width: parent.width
            height: 32
            spacing: 10
            Text {
                text: "功能脚本"
                color: "#E6EDF3"; font.pixelSize: 16; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: scriptActionModel.count + " 条脚本"
                color: "#6E7681"; font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 添加栏
        Rectangle {
            width: parent.width; height: 60
            radius: 10; color: "#161B22"; border.color: "#30363D"

            Row {
                anchors.fill: parent; anchors.margins: 10; spacing: 10

                Column {
                    spacing: 3; anchors.verticalCenter: parent.verticalCenter
                    Text { text: "按钮名"; color:"#8B949E"; font.pixelSize:10 }
                    TextField {
                        id: nameField; width: 160; height: 30
                        placeholderText: "脚本名称"
                        color:"#E6EDF3"; font.pixelSize:13; placeholderTextColor:"#6E7681"
                        background: Rectangle { radius:6; color:"#21262D"; border.color:"#30363D" }
                    }
                }
                Column {
                    spacing: 3; anchors.verticalCenter: parent.verticalCenter
                    Text { text: "运行指令 / 路径"; color:"#8B949E"; font.pixelSize:10 }
                    TextField {
                        id: commandField; width: 480; height: 30
                        placeholderText: "e.g. /opt/scripts/run.sh  或  ros2 run pkg node"
                        color:"#E6EDF3"; font.pixelSize:13; placeholderTextColor:"#6E7681"
                        background: Rectangle { radius:6; color:"#21262D"; border.color:"#30363D" }
                    }
                }
                Column {
                    spacing: 3; anchors.verticalCenter: parent.verticalCenter
                    Text { text: "发送方式"; color:"#8B949E"; font.pixelSize:10 }
                    ComboBox {
                        id: targetBox; width: 200; height: 30
                        model: ["Send To Current UAV", "Send To Local Station", "Send To Remote Host"]
                        background: Rectangle { radius:6; color:"#21262D"; border.color:"#30363D" }
                        contentItem: Text {
                            leftPadding:8; text:targetBox.currentText
                            color:"#E6EDF3"; font.pixelSize:12; verticalAlignment:Text.AlignVCenter
                        }
                    }
                }
                PrimaryButton {
                    width: 80; height: 30; text: "添加"
                    fillColor: "#1F6FEB"
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        if (nameField.text.length === 0 || commandField.text.length === 0) return
                        scriptActionModel.addAction(nameField.text, commandField.text, targetBox.currentText)
                        nameField.text = ""; commandField.text = ""
                    }
                }
            }
        }

        // 列表头
        Rectangle {
            width: parent.width; height: 28
            color: "transparent"
            Row {
                anchors.fill: parent; anchors.leftMargin: 14; spacing: 0
                Text { text:"按钮名"; color:"#6E7681"; font.pixelSize:11; font.bold:true; width:170 }
                Text { text:"运行指令"; color:"#6E7681"; font.pixelSize:11; font.bold:true; width:parent.width-170-210-200-20 }
                Text { text:"发送方式"; color:"#6E7681"; font.pixelSize:11; font.bold:true; width:210 }
                Text { text:"操作"; color:"#6E7681"; font.pixelSize:11; font.bold:true; width:200 }
            }
        }

        // 脚本列表
        Rectangle {
            width: parent.width
            height: parent.height - 32 - 60 - 28 - 30
            radius: 10; color: "#161B22"; border.color: "#30363D"

            ListView {
                anchors.fill: parent
                anchors.margins: 8
                model: scriptActionModel
                spacing: 6
                clip: true

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 50
                    radius: 8
                    color: "#21262D"
                    border.color: "#30363D"

                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 0

                        // 名称
                        Column {
                            width: 160; anchors.verticalCenter: parent.verticalCenter; spacing: 2
                            Text { text: name; color:"#E6EDF3"; font.pixelSize:13; font.bold:true; elide:Text.ElideRight; width:parent.width }
                            Rectangle { width: 40; height: 4; radius: 2; color: "#1F6FEB" }
                        }
                        Item { width: 10 }

                        // 指令
                        Text {
                            width: parent.width - 160 - 210 - 200 - 30
                            text: command; color:"#8B949E"; font.pixelSize:12; font.family:"Monospace"
                            elide:Text.ElideRight; anchors.verticalCenter:parent.verticalCenter
                        }

                        // 发送方式
                        Text {
                            width: 210; text: target; color:"#6E7681"; font.pixelSize:12
                            elide:Text.ElideRight; anchors.verticalCenter:parent.verticalCenter
                        }

                        // 操作按钮
                        Row {
                            width: 200; spacing: 8; anchors.verticalCenter:parent.verticalCenter
                            PrimaryButton {
                                width: 86; height: 30; text: "运行"
                                fillColor: "#1A4A2E"
                                onClicked: appState.runScriptAction(name, command, target)
                            }
                            PrimaryButton {
                                width: 86; height: 30; text: "删除"
                                fillColor: "#21262D"; textColor: "#6E7681"
                                onClicked: scriptActionModel.removeAction(index)
                            }
                        }
                    }
                }
            }
        }
    }
}
