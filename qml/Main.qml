import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1600
    height: 980
    visible: true
    title: "Zenith Ground Station"
    color: "#0D1117"

    property int currentPage: 0

    // ── Header ──
    header: Rectangle {
        height: 78
        color: "#161B22"

        Column {
            anchors.fill: parent
            spacing: 0

            // ── Row 1: Brand + Tabs + Key Status ──
            Item {
                width: parent.width; height: 42

                // Left: Brand + Vehicle + Connection
                Row {
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 14; spacing: 10

                    Column {
                        anchors.verticalCenter: parent.verticalCenter; spacing: 1
                        Text { text: "ZENITH"; color: "#58A6FF"; font.pixelSize: 17; font.bold: true; font.letterSpacing: 3 }
                        Text { text: "Ground Station"; color: "#6E7681"; font.pixelSize: 8; font.letterSpacing: 1 }
                    }

                    Rectangle { width: 1; height: 26; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }

                    ComboBox {
                        id: vehicleCombo; width: 86; height: 24
                        anchors.verticalCenter: parent.verticalCenter
                        model: ["UAV1", "UAV2", "UAV3"]; currentIndex: 0
                        onActivated: appState.selectVehicle(currentText)
                        background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
                        contentItem: Text { leftPadding: 8; text: vehicleCombo.currentText; color: "#E6EDF3"; font.pixelSize: 12; font.bold: true; verticalAlignment: Text.AlignVCenter }
                    }

                    Button {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 68; implicitHeight: 24; text: "连接设置"
                        onClicked: connectionDialog.open()
                        background: Rectangle { radius: 5; color: "#21262D"; border.color: "#1F6FEB" }
                        contentItem: Text { text: parent.text; color: "#58A6FF"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }

                // Center: Tabs
                Row {
                    anchors.centerIn: parent; spacing: 4
                    Repeater {
                        model: [ { label: "概览", page: 0 }, { label: "脚本", page: 1 }, { label: "参数", page: 2 } ]
                        delegate: Button {
                            implicitWidth: 60; implicitHeight: 26; text: modelData.label
                            onClicked: window.currentPage = modelData.page
                            background: Rectangle { radius: 6; color: window.currentPage === modelData.page ? "#1F6FEB" : "transparent"; border.color: window.currentPage === modelData.page ? "#1F6FEB" : "#30363D" }
                            contentItem: Text { text: parent.text; color: window.currentPage === modelData.page ? "#FFFFFF" : "#8B949E"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }

                // Right: Key operational badges
                Row {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 14; spacing: 8

                    HeaderBadge { label: "执行状态"; value: appState.connected ? appState.execState : "--"
                        accent: appState.execState === "FAILSAFE" ? "#F85149" : appState.execState === "DISARMED" ? "#8B949E" : "#3FB950" }
                    HeaderBadge { label: "任务模式"; value: appState.connected ? appState.missionMode : "--"
                        accent: appState.missionMode === "EMERGENCY" ? "#F85149" : "#BC8CFF" }
                    HeaderBadge { label: "指令源"; value: appState.connected ? appState.activeCommandSource : "--"; accent: "#FFA657" }
                }
            }

            // ── Separator ──
            Rectangle { width: parent.width; height: 1; color: "#30363D" }

            // ── Row 2: System status — uniform "label: value" items with dot separators ──
            Item {
                width: parent.width; height: 35

                Row {
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 14; spacing: 0

                    StatusItem {
                        label: "解锁状态"; value: appState.armed ? "已解锁" : "未解锁"
                        dotColor: appState.armed ? "#F85149" : "#3FB950"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "电量[ V ]"; value: appState.connected ? Number(appState.batteryVoltage).toFixed(1) + " | " + Math.round(appState.batteryPercent * 100) + "%" : "--"
                        dotColor: !appState.connected ? "#8B949E" : appState.batteryPercent < 0.2 ? "#F85149" : appState.batteryPercent < 0.4 ? "#FFA657" : "#3FB950"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "定位源"; value: appState.connected ? appState.locationSource : "--"
                        dotColor: "#A5D6FF"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "GPS状态"; value: appState.connected ? appState.gpsStatus : "--"
                        dotColor: "#A5D6FF"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "心跳链路"; value: appState.heartbeatLinkState
                        dotColor: appState.heartbeatLinkState === "CONNECTED" ? "#3FB950" : "#F85149"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "协议连接"; value: appState.protocolConnected ? "已连接" : "未连接"
                        dotColor: appState.protocolConnected ? "#3FB950" : "#8B949E"
                    }
                    HeaderSep {}
                    StatusItem {
                        label: "保护触发"; value: appState.failsafe ? "true" : "false"
                        dotColor: appState.failsafe ? "#F85149" : "#3FB950"
                    }
                }
            }
        }
    }

    // ── Main Content ──
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 10
        radius: 12
        color: "#0D1117"
        border.color: "#21262D"

        Loader {
            anchors.fill: parent
            anchors.margins: 10
            sourceComponent: window.currentPage === 0 ? overviewPage
                             : window.currentPage === 1 ? scriptsPage
                             : paramsPage
        }
    }

    // ── HeaderBadge Component (Row 1) ──
    component HeaderBadge: Rectangle {
        property string label: ""
        property string value: ""
        property color accent: "#58A6FF"

        height: 36
        width: Math.max(82, labelTxt.width + valTxt.width + 20)
        radius: 7
        color: Qt.rgba(accent.r, accent.g, accent.b, 0.08)
        border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.20)

        Column {
            anchors.centerIn: parent
            spacing: 1
            Text {
                id: labelTxt; text: label
                color: Qt.rgba(accent.r, accent.g, accent.b, 0.70)
                font.pixelSize: 8; font.letterSpacing: 0.5
                horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                id: valTxt; text: value
                color: accent; font.pixelSize: 11; font.bold: true
                horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // ── StatusItem Component (Row 2) — "● label: value" format ──
    component StatusItem: Item {
        property string label: ""
        property string value: ""
        property color dotColor: "#58A6FF"

        width: statusRow.width; height: 35

        Row {
            id: statusRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            Rectangle {
                width: 7; height: 7; radius: 3.5
                color: dotColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: label + ":"; color: "#6E7681"; font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: value; color: "#C9D1D9"; font.pixelSize: 11; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ── HeaderSep — thin separator between Row 2 items ──
    component HeaderSep: Item {
        width: 18; height: 35
        Rectangle {
            anchors.centerIn: parent
            width: 1; height: 14; color: "#30363D"
        }
    }

    // ── Connection Dialog ──
    Dialog {
        id: connectionDialog
        modal: true
        width: 680
        height: 620
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        padding: 0
        closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside

        property var savedProfiles: []

        function refreshProfiles() { savedProfiles = appState.connectionProfiles() }

        Component.onCompleted: refreshProfiles()
        Connections {
            target: appState
            function onProfilesChanged() { connectionDialog.refreshProfiles() }
        }

        background: Rectangle { radius: 12; color: "#161B22"; border.color: "#30363D" }

        header: Rectangle {
            height: 48; radius: 12; color: "#161B22"
            Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 12; color: "#161B22" }
            Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 20; text: "连接设置"; color: "#E6EDF3"; font.pixelSize: 15; font.bold: true }
            Rectangle {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.rightMargin: 16
                width: 28; height: 28; radius: 6
                color: closeArea.containsMouse ? "#30363D" : "transparent"
                border.color: closeArea.containsMouse ? "#484F58" : "transparent"
                Text { anchors.centerIn: parent; text: "\u00D7"; color: "#8B949E"; font.pixelSize: 18 }
                MouseArea { id: closeArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: connectionDialog.close() }
            }
        }

        contentItem: Rectangle {
            color: "#161B22"

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                // ── Saved Profiles ──
                Rectangle {
                    width: parent.width; height: profileCol.height + 20; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Column {
                        id: profileCol
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 10; spacing: 6

                        Row {
                            spacing: 8
                            Text { text: "已保存配置"; color: "#8B949E"; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: connectionDialog.savedProfiles.length + " 个"; color: "#6E7681"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }

                        // Profile list (horizontal flow of clickable chips)
                        Flow {
                            width: parent.width; spacing: 6

                            Repeater {
                                model: connectionDialog.savedProfiles
                                delegate: Rectangle {
                                    width: chipRow.width + 12; height: 28; radius: 6
                                    color: chipMA.containsMouse ? "#30363D" : "#161B22"
                                    border.color: "#30363D"

                                    Row {
                                        id: chipRow; anchors.centerIn: parent; spacing: 6
                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter; spacing: 1
                                            Text { text: modelData.name; color: "#E6EDF3"; font.pixelSize: 11; font.bold: true }
                                            Text { text: modelData.ip + ":" + modelData.tcp; color: "#6E7681"; font.pixelSize: 9 }
                                        }
                                        // Delete button
                                        Text {
                                            text: "\u00D7"; color: "#6E7681"; font.pixelSize: 14
                                            anchors.verticalCenter: parent.verticalCenter
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: appState.deleteConnectionProfile(modelData.name)
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: chipMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            profileNameField.text = modelData.name
                                            hostField.text = modelData.ip
                                            udpField.text = String(modelData.udp)
                                            tcpField.text = String(modelData.tcp)
                                            heartbeatField.text = String(modelData.heartbeat)
                                        }
                                    }
                                }
                            }

                            // Empty hint
                            Text {
                                visible: connectionDialog.savedProfiles.length === 0
                                text: "暂无已保存配置，填写后点击「保存配置」"
                                color: "#484F58"; font.pixelSize: 11
                            }
                        }
                    }
                }

                // ── Connection fields ──
                Rectangle {
                    width: parent.width; height: 54; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 8
                        Column {
                            spacing: 3
                            Text { text: "配置名称"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: profileNameField; width: 120; height: 28
                                text: appState.vehicleName; placeholderText: "UAV1-Indoor"
                                color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "控制机 IP"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: hostField; width: 150; height: 28
                                text: appState.remoteHostIp; placeholderText: "192.168.1.x"
                                color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "UDP"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: udpField; width: 70; height: 28
                                text: String(appState.udpPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "TCP"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: tcpField; width: 70; height: 28
                                text: String(appState.tcpPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "Heartbeat"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: heartbeatField; width: 80; height: 28
                                text: String(appState.heartbeatPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                    }
                }

                // ── Buttons ──
                Row {
                    spacing: 8
                    PrimaryButton { width: 100; text: "保存配置"; fillColor: "#21262D"; textColor: "#58A6FF"
                        onClicked: {
                            var name = profileNameField.text.trim()
                            if (name.length === 0) name = appState.vehicleName
                            appState.saveConnectionProfile(name, hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text))
                        }
                    }
                    PrimaryButton { width: 100; text: "应用设置"; fillColor: "#21262D"; textColor: "#FFA657"
                        onClicked: appState.applyConnectionSettings(hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text))
                    }
                    PrimaryButton { width: 100; text: "连接测试"; fillColor: "#1F4E8C"
                        onClicked: appState.testProtocol() }
                    PrimaryButton { width: 100; text: "开始连接"; fillColor: "#1A6334"
                        onClicked: { appState.applyConnectionSettings(hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text)); appState.connectProtocol() } }
                    PrimaryButton { width: 100; text: "断开连接"; fillColor: "#6E1A1A"
                        onClicked: appState.disconnectProtocol() }
                }

                // ── Current status ──
                Rectangle {
                    width: parent.width; height: 70; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 5
                        Text { text: "当前配对: " + appState.vehicleName; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true }
                        Row {
                            spacing: 20
                            Text { text: "UDP: " + appState.udpLinkState; color: "#8B949E"; font.pixelSize: 12 }
                            Text { text: "TCP: " + appState.tcpLinkState; color: "#8B949E"; font.pixelSize: 12 }
                            Text { text: "HB: " + appState.heartbeatLinkState; color: "#8B949E"; font.pixelSize: 12 }
                        }
                        Text { text: appState.connectionSummary; color: "#6E7681"; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                    }
                }

                // ── Protocol log ──
                Rectangle {
                    width: parent.width; height: 200; radius: 8; color: "#0D1117"; border.color: "#30363D"
                    ScrollView {
                        anchors.fill: parent; anchors.margins: 8
                        TextArea {
                            readOnly: true; wrapMode: TextArea.Wrap
                            text: appState.protocolLogText; font.pixelSize: 11; font.family: "Monospace"; color: "#8B949E"; background: null
                            onTextChanged: cursorPosition = length
                        }
                    }
                }
            } // Column
        } // contentItem Rectangle
    } // Dialog

    Component { id: overviewPage; OverviewPage { } }
    Component { id: scriptsPage; ScriptsPage { } }
    Component { id: paramsPage; ParamsPage { } }
}
