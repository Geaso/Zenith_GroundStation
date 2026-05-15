import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

Item {
    id: root

    // ── Module definitions ──
    readonly property var moduleList: [
        { name: "ODIN",       label: "ODIN Lidar",     node: "odin1",                    startKey: "ODIN",          color: "#1F6FEB" },
        { name: "OAK_VIO",    label: "OAK VIO",        node: "oakchina_vio",             startKey: "OAK_VIO",       color: "#8B5CF6" },
        { name: "D435i",      label: "RealSense D435i", node: "realsense2_camera",       startKey: "D435i",         color: "#06B6D4" },
        { name: "Control",    label: "Zenith Control",  node: "uav_control_main",         startKey: "",              color: "#F59E0B" }
    ]

    readonly property var profileList: [
        { name: "Indoor ODIN",      steps: ["ODIN", "Zenith_ODIN"],     color: "#1F6FEB", desc: "ODIN Lidar → Zenith Control (odin profile)" },
        { name: "Indoor OAK VIO",   steps: ["OAK_VIO", "Zenith_OAKVIO"], color: "#8B5CF6", desc: "OAK VIO → Zenith Control (vins profile)" },
        { name: "Indoor OpenVINS",  steps: ["D435i", "Zenith_OPENVINS"], color: "#06B6D4", desc: "D435i → Zenith Control (openvins profile)" }
    ]

    property int runningProfileStep: -1
    property var currentProfileSteps: []
    property string currentProfileName: ""

    Row {
        anchors.fill: parent
        spacing: 0

        // ═══════════════════════════════════════
        //  Left: Module Status Cards (flexible)
        // ═══════════════════════════════════════
        Column {
            width: parent.width - 280 - 8
            height: parent.height
            spacing: 10

            // ── Module Status ──
            Text { text: "模块状态"; color: "#8B949E"; font.pixelSize: 12; font.bold: true }

            Grid {
                width: parent.width
                columns: 2
                spacing: 8

                Repeater {
                    model: moduleList
                    delegate: Rectangle {
                        width: (parent.width - 8) / 2
                        height: 72
                        radius: 8
                        color: "#161B22"
                        border.color: "#30363D"

                        property bool running: appState.isModuleRunning(modelData.node)

                        Row {
                            anchors.fill: parent; anchors.margins: 12; spacing: 10

                            // Status dot
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: running ? "#3FB950" : "#484F58"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Info
                            Column {
                                anchors.verticalCenter: parent.verticalCenter; spacing: 3
                                width: parent.width - 110
                                Text { text: modelData.label; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true }
                                Text {
                                    text: running ? "运行中" : "未启动"
                                    color: running ? "#3FB950" : "#6E7681"
                                    font.pixelSize: 10
                                }
                            }

                            // Buttons
                            Row {
                                anchors.verticalCenter: parent.verticalCenter; spacing: 4
                                ModuleBtn {
                                    text: "启动"; btnColor: modelData.color
                                    visible: modelData.startKey.length > 0
                                    enabled: !running && appState.connected
                                    onClicked: appState.startModule(modelData.startKey)
                                }
                                ModuleBtn {
                                    text: "停止"; btnColor: "#6E1A1A"
                                    enabled: running && appState.connected
                                    onClicked: appState.stopModule(modelData.name)
                                }
                            }
                        }
                    }
                }
            }

            // ── Separator ──
            Rectangle { width: parent.width; height: 1; color: "#262C36" }

            // ── Quick Launch Profiles ──
            Text { text: "快速启动配置"; color: "#8B949E"; font.pixelSize: 12; font.bold: true }

            Column {
                width: parent.width; spacing: 6

                Repeater {
                    model: profileList
                    delegate: Rectangle {
                        width: parent.width; height: 56; radius: 8
                        color: profileMA.containsMouse ? "#1C2333" : "#161B22"
                        border.color: "#30363D"

                        MouseArea {
                            id: profileMA; anchors.fill: parent; hoverEnabled: true
                        }

                        Row {
                            anchors.fill: parent; anchors.margins: 12; spacing: 10

                            Rectangle {
                                width: 4; height: 32; radius: 2; color: modelData.color
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter; spacing: 2
                                width: parent.width - 120
                                Text { text: modelData.name; color: "#E6EDF3"; font.pixelSize: 12; font.bold: true }
                                Text { text: modelData.desc; color: "#6E7681"; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                            }

                            PrimaryButton {
                                width: 80; height: 30; text: "一键启动"
                                fillColor: modelData.color
                                anchors.verticalCenter: parent.verticalCenter
                                enabled: appState.connected && runningProfileStep < 0
                                onClicked: launchProfile(modelData.name, modelData.steps)
                            }
                        }
                    }
                }
            }

            // ── Profile launch status ──
            Rectangle {
                width: parent.width; height: 32; radius: 6
                color: "#0D1117"; border.color: "#21262D"
                visible: currentProfileName.length > 0

                Text {
                    anchors.centerIn: parent
                    text: runningProfileStep >= 0
                          ? currentProfileName + " - 步骤 " + (runningProfileStep + 1) + "/" + currentProfileSteps.length + " 执行中..."
                          : currentProfileName + " - 完成"
                    color: runningProfileStep >= 0 ? "#FFA657" : "#3FB950"
                    font.pixelSize: 11
                }
            }

            // ── Separator ──
            Rectangle { width: parent.width; height: 1; color: "#262C36" }

            // ── Custom Command ──
            Text { text: "自定义远程命令"; color: "#8B949E"; font.pixelSize: 12; font.bold: true }

            Row {
                width: parent.width; spacing: 6
                TextField {
                    id: customCmdField; width: parent.width - 86; height: 32
                    placeholderText: "/home/jetson/Zenith_ws/scripts/modules/..."
                    color: "#E6EDF3"; font.pixelSize: 11; placeholderTextColor: "#484F58"
                    background: Rectangle { radius: 6; color: "#21262D"; border.color: "#30363D" }
                }
                PrimaryButton {
                    width: 80; height: 32; text: "执行"; fillColor: "#1F6FEB"
                    enabled: appState.connected && customCmdField.text.length > 0
                    onClicked: {
                        appState.executeRemoteCommand("Custom", customCmdField.text)
                        customCmdField.text = ""
                    }
                }
            }

            // ── Exec feedback ──
            Rectangle {
                width: parent.width
                height: parent.height - parent.childrenRect.height + height
                radius: 8; color: "#0D1117"; border.color: "#21262D"
                clip: true

                Column {
                    anchors.fill: parent; anchors.margins: 10; spacing: 4
                    Text { text: "执行反馈"; color: "#8B949E"; font.pixelSize: 10; font.bold: true }
                    Rectangle { width: parent.width; height: 1; color: "#262C36" }
                    Text {
                        width: parent.width
                        text: appState.moduleExecFeedback.length > 0 ? appState.moduleExecFeedback : "等待命令..."
                        color: {
                            var fb = appState.moduleExecFeedback
                            if (fb.indexOf("OK") >= 0) return "#3FB950"
                            if (fb.indexOf("FAIL") >= 0 || fb.indexOf("REJECTED") >= 0) return "#F85149"
                            if (fb.indexOf("START") >= 0) return "#FFA657"
                            return "#6E7681"
                        }
                        font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { width: 8; height: 1 }

        // ═══════════════════════════════════════
        //  Right: Running Nodes List (280px)
        // ═══════════════════════════════════════
        Rectangle {
            width: 280; height: parent.height; radius: 10
            color: "#161B22"; border.color: "#30363D"

            Column {
                anchors.fill: parent; anchors.margins: 12; spacing: 8

                Text { text: "ROS 节点列表"; color: "#8B949E"; font.pixelSize: 12; font.bold: true }

                Text {
                    text: appState.runningNodes.length > 0
                          ? appState.runningNodes.length + " 个节点运行中"
                          : "等待 heartbeat..."
                    color: "#6E7681"; font.pixelSize: 10
                }

                Rectangle { width: parent.width; height: 1; color: "#262C36" }

                ListView {
                    width: parent.width
                    height: parent.height - 60
                    model: appState.runningNodes
                    spacing: 2
                    clip: true

                    delegate: Rectangle {
                        width: ListView.view.width; height: 24; radius: 4
                        color: index % 2 === 0 ? "#0D1117" : "transparent"

                        Row {
                            anchors.fill: parent; anchors.leftMargin: 8; spacing: 6
                            Rectangle {
                                width: 6; height: 6; radius: 3; color: "#3FB950"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData; color: "#C9D1D9"; font.pixelSize: 10
                                font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideMiddle; width: parent.width - 20
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════
    //  Profile launch sequence
    // ═══════════════════════════════════════
    Timer {
        id: profileTimer; interval: 8000; repeat: false
        onTriggered: {
            runningProfileStep++
            if (runningProfileStep < currentProfileSteps.length) {
                appState.startModule(currentProfileSteps[runningProfileStep])
                profileTimer.start()
            } else {
                runningProfileStep = -1
            }
        }
    }

    function launchProfile(name, steps) {
        currentProfileName = name
        currentProfileSteps = steps
        runningProfileStep = 0
        appState.startModule(steps[0])
        if (steps.length > 1) {
            profileTimer.start()
        } else {
            runningProfileStep = -1
        }
    }

    // ═══════════════════════════════════════
    //  Inline Components
    // ═══════════════════════════════════════
    component ModuleBtn: Rectangle {
        property string text: ""
        property color btnColor: "#1F6FEB"
        property bool enabled: true
        signal clicked()

        width: 44; height: 24; radius: 4
        color: !enabled ? "#21262D" : btnMA.containsMouse ? Qt.lighter(btnColor, 1.2) : btnColor
        opacity: enabled ? 1.0 : 0.4

        Text {
            anchors.centerIn: parent; text: parent.text
            color: parent.enabled ? "#FFFFFF" : "#6E7681"
            font.pixelSize: 10; font.bold: true
        }
        MouseArea {
            id: btnMA; anchors.fill: parent; hoverEnabled: true
            cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (parent.enabled) parent.clicked()
        }
    }
}
