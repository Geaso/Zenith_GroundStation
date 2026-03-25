import QtQuick 2.15
import QtQuick.Controls 2.15
import "../components"

Item {
    Row {
        anchors.fill: parent
        spacing: 14

        Column {
            width: 92
            spacing: 10

            Rectangle {
                width: parent.width
                height: 118
                radius: 16
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Text { text: "视频监控"; color: "#2F6BFF"; font.pixelSize: 18; font.bold: true }
                    Text { text: "地图监控"; color: "#6E8095"; font.pixelSize: 15 }
                    Text { text: "数据监控"; color: "#6E8095"; font.pixelSize: 15 }
                }
            }
        }

        Column {
            width: parent.width - 92 - 328 - 28
            spacing: 10

            Row {
                width: parent.width
                spacing: 10

                Text {
                    text: "消息反馈"
                    color: "#1D2A3A"
                    font.pixelSize: 16
                    font.bold: true
                }

                Text {
                    text: appState.commandAck
                    color: "#6E8095"
                    font.pixelSize: 13
                }
            }

            Rectangle {
                width: parent.width
                height: 626
                radius: 14
                color: "#0F1725"
                border.color: "#314256"

                Canvas {
                    anchors.fill: parent
                    anchors.margins: 12
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#101827"
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = "#314256"
                        ctx.lineWidth = 1
                        for (var i = 1; i < 6; ++i) {
                            ctx.beginPath()
                            ctx.moveTo(0, i * height / 6)
                            ctx.lineTo(width, i * height / 6)
                            ctx.stroke()
                        }
                        for (var j = 1; j < 10; ++j) {
                            ctx.beginPath()
                            ctx.moveTo(j * width / 10, 0)
                            ctx.lineTo(j * width / 10, height)
                            ctx.stroke()
                        }
                        ctx.strokeStyle = "#FF4D4F"
                        ctx.lineWidth = 4
                        ctx.strokeRect(width * 0.34, height * 0.24, width * 0.28, height * 0.42)
                        ctx.strokeStyle = "#53C3A6"
                        ctx.lineWidth = 3
                        ctx.strokeRect(width * 0.61, height * 0.62, width * 0.12, height * 0.10)
                    }
                }

                Text {
                    text: "LIVE"
                    color: "#FF716B"
                    font.pixelSize: 22
                    font.bold: true
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 18
                }

                Text {
                    text: Qt.formatDateTime(new Date(), "yyyy-MM-dd hh:mm:ss")
                    color: "#D7E1EF"
                    font.pixelSize: 14
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                }
            }
        }

        Column {
            width: 308
            spacing: 12

            Rectangle {
                width: parent.width
                height: 348
                radius: 12
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Row {
                        spacing: 8
                        Rectangle { width: 132; height: 32; radius: 8; color: "#EAF0FF"; border.color: "#CFE0FF"
                            Text { anchors.centerIn: parent; text: "基本信息"; color: "#2F6BFF"; font.pixelSize: 14; font.bold: true } }
                        Rectangle { width: 132; height: 32; radius: 8; color: "#FFFFFF"; border.color: "#E0E7F1"
                            Text { anchors.centerIn: parent; text: "GPS信息"; color: "#6E8095"; font.pixelSize: 14 } }
                    }

                    Rectangle {
                        width: parent.width
                        height: 292
                        radius: 10
                        color: "#FFFFFF"
                        border.color: "#E3EAF3"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Text { text: "位置[x y z] [m]"; color: "#425469"; font.pixelSize: 13; font.bold: true }
                            Row {
                                width: parent.width
                                spacing: 18
                                Text { text: Number(appState.positionX).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.positionY).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.positionZ).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                            }

                            Text { text: "速度[x y z] [m/s]"; color: "#425469"; font.pixelSize: 13; font.bold: true }
                            Row {
                                width: parent.width
                                spacing: 18
                                Text { text: Number(appState.velocityX).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.velocityY).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.velocityZ).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                            }

                            Text { text: "姿态[r p y] [deg]"; color: "#425469"; font.pixelSize: 13; font.bold: true }
                            Row {
                                width: parent.width
                                spacing: 18
                                Text { text: Number(appState.roll).toFixed(1); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.pitch).toFixed(1); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.yaw).toFixed(1); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                            }

                            Text { text: "期望位置[x y z] [m]"; color: "#425469"; font.pixelSize: 13; font.bold: true }
                            Row {
                                width: parent.width
                                spacing: 18
                                Text { text: Number(appState.desiredPositionX).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.desiredPositionY).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.desiredPositionZ).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                            }

                            Text { text: "期望速度[x y z] [m/s]"; color: "#425469"; font.pixelSize: 13; font.bold: true }
                            Row {
                                width: parent.width
                                spacing: 18
                                Text { text: Number(appState.desiredVelocityX).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.desiredVelocityY).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                                Text { text: Number(appState.desiredVelocityZ).toFixed(2); color: "#17212E"; font.pixelSize: 20; width: 64; horizontalAlignment: Text.AlignHCenter }
                            }
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8

                StatusPill { width: parent.width; label: "飞控状态"; value: appState.controlState; tone: "#D83A3A"; valueColor: "#D83A3A"; borderTone: "#F3C1C1" }
                StatusPill { width: parent.width; label: "电量"; value: Number(appState.batteryVoltage).toFixed(1) + "V / " + Number(appState.batteryPercent * 100).toFixed(0) + "%"; tone: "#D83A3A"; valueColor: "#D83A3A"; borderTone: "#F3C1C1" }
                StatusPill { width: parent.width; label: "解锁状态"; value: appState.armed ? "Armed" : "Disarmed"; tone: "#D83A3A"; valueColor: "#D83A3A"; borderTone: "#F3C1C1" }
                StatusPill { width: parent.width; label: "定位源"; value: appState.locationSource; tone: "#D83A3A"; valueColor: "#D83A3A"; borderTone: "#F3C1C1" }
            }

            PrimaryButton { width: parent.width; text: "当前点悬停"; onClicked: appState.issueCommand(text) }
            PrimaryButton { width: parent.width; text: "初始点悬停"; fillColor: "#2A66C9"; onClicked: appState.issueCommand(text) }
            PrimaryButton { width: parent.width; text: "降落"; fillColor: "#D14343"; onClicked: appState.issueCommand(text) }

            ComboBox {
                id: manualModeBox
                width: parent.width
                model: ["XYZ_POS", "XYZ_VEL", "XYZ_POS_BODY", "LAT_LON_ALT"]
            }

            TextField { id: xField; width: parent.width; placeholderText: "x [m]" }
            TextField { id: yField; width: parent.width; placeholderText: "y [m]" }
            TextField { id: zField; width: parent.width; placeholderText: "z [m]" }
            TextField { id: yawField; width: parent.width; placeholderText: "yaw [deg]" }

            PrimaryButton {
                width: parent.width
                text: "上传"
                onClicked: appState.sendManualMove(manualModeBox.currentText,
                                                   Number(xField.text || 0),
                                                   Number(yField.text || 0),
                                                   Number(zField.text || 0),
                                                   Number(yawField.text || 0))
            }
        }
    }
}
