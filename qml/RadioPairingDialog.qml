import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root
    modal: true
    width: 760
    height: 610
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside

    property string operationMessage: ""

    function loadRow(row) {
        var item = appState.radioPairingModel.pairingAt(row)
        if (!item || !item.name) {
            nameField.text = ""
            addressField.text = ""
            uavIdField.text = ""
            noteField.text = ""
            return
        }
        nameField.text = item.name
        addressField.text = String(item.radio_address)
        uavIdField.text = String(item.uav_id)
        noteField.text = item.note || ""
    }

    function currentItem() {
        return appState.radioPairingModel.pairingAt(pairingList.currentIndex)
    }

    function report(ok, successText) {
        operationMessage = ok ? successText : appState.radioPairingModel.lastError
        if (ok) {
            pairingList.currentIndex = appState.selectedRadioPairingIndex
            loadRow(pairingList.currentIndex)
        }
    }

    onOpened: {
        operationMessage = ""
        pairingList.currentIndex = appState.selectedRadioPairingIndex
        loadRow(pairingList.currentIndex)
    }

    background: Rectangle { radius: 12; color: "#161B22"; border.color: "#30363D" }

    header: Rectangle {
        height: 50; radius: 12; color: "#161B22"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#30363D" }
        Column {
            anchors.left: parent.left; anchors.leftMargin: 20; anchors.verticalCenter: parent.verticalCenter; spacing: 1
            Text { text: "数传配对管理"; color: "#E6EDF3"; font.pixelSize: 15; font.bold: true }
            Text { text: "地址与飞机 ID 均限定 1–254，且必须一致"; color: "#8B949E"; font.pixelSize: 10 }
        }
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter
            width: 28; height: 28; radius: 6; color: closeArea.containsMouse ? "#30363D" : "transparent"
            Text { anchors.centerIn: parent; text: "\u00D7"; color: "#8B949E"; font.pixelSize: 18 }
            MouseArea { id: closeArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.close() }
        }
    }

    contentItem: Item {
        Row {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            Rectangle {
                width: 330; height: parent.height; radius: 8; color: "#0D1117"; border.color: "#30363D"

            Column {
                anchors.fill: parent; anchors.margins: 10; spacing: 8
                Row {
                    width: parent.width
                    Text { text: "配对表"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true }
                    Item { width: parent.width - 130; height: 1 }
                    Text { text: appState.radioPairingModel.count + " 项"; color: "#6E7681"; font.pixelSize: 10 }
                }

                ListView {
                    id: pairingList
                    width: parent.width; height: parent.height - 36
                    clip: true; model: appState.radioPairingModel
                    spacing: 4
                    onCurrentIndexChanged: root.loadRow(currentIndex)
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Rectangle {
                        required property int index
                        required property string name
                        required property int radioAddress
                        required property int uavId
                        required property string note
                        required property bool builtIn
                        required property bool overridden
                        width: pairingList.width - 8; height: 62; radius: 6
                        color: pairingList.currentIndex === index ? "#1F4E8C" : itemArea.containsMouse ? "#21262D" : "#161B22"
                        border.color: pairingList.currentIndex === index ? "#58A6FF" : "#30363D"

                        Column {
                            anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                            Row {
                                spacing: 6
                                Text { text: name; color: "#E6EDF3"; font.pixelSize: 12; font.bold: true }
                                Text { text: builtIn ? (overridden ? "内置·已覆盖" : "内置") : "自定义"; color: overridden ? "#FFA657" : "#8B949E"; font.pixelSize: 9 }
                            }
                            Text { text: "数传 " + radioAddress + "  ·  UAV " + uavId; color: "#58A6FF"; font.pixelSize: 11; font.family: "Consolas" }
                            Text { text: note || "无备注"; color: "#6E7681"; font.pixelSize: 9; width: 285; elide: Text.ElideRight }
                        }
                        MouseArea { id: itemArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: pairingList.currentIndex = index }
                    }
                }
            }
        }

            Rectangle {
                width: parent.width - 330 - 14; height: parent.height; radius: 8; color: "#21262D"; border.color: "#30363D"

            Column {
                anchors.fill: parent; anchors.margins: 14; spacing: 10

                Text { text: pairingList.currentIndex >= 0 ? "编辑配对" : "新增配对"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true }
                Text {
                    text: {
                        var item = root.currentItem()
                        return item && item.builtIn ? "内置名称受保护；保存会生成用户覆盖，可随时恢复。" : "名称、地址和备注会保存到当前用户配置。"
                    }
                    color: "#8B949E"; font.pixelSize: 10; wrapMode: Text.Wrap; width: parent.width
                }

                Column {
                    spacing: 4
                    Text { text: "名称"; color: "#8B949E"; font.pixelSize: 10 }
                    TextField {
                        id: nameField; width: 340; height: 30; color: "#E6EDF3"; font.pixelSize: 12
                        placeholderText: "例如：测试机216"; maximumLength: 64
                        readOnly: { var item = root.currentItem(); return item && item.builtIn }
                        background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                    }
                }

                Row {
                    spacing: 10
                    Column {
                        spacing: 4
                        Text { text: "数传地址"; color: "#8B949E"; font.pixelSize: 10 }
                        TextField {
                            id: addressField; width: 165; height: 30; color: "#E6EDF3"; font.pixelSize: 12
                            placeholderText: "1–254"; validator: IntValidator { bottom: 1; top: 254 }
                            background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                        }
                    }
                    Column {
                        spacing: 4
                        Text { text: "飞机 UAV ID"; color: "#8B949E"; font.pixelSize: 10 }
                        TextField {
                            id: uavIdField; width: 165; height: 30; color: "#E6EDF3"; font.pixelSize: 12
                            placeholderText: "与数传地址相同"; validator: IntValidator { bottom: 1; top: 254 }
                            background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                        }
                    }
                }

                Column {
                    spacing: 4
                    Text { text: "备注"; color: "#8B949E"; font.pixelSize: 10 }
                    TextField {
                        id: noteField; width: 340; height: 30; color: "#E6EDF3"; font.pixelSize: 12
                        placeholderText: "机体、客户或交付状态"; maximumLength: 200
                        background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                    }
                }

                Row {
                    spacing: 7
                    Button {
                        text: "新建"; width: 70; height: 30
                        onClicked: { pairingList.currentIndex = -1; root.operationMessage = ""; root.loadRow(-1) }
                    }
                    Button {
                        text: pairingList.currentIndex >= 0 ? "保存修改" : "添加"; width: 88; height: 30
                        onClicked: {
                            var ok = pairingList.currentIndex >= 0
                                ? appState.updateRadioPairing(pairingList.currentIndex, nameField.text, Number(addressField.text), Number(uavIdField.text), noteField.text)
                                : appState.addRadioPairing(nameField.text, Number(addressField.text), Number(uavIdField.text), noteField.text)
                            root.report(ok, "配对表已保存")
                        }
                    }
                    Button {
                        text: "删除"; width: 66; height: 30
                        enabled: { var item = root.currentItem(); return item && item.canDelete }
                        onClicked: root.report(appState.removeRadioPairing(pairingList.currentIndex), "自定义配对已删除")
                    }
                    Button {
                        text: "恢复内置"; width: 82; height: 30
                        enabled: { var item = root.currentItem(); return item && item.builtIn && item.overridden }
                        onClicked: root.report(appState.restoreBuiltInRadioPairing(pairingList.currentIndex), "已恢复内置值")
                    }
                }

                Text {
                    width: parent.width; wrapMode: Text.Wrap
                    text: root.operationMessage || appState.radioPairingModel.lastError
                    color: root.operationMessage === "配对表已保存" || root.operationMessage.indexOf("已删除") >= 0 || root.operationMessage.indexOf("已恢复") >= 0 ? "#3FB950" : "#F85149"
                    font.pixelSize: 10
                }

                Item { width: 1; height: 1; Layout.fillHeight: true }
                Text {
                    width: parent.width; wrapMode: Text.Wrap
                    text: "提示：运行中切换配对会立即重新进入 LR24 配置模式；验证成功前不会发送心跳或业务指令。"
                    color: "#FFA657"; font.pixelSize: 10
                }
            }
            }
        }
    }
}
