import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ZenithUI 1.0

ApplicationWindow {
    id: window
    width: 1600
    height: 980
    visible: true
    title: "Zenith Ground Station"
    color: "#0D1117"

    property int currentPage: 0
    property var savedProfileNames: []

    // Helper: eliminate -0.0 display
    function fmt(val, decimals) {
        var s = Number(val).toFixed(decimals)
        return (s.charAt(0) === '-' && parseFloat(s) === 0) ? s.substring(1) : s
    }

    function updateProfileNames() {
        var profiles = appState.connectionProfiles()
        var names = []
        for (var i = 0; i < profiles.length; i++)
            names.push(profiles[i].name)
        savedProfileNames = names.length > 0 ? names : ["UAV1"]
    }

    Component.onCompleted: updateProfileNames()
    Connections {
        target: appState
        function onProfilesChanged() { updateProfileNames() }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════
        //  Header — single compact row (44px)
        // ══════════════════════════════════════
        Rectangle {
            id: headerBar
            width: parent.width; height: 44
            color: "#161B22"

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#30363D" }

            // ── Left: Brand + Vehicle + Connection ──
            Row {
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 56   // align past sidebar width
                spacing: 10

                Text {
                    text: "ZENITH"; color: "#58A6FF"
                    font.pixelSize: 16; font.bold: true; font.letterSpacing: 3
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle { width: 1; height: 22; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }

                // Protocol indicator
                Rectangle {
                    width: protoLabel.width + 16; height: 24; radius: 4
                    color: "#21262D"; border.color: "#30363D"
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: protoLabel; anchors.centerIn: parent
                        text: appState.protocolClient.transportMode === 1 ? "Serial" : "TCP"
                        color: "#8B949E"; font.pixelSize: 10; font.bold: true
                    }
                }

                // Connection address
                Rectangle {
                    width: connAddr.width + 16; height: 24; radius: 4
                    color: "#0D1117"; border.color: "#30363D"
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: connAddr; anchors.centerIn: parent
                        text: appState.protocolClient.transportMode === 1
                              ? appState.protocolClient.serialPortName + " @ " + appState.protocolClient.serialBaudRate
                              : appState.remoteHostIp + ":" + appState.tcpPort
                        color: "#8B949E"; font.pixelSize: 11; font.family: "Consolas"
                    }
                }

                // Connect / Disconnect button
                Rectangle {
                    width: connBtnLabel.width + 24; height: 26; radius: 5
                    color: appState.protocolConnected ? "#6E1A1A" : "#1A6334"
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: connBtnLabel; anchors.centerIn: parent
                        text: appState.protocolConnected ? "Disconnect" : "Connect"
                        color: "#FFFFFF"; font.pixelSize: 11; font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: appState.protocolConnected ? appState.disconnectProtocol() : appState.connectProtocol()
                    }
                }

                // Vehicle selector (from saved profiles)
                ComboBox {
                    id: vehicleCombo; width: Math.max(80, contentItem.implicitWidth + 28); height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    model: savedProfileNames; currentIndex: 0
                    onActivated: {
                        var name = currentText
                        appState.selectVehicle(name)
                        var profile = appState.loadConnectionProfile(name)
                        if (profile && profile.name) {
                            if (profile.type === "serial") {
                                appState.applySerialSettings(profile.portName, profile.baudRate)
                            } else {
                                appState.applyConnectionSettings(profile.ip, profile.udp, profile.tcp, profile.heartbeat)
                            }
                        }
                    }
                    background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
                    contentItem: Text { leftPadding: 8; text: vehicleCombo.currentText; color: "#E6EDF3"; font.pixelSize: 11; font.bold: true; verticalAlignment: Text.AlignVCenter }
                    popup: Popup {
                        y: vehicleCombo.height; width: vehicleCombo.width
                        implicitHeight: contentItem.implicitHeight + 2; padding: 1
                        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: vehicleCombo.popup.visible ? vehicleCombo.delegateModel : null; ScrollIndicator.vertical: ScrollIndicator { } }
                        background: Rectangle { radius: 6; color: "#21262D"; border.color: "#30363D" }
                    }
                    delegate: ItemDelegate {
                        width: vehicleCombo.width; height: 26
                        contentItem: Text { text: modelData; color: "#E6EDF3"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: hovered ? "#30363D" : "transparent" }
                    }
                }

                // Gear → open connection dialog
                Rectangle {
                    width: 26; height: 26; radius: 5
                    color: gearMA.containsMouse ? "#30363D" : "#21262D"
                    border.color: "#30363D"
                    anchors.verticalCenter: parent.verticalCenter
                    Text { anchors.centerIn: parent; text: "\u2699"; font.pixelSize: 14; color: "#8B949E" }
                    MouseArea { id: gearMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: connectionDialog.open() }
                }
            }

            // ── Right: Mode + Armed + Status dots + Metrics ──
            Row {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 14; spacing: 10

                // Mode badge (PX4 flight mode)
                HeaderBadge {
                    label: "Mode"
                    value: appState.connected ? appState.flightMode : "--"
                    accent: appState.failsafe ? "#F85149" : appState.flightMode === "OFFBOARD" ? "#3FB950" : "#FFA657"
                }

                // Armed badge
                Rectangle {
                    width: armedLabel.width + 16; height: 24; radius: 4
                    color: appState.armed ? Qt.rgba(0.973, 0.318, 0.286, 0.12) : Qt.rgba(0.247, 0.725, 0.314, 0.12)
                    border.color: appState.armed ? Qt.rgba(0.973, 0.318, 0.286, 0.30) : Qt.rgba(0.247, 0.725, 0.314, 0.30)
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: armedLabel; anchors.centerIn: parent
                        text: appState.armed ? "ARMED" : "DISARMED"
                        color: appState.armed ? "#F85149" : "#3FB950"
                        font.pixelSize: 10; font.bold: true
                    }
                }

                Rectangle { width: 1; height: 20; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }

                // Status dots: Connection / GPS / Battery
                Row {
                    anchors.verticalCenter: parent.verticalCenter; spacing: 10
                    StatusDot { dotColor: appState.protocolConnected ? "#3FB950" : "#F85149"; label: "Link" }
                    StatusDot { dotColor: appState.connected && appState.gpsStatus.indexOf("3D") >= 0 ? "#3FB950" : appState.connected && appState.gpsStatus.indexOf("2D") >= 0 ? "#FFA657" : "#F85149"; label: "GPS" }
                    StatusDot { dotColor: !appState.connected ? "#484F58" : appState.batteryPercent < 0.2 ? "#F85149" : appState.batteryPercent < 0.4 ? "#FFA657" : "#3FB950"; label: "Bat" }
                }

                Rectangle { width: 1; height: 20; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }

                // Battery voltage + percent
                Text {
                    text: appState.connected ? fmt(appState.batteryVoltage, 1) + "V" : "--"
                    color: "#C9D1D9"; font.pixelSize: 13; font.family: "Consolas"; font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: appState.connected ? Math.round(appState.batteryPercent * 100) + "%" : "--"
                    color: appState.batteryPercent < 0.2 ? "#F85149" : "#C9D1D9"
                    font.pixelSize: 13; font.family: "Consolas"; font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // ══════════════════════════════════════
        //  Body: Global Sidebar + Content
        // ══════════════════════════════════════
        Row {
            width: parent.width
            height: parent.height - 44
            spacing: 0

            // ── Global Navigation Sidebar (44px) ──
            Rectangle {
                id: globalSidebar
                width: 44; height: parent.height
                color: "#161B22"

                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#30363D" }

                Column {
                    anchors.top: parent.top; anchors.topMargin: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 2

                    NavIcon { icon: "\u229E"; label: "概览";  active: currentPage === 0; onNav: currentPage = 0 }
                    NavIcon { icon: "\u2B21"; label: "模块";  active: currentPage === 1; onNav: currentPage = 1 }
                    NavIcon { icon: "\u27E8\u27E9"; label: "脚本"; active: currentPage === 2; onNav: currentPage = 2 }
                    NavIcon { icon: "\u2261"; label: "参数";  active: currentPage === 3; onNav: currentPage = 3 }
                }

                // Settings at bottom
                NavIcon {
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    icon: "\u2699"; label: "设置"; active: false
                    onNav: connectionDialog.open()
                }
            }

            // ── Content Area ──
            Rectangle {
                width: parent.width - 44; height: parent.height
                color: "#0D1117"

                Loader {
                    id: pageLoader
                    anchors.fill: parent
                    anchors.margins: 6
                    sourceComponent: window.currentPage === 0 ? overviewPage
                                     : window.currentPage === 1 ? modulesPage
                                     : window.currentPage === 2 ? scriptsPage
                                     : paramsPage
                    onSourceComponentChanged: pageLoader.opacity = 0.0
                    onLoaded: pageLoader.opacity = 1.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }
    }

    // ══════════════════════════════════════
    //  Inline Components
    // ══════════════════════════════════════

    component HeaderBadge: Rectangle {
        property string label: ""
        property string value: ""
        property color accent: "#58A6FF"

        height: 32; width: Math.max(70, badgeLbl.width + badgeVal.width + 18)
        radius: 6; anchors.verticalCenter: parent.verticalCenter
        color: Qt.rgba(accent.r, accent.g, accent.b, 0.08)
        border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.20)

        Column {
            anchors.centerIn: parent; spacing: 1
            Text { id: badgeLbl; text: label; color: Qt.rgba(accent.r, accent.g, accent.b, 0.65); font.pixelSize: 7; font.letterSpacing: 0.5; anchors.horizontalCenter: parent.horizontalCenter }
            Text { id: badgeVal; text: value; color: accent; font.pixelSize: 11; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }

    component StatusDot: Row {
        property color dotColor: "#3FB950"
        property string label: ""
        spacing: 5; anchors.verticalCenter: parent.verticalCenter
        Rectangle { width: 8; height: 8; radius: 4; color: dotColor; anchors.verticalCenter: parent.verticalCenter }
        Text { text: label; color: "#6E7681"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
    }

    component NavIcon: Item {
        property string icon: ""
        property string label: ""
        property bool active: false
        signal nav()

        width: 40; height: 46

        // Active left bar
        Rectangle {
            visible: active
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            width: 3; height: 22; radius: 1.5; color: "#1F6FEB"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 36; height: 42; radius: 8
            color: active ? Qt.rgba(0.122, 0.435, 0.922, 0.12) : (navMA.containsMouse ? "#21262D" : "transparent")
            Behavior on color { ColorAnimation { duration: 120 } }

            Column {
                anchors.centerIn: parent; spacing: 2
                Text { text: icon; font.pixelSize: 15; color: active ? "#58A6FF" : "#8B949E"; anchors.horizontalCenter: parent.horizontalCenter; Behavior on color { ColorAnimation { duration: 120 } } }
                Text { text: label; font.pixelSize: 8; color: active ? "#E6EDF3" : "#6E7681"; anchors.horizontalCenter: parent.horizontalCenter; Behavior on color { ColorAnimation { duration: 120 } } }
            }

            MouseArea { id: navMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: parent.parent.nav() }
        }
    }

    // ══════════════════════════════════════
    //  Connection Dialog (TCP + Serial)
    // ══════════════════════════════════════
    Dialog {
        id: connectionDialog
        modal: true
        width: 680
        height: 660
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        padding: 0
        closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside

        property var savedProfiles: []
        property int selectedTransportMode: 0  // 0=TCP, 1=Serial

        function refreshProfiles() { savedProfiles = appState.connectionProfiles() }

        Component.onCompleted: {
            refreshProfiles()
            appState.protocolClient.refreshSerialPorts()
        }
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

                        Flow {
                            width: parent.width; spacing: 6

                            Repeater {
                                model: connectionDialog.savedProfiles
                                delegate: Rectangle {
                                    width: chipRow.width + 12; height: 28; radius: 6
                                    color: chipMA.containsMouse ? "#30363D" : "#161B22"
                                    border.color: "#30363D"

                                    MouseArea {
                                        id: chipMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            profileNameField.text = modelData.name
                                            if (modelData.type === "serial") {
                                                connectionDialog.selectedTransportMode = 1
                                                // Set serial port ComboBox to matching port
                                                var ports = appState.protocolClient.availableSerialPorts
                                                var idx = -1
                                                for (var i = 0; i < ports.length; i++) {
                                                    if (ports[i] === modelData.portName) { idx = i; break }
                                                }
                                                if (idx >= 0) serialPortCombo.currentIndex = idx
                                                // Set baud rate ComboBox to matching rate
                                                var bauds = [9600, 19200, 38400, 57600, 115200, 460800, 921600]
                                                for (var j = 0; j < bauds.length; j++) {
                                                    if (bauds[j] === modelData.baudRate) { baudRateCombo.currentIndex = j; break }
                                                }
                                            } else {
                                                connectionDialog.selectedTransportMode = 0
                                                hostField.text = modelData.ip
                                                udpField.text = String(modelData.udp)
                                                tcpField.text = String(modelData.tcp)
                                                heartbeatField.text = String(modelData.heartbeat)
                                            }
                                        }
                                    }

                                    Row {
                                        id: chipRow; anchors.centerIn: parent; spacing: 6
                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter; spacing: 1
                                            Text { text: modelData.name; color: "#E6EDF3"; font.pixelSize: 11; font.bold: true }
                                            Text {
                                                text: modelData.type === "serial"
                                                      ? modelData.portName + " @ " + modelData.baudRate
                                                      : modelData.ip + ":" + modelData.tcp
                                                color: "#6E7681"; font.pixelSize: 9
                                            }
                                        }
                                        Rectangle {
                                            width: 18; height: 18; radius: 4
                                            color: delMA.containsMouse ? "#F8514933" : "transparent"
                                            anchors.verticalCenter: parent.verticalCenter
                                            Text { anchors.centerIn: parent; text: "\u00D7"; color: delMA.containsMouse ? "#F85149" : "#6E7681"; font.pixelSize: 14 }
                                            MouseArea { id: delMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: appState.deleteConnectionProfile(modelData.name) }
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: connectionDialog.savedProfiles.length === 0
                                text: "暂无已保存配置，填写后点击「保存配置」"
                                color: "#484F58"; font.pixelSize: 11
                            }
                        }
                    }
                }

                // ── Transport Mode Selector ──
                Rectangle {
                    width: 240; height: 32; radius: 6
                    color: "#21262D"; border.color: "#30363D"

                    Row {
                        anchors.fill: parent; anchors.margins: 2; spacing: 2

                        Rectangle {
                            width: (parent.width - 2) / 2; height: parent.height; radius: 4
                            color: connectionDialog.selectedTransportMode === 0 ? "#1F6FEB" : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "TCP 网络"
                                color: connectionDialog.selectedTransportMode === 0 ? "#FFFFFF" : "#8B949E"
                                font.pixelSize: 12; font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: connectionDialog.selectedTransportMode = 0
                            }
                        }

                        Rectangle {
                            width: (parent.width - 2) / 2; height: parent.height; radius: 4
                            color: connectionDialog.selectedTransportMode === 1 ? "#1F6FEB" : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "串口数传"
                                color: connectionDialog.selectedTransportMode === 1 ? "#FFFFFF" : "#8B949E"
                                font.pixelSize: 12; font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    connectionDialog.selectedTransportMode = 1
                                    appState.protocolClient.refreshSerialPorts()
                                }
                            }
                        }
                    }
                }

                // ── Config Name (shared between both modes) ──
                Rectangle {
                    width: parent.width; height: 54; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 8
                        Column {
                            spacing: 3
                            Text { text: "配置名称"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: profileNameField; width: 200; height: 28
                                text: appState.vehicleName; placeholderText: "UAV1-Indoor"
                                color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                    }
                }

                // ── TCP Connection fields ──
                Rectangle {
                    visible: connectionDialog.selectedTransportMode === 0
                    width: parent.width; height: 54; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 8
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
                                id: udpField; width: 90; height: 28
                                text: String(appState.udpPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "TCP"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: tcpField; width: 90; height: 28
                                text: String(appState.tcpPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "Heartbeat"; color: "#8B949E"; font.pixelSize: 10 }
                            TextField {
                                id: heartbeatField; width: 90; height: 28
                                text: String(appState.heartbeatPort); color: "#E6EDF3"; font.pixelSize: 13
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                            }
                        }
                    }
                }

                // ── Serial Connection fields ──
                Rectangle {
                    visible: connectionDialog.selectedTransportMode === 1
                    width: parent.width; height: 54; radius: 8; color: "#21262D"; border.color: "#30363D"
                    Row {
                        anchors.fill: parent; anchors.margins: 10; spacing: 8
                        Column {
                            spacing: 3
                            Text { text: "串口"; color: "#8B949E"; font.pixelSize: 10 }
                            Row {
                                spacing: 6
                                ComboBox {
                                    id: serialPortCombo; width: 160; height: 28
                                    model: appState.protocolClient.availableSerialPorts
                                    background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                                    contentItem: Text {
                                        leftPadding: 8; text: serialPortCombo.currentText
                                        color: "#E6EDF3"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
                                    }
                                    popup: Popup {
                                        y: serialPortCombo.height; width: serialPortCombo.width
                                        implicitHeight: contentItem.implicitHeight + 2
                                        padding: 1
                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: serialPortCombo.popup.visible ? serialPortCombo.delegateModel : null
                                            ScrollIndicator.vertical: ScrollIndicator { }
                                        }
                                        background: Rectangle { radius: 6; color: "#21262D"; border.color: "#30363D" }
                                    }
                                    delegate: ItemDelegate {
                                        width: serialPortCombo.width; height: 28
                                        contentItem: Text { text: modelData; color: "#E6EDF3"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: hovered ? "#30363D" : "transparent" }
                                    }
                                }
                                Rectangle {
                                    width: 28; height: 28; radius: 6
                                    color: refreshMA.containsMouse ? "#30363D" : "#0D1117"
                                    border.color: "#30363D"
                                    Text { anchors.centerIn: parent; text: "\u21BB"; color: "#8B949E"; font.pixelSize: 14 }
                                    MouseArea {
                                        id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: appState.protocolClient.refreshSerialPorts()
                                    }
                                }
                            }
                        }
                        Column {
                            spacing: 3
                            Text { text: "波特率"; color: "#8B949E"; font.pixelSize: 10 }
                            ComboBox {
                                id: baudRateCombo; width: 120; height: 28
                                model: [9600, 19200, 38400, 57600, 115200, 460800, 921600]
                                currentIndex: 6  // default 921600
                                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
                                contentItem: Text {
                                    leftPadding: 8; text: baudRateCombo.currentText
                                    color: "#E6EDF3"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
                                }
                                popup: Popup {
                                    y: baudRateCombo.height; width: baudRateCombo.width
                                    implicitHeight: contentItem.implicitHeight + 2
                                    padding: 1
                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: baudRateCombo.popup.visible ? baudRateCombo.delegateModel : null
                                        ScrollIndicator.vertical: ScrollIndicator { }
                                    }
                                    background: Rectangle { radius: 6; color: "#21262D"; border.color: "#30363D" }
                                }
                                delegate: ItemDelegate {
                                    width: baudRateCombo.width; height: 28
                                    contentItem: Text { text: modelData; color: "#E6EDF3"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: hovered ? "#30363D" : "transparent" }
                                }
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
                            if (connectionDialog.selectedTransportMode === 1) {
                                appState.saveSerialConnectionProfile(name, serialPortCombo.currentText, Number(baudRateCombo.currentText))
                            } else {
                                appState.saveConnectionProfile(name, hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text))
                            }
                        }
                    }
                    PrimaryButton { width: 100; text: "应用设置"; fillColor: "#21262D"; textColor: "#FFA657"
                        onClicked: {
                            if (connectionDialog.selectedTransportMode === 1) {
                                appState.applySerialSettings(serialPortCombo.currentText, Number(baudRateCombo.currentText))
                            } else {
                                appState.applyConnectionSettings(hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text))
                            }
                        }
                    }
                    PrimaryButton { width: 100; text: "连接测试"; fillColor: "#1F4E8C"
                        enabled: connectionDialog.selectedTransportMode === 0
                        onClicked: appState.testProtocol()
                    }
                    PrimaryButton { width: 100; text: "开始连接"; fillColor: "#1A6334"
                        onClicked: {
                            if (connectionDialog.selectedTransportMode === 1) {
                                appState.applySerialSettings(serialPortCombo.currentText, Number(baudRateCombo.currentText))
                            } else {
                                appState.applyConnectionSettings(hostField.text, Number(udpField.text), Number(tcpField.text), Number(heartbeatField.text))
                            }
                            appState.connectProtocol()
                        }
                    }
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
                    width: parent.width; height: 160; radius: 8; color: "#0D1117"; border.color: "#30363D"
                    ScrollView {
                        anchors.fill: parent; anchors.margins: 8
                        TextArea {
                            readOnly: true; wrapMode: TextArea.Wrap
                            text: appState.protocolLogText; font.pixelSize: 11; font.family: "Monospace"; color: "#8B949E"; background: null
                            onTextChanged: cursorPosition = length
                        }
                    }
                }
            }
        }
    }

    Component { id: overviewPage; OverviewPage { } }
    Component { id: modulesPage; ModulesPage { } }
    Component { id: scriptsPage; ScriptsPage { } }
    Component { id: paramsPage; ParamsPage { } }
}
