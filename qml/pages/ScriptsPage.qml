import QtQuick 2.15
import QtQuick.Controls 2.15
import "../components"

Item {
    Column {
        anchors.fill: parent
        spacing: 12

        Text {
            text: "功能脚本"
            color: "#1D2A3A"
            font.pixelSize: 18
            font.bold: true
        }

        Rectangle {
            width: parent.width
            height: 78
            radius: 12
            color: "#F8FAFD"
            border.color: "#DCE4F2"

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                TextField { id: nameField; width: 180; placeholderText: "按钮名" }
                TextField { id: commandField; width: 520; placeholderText: "运行指令 / 脚本路径" }
                ComboBox { id: targetBox; width: 220; model: ["Send To Current UAV", "Send To Local Station", "Send To Remote Host"] }
                PrimaryButton {
                    width: 120
                    text: "添加"
                    onClicked: {
                        if (nameField.text.length === 0 || commandField.text.length === 0)
                            return
                        scriptActionModel.addAction(nameField.text, commandField.text, targetBox.currentText)
                        nameField.text = ""
                        commandField.text = ""
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 628
            radius: 12
            color: "#F8FAFD"
            border.color: "#DCE4F2"

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Row {
                    spacing: 12
                    Text { text: "按钮名"; color: "#6E8095"; font.pixelSize: 13; font.bold: true; width: 180 }
                    Text { text: "运行指令"; color: "#6E8095"; font.pixelSize: 13; font.bold: true; width: 560 }
                    Text { text: "发送方式"; color: "#6E8095"; font.pixelSize: 13; font.bold: true; width: 200 }
                    Text { text: "操作"; color: "#6E8095"; font.pixelSize: 13; font.bold: true }
                }

                ListView {
                    width: parent.width
                    height: 570
                    model: scriptActionModel
                    spacing: 8
                    clip: true

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 52
                        radius: 10
                        color: "#FFFFFF"
                        border.color: "#E5ECF5"

                        Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 12

                            Text {
                                text: name
                                color: "#17212E"
                                font.pixelSize: 14
                                font.bold: true
                                width: 180
                                elide: Text.ElideRight
                            }

                            Text {
                                text: command
                                color: "#425469"
                                font.pixelSize: 13
                                width: 560
                                elide: Text.ElideRight
                            }

                            Text {
                                text: target
                                color: "#596B81"
                                font.pixelSize: 13
                                width: 200
                                elide: Text.ElideRight
                            }

                            PrimaryButton {
                                width: 90
                                text: "运行"
                                onClicked: appState.runScriptAction(name, command, target)
                            }

                            PrimaryButton {
                                width: 90
                                text: "删除"
                                fillColor: "#E5ECF5"
                                textColor: "#233042"
                                onClicked: scriptActionModel.removeAction(index)
                            }
                        }
                    }
                }
            }
        }
    }
}
