import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1600
    height: 980
    visible: true
    title: "Zenith Ground Station"
    color: "#EEF3F9"

    property int currentPage: 0

    header: Rectangle {
        height: 122
        color: "#F7FAFD"
        border.color: "#D9E3F0"

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Row {
                width: parent.width
                spacing: 16

                Text {
                    text: "Zenith"
                    color: "#1B3A8C"
                    font.pixelSize: 32
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Autonomous UAV Software Platform"
                    color: "#E25544"
                    font.pixelSize: 13
                    font.bold: true
                }

                ComboBox {
                    width: 110
                    model: ["UAV1", "UAV2", "UAV3"]
                    currentIndex: 0
                    onActivated: appState.selectVehicle(currentText)
                }

                Button {
                    text: "连接设置"
                    implicitWidth: 108
                    implicitHeight: 34
                    onClicked: connectionDialog.open()

                    background: Rectangle {
                        radius: 8
                        color: "#FFFFFF"
                        border.color: "#D9E3F0"
                    }

                    contentItem: Text {
                        text: parent.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#314256"
                        font.pixelSize: 13
                        font.bold: true
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: appState.protocolConnected ? "链路: 已连接" : "链路: 未连接"
                    color: appState.protocolConnected ? "#1D9B5F" : "#D83A3A"
                    font.pixelSize: 12
                    font.bold: true
                }

                Flow {
                    width: 760
                    spacing: 12

                    Text { text: "飞行状态: " + appState.flightStatus; color: appState.connected ? "#2B3A4A" : "#D83A3A"; font.pixelSize: 12; font.bold: !appState.connected }
                    Text { text: "飞行模式: " + appState.flightMode; color: "#2B3A4A"; font.pixelSize: 12 }
                    Text { text: "控制器: " + appState.controllerMode; color: "#2B3A4A"; font.pixelSize: 12 }
                    Text { text: "控制状态: " + appState.controlState; color: "#D83A3A"; font.pixelSize: 12; font.bold: true }
                    Text { text: "定位源: " + appState.locationSource; color: "#D83A3A"; font.pixelSize: 12; font.bold: true }
                    Text { text: "GPS: " + appState.gpsStatus; color: "#2B3A4A"; font.pixelSize: 12 }
                    Text { text: "解锁: " + (appState.armed ? "已解锁" : "未解锁"); color: "#D83A3A"; font.pixelSize: 12; font.bold: true }
                    Text { text: "电量: " + Number(appState.batteryVoltage).toFixed(1) + "V / " + Number(appState.batteryPercent * 100).toFixed(0) + "%"; color: "#D83A3A"; font.pixelSize: 12; font.bold: true }
                    Text { text: "RC: " + appState.rcLink; color: "#2B3A4A"; font.pixelSize: 12 }
                    Text { text: "Video: " + appState.videoLink; color: "#2B3A4A"; font.pixelSize: 12 }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: appState.currentTime
                    color: "#6E8095"
                    font.pixelSize: 12
                }
            }

            Row {
                spacing: 10

                Repeater {
                    model: [
                        { label: "Overview", page: 0 },
                        { label: "Map / Track", page: 1 },
                        { label: "Scripts", page: 2 }
                    ]

                    delegate: Button {
                        text: modelData.label
                        implicitWidth: 126
                        implicitHeight: 34
                        onClicked: window.currentPage = modelData.page

                        background: Rectangle {
                            radius: 10
                            color: window.currentPage === modelData.page ? "#2F6BFF" : "#FFFFFF"
                            border.color: window.currentPage === modelData.page ? "#2F6BFF" : "#D9E3F0"
                        }

                        contentItem: Text {
                            text: parent.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: window.currentPage === modelData.page ? "#FFFFFF" : "#314256"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }

                Item { width: 24; height: 1 }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Mission: " + appState.missionStage
                    color: "#3D5065"
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: appState.connectionSummary
                    color: "#6E8095"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    width: 760
                }
            }
        }
    }

    footer: Rectangle {
        height: 34
        color: "#F7FAFD"
        border.color: "#D9E3F0"

        Row {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 20

            Text { text: "Vehicle: " + appState.vehicleName; color: "#4B5D73"; font.pixelSize: 12 }
            Text { text: "Battery: " + Number(appState.batteryPercent * 100).toFixed(0) + "%"; color: "#4B5D73"; font.pixelSize: 12 }
            Text { text: "Altitude: " + Number(appState.altitude).toFixed(1) + " m"; color: "#4B5D73"; font.pixelSize: 12 }
            Text { text: "UDP: " + appState.udpLinkState; color: "#4B5D73"; font.pixelSize: 12 }
            Text { text: "TCP: " + appState.tcpLinkState; color: "#4B5D73"; font.pixelSize: 12 }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.margins: 14
        radius: 20
        color: "#FFFFFF"
        border.color: "#D9E3F0"

        Loader {
            anchors.fill: parent
            anchors.margins: 16
            sourceComponent: window.currentPage === 0 ? overviewPage
                             : window.currentPage === 1 ? missionPage
                             : scriptsPage
        }
    }

    Dialog {
        id: connectionDialog
        modal: true
        width: 620
        height: 560
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        title: "连接设置"
        standardButtons: Dialog.Close

        background: Rectangle {
            radius: 10
            color: "#FFFFFF"
            border.color: "#D9E3F0"
        }

        contentItem: Column {
            spacing: 12

            Row {
                spacing: 10
                TextField { id: hostField; width: 220; text: appState.remoteHostIp; placeholderText: "无人机控制机 IP" }
                TextField { id: udpField; width: 100; text: String(appState.udpPort); placeholderText: "UDP" }
                TextField { id: tcpField; width: 100; text: String(appState.tcpPort); placeholderText: "TCP" }
                TextField { id: heartbeatField; width: 120; text: String(appState.heartbeatPort); placeholderText: "Heartbeat" }
            }

            Row {
                spacing: 10

                PrimaryButton {
                    width: 120
                    text: "保存设置"
                    onClicked: appState.applyConnectionSettings(hostField.text,
                                                                Number(udpField.text),
                                                                Number(tcpField.text),
                                                                Number(heartbeatField.text))
                }

                PrimaryButton {
                    width: 120
                    text: "连接测试"
                    fillColor: "#2A66C9"
                    onClicked: appState.testProtocol()
                }

                PrimaryButton {
                    width: 120
                    text: "开始连接"
                    fillColor: "#1D9B5F"
                    onClicked: {
                        appState.applyConnectionSettings(hostField.text,
                                                         Number(udpField.text),
                                                         Number(tcpField.text),
                                                         Number(heartbeatField.text))
                        appState.connectProtocol()
                    }
                }

                PrimaryButton {
                    width: 120
                    text: "断开连接"
                    fillColor: "#D14343"
                    onClicked: appState.disconnectProtocol()
                }
            }

            Rectangle {
                width: 580
                height: 108
                radius: 8
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    Text { text: "当前配对载具: " + appState.vehicleName; color: "#17212E"; font.pixelSize: 13; font.bold: true }
                    Text { text: "UDP: " + appState.udpLinkState; color: "#425469"; font.pixelSize: 12 }
                    Text { text: "TCP: " + appState.tcpLinkState; color: "#425469"; font.pixelSize: 12 }
                    Text { text: "Heartbeat: " + appState.heartbeatLinkState; color: "#425469"; font.pixelSize: 12 }
                    Text { text: "摘要: " + appState.connectionSummary; color: "#6E8095"; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                }
            }

            Text {
                text: "配对逻辑: 地面站监听 UDP " + appState.udpPort
                      + " 和 Heartbeat " + appState.heartbeatPort
                      + "，并向 " + appState.remoteHostIp + ":" + appState.tcpPort
                      + " 发送 MODESELECTION 与控制命令。"
                color: "#596B81"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                width: 580
            }

            Rectangle {
                width: 580
                height: 240
                radius: 8
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                TextArea {
                    anchors.fill: parent
                    anchors.margins: 8
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    text: appState.protocolLogText
                    font.pixelSize: 12
                    color: "#233042"
                    background: null
                }
            }
        }
    }

    Component { id: overviewPage; OverviewPage { } }
    Component { id: missionPage; MissionPage { } }
    Component { id: scriptsPage; ScriptsPage { } }
}
