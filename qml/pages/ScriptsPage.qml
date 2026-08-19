import QtQuick 2.15
import QtQuick.Controls 2.15
import ZenithUI 1.0

Item {
    id: root
    property string pendingTaskId: ""

    function requestStart(taskId, taskName) {
        if (taskId === "c10_apriltag_landing") {
            pendingTaskId = taskId
            landingConfirm.open()
        } else {
            appState.startManagedTask(taskId)
        }
    }

    Dialog {
        id: landingConfirm
        anchors.centerIn: parent
        modal: true
        title: "确认启动精准降落"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appState.startManagedTask(root.pendingTaskId)

        contentItem: Text {
            width: 430
            wrapMode: Text.WordWrap
            color: "#E6EDF3"
            text: "该入口会启动 AprilTag 降落状态机。满足位姿、控制模式和高度门限后，任务可独立发送降落命令。请确认目标 Tag、相机和安全区域已准备好。"
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#8B5A2B"
        }
    }

    Column {
        anchors.fill: parent
        spacing: 10

        Row {
            width: parent.width
            height: 34
            spacing: 12
            Text {
                text: "机载任务"
                color: "#E6EDF3"
                font.pixelSize: 16
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: scriptActionModel.count + " 个白名单任务"
                color: "#6E7681"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: 20; height: 1 }
            PrimaryButton {
                width: 86
                height: 28
                text: "刷新状态"
                fillColor: "#21262D"
                enabled: appState.protocolConnected
                anchors.verticalCenter: parent.verticalCenter
                onClicked: appState.queryManagedTask()
            }
        }

        Rectangle {
            width: parent.width
            height: 82
            radius: 10
            color: "#161B22"
            border.color: appState.managedTaskActive ? "#2EA043" : "#30363D"

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 18

                Column {
                    width: 190
                    spacing: 5
                    Text { text: "机载任务状态"; color: "#8B949E"; font.pixelSize: 11 }
                    Text {
                        text: appState.managedTaskState
                        color: appState.managedTaskActive ? "#3FB950" : "#E6EDF3"
                        font.pixelSize: 17
                        font.bold: true
                    }
                }
                Column {
                    width: 250
                    spacing: 5
                    Text { text: "任务 ID"; color: "#8B949E"; font.pixelSize: 11 }
                    Text {
                        width: parent.width
                        text: appState.managedTaskName.length > 0 ? appState.managedTaskName : "--"
                        color: "#E6EDF3"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }
                Column {
                    width: parent.width - 470
                    spacing: 5
                    Text { text: "ACK / 说明"; color: "#8B949E"; font.pixelSize: 11 }
                    Text {
                        width: parent.width
                        text: (appState.managedTaskAck.length > 0 ? appState.managedTaskAck + " · " : "")
                              + appState.managedTaskReason
                        color: "#C9D1D9"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 36
            radius: 8
            color: "#2B2115"
            border.color: "#6E4C1E"
            Text {
                anchors.fill: parent
                anchors.margins: 9
                text: "停止任务：先停止任务输出，再锁定当前位置并请求 RC_POS_CONTROL；不会触发降落。若已在 LAND_CONTROL，则不干预降落。"
                color: "#D29922"
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
        }

        ListView {
            width: parent.width
            height: parent.height - 182
            model: scriptActionModel
            spacing: 8
            clip: true

            delegate: Rectangle {
                id: taskCard
                required property int index
                required property string name
                required property string command
                required property string note
                required property string category
                readonly property bool isActive:
                    appState.managedTaskActive && appState.managedTaskName === command
                readonly property bool isLast:
                    !appState.managedTaskActive && appState.managedTaskName === command

                width: ListView.view.width
                height: 82
                radius: 10
                color: isActive ? "#15251C" : "#161B22"
                border.color: isActive ? "#2EA043" : "#30363D"

                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 14

                    Rectangle {
                        width: 54
                        height: 24
                        radius: 12
                        color: category === "降落" ? "#3A211C"
                              : category === "跟踪" ? "#17243A" : "#1A2D24"
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: category
                            color: category === "降落" ? "#F0883E"
                                  : category === "跟踪" ? "#58A6FF" : "#3FB950"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Column {
                        width: parent.width - 278
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter
                        Row {
                            spacing: 10
                            Text { text: name; color: "#E6EDF3"; font.pixelSize: 14; font.bold: true }
                            Text {
                                text: isActive ? appState.managedTaskState : (isLast ? appState.managedTaskState : "")
                                color: isActive ? "#3FB950" : "#8B949E"
                                font.pixelSize: 11
                            }
                        }
                        Text {
                            width: parent.width
                            text: note
                            color: "#8B949E"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Row {
                        width: 196
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        PrimaryButton {
                            width: 92
                            height: 32
                            text: category === "降落" ? "启动精降" : "启动"
                            fillColor: category === "降落" ? "#6E3B1F" : "#1A4A2E"
                            enabled: appState.protocolConnected && !appState.managedTaskActive
                            onClicked: root.requestStart(command, name)
                        }
                        PrimaryButton {
                            width: 92
                            height: 32
                            text: "安全停止"
                            fillColor: isActive ? "#7D2525" : "#21262D"
                            textColor: isActive ? "#FFFFFF" : "#6E7681"
                            enabled: appState.protocolConnected && isActive
                            onClicked: appState.stopManagedTask(command)
                        }
                    }
                }
            }
        }
    }
}
