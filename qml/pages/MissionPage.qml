import QtQuick 2.15
import QtQuick.Controls 2.15
import "../components"

Item {
    Row {
        anchors.fill: parent
        spacing: 14

        Column {
            width: parent.width - 334
            spacing: 10

            Text { text: "地图监控"; color: "#1D2A3A"; font.pixelSize: 18; font.bold: true }

            Rectangle {
                width: parent.width
                height: 718
                radius: 14
                color: "#EAF1F8"
                border.color: "#D5E1EE"

                Canvas {
                    anchors.fill: parent
                    anchors.margins: 16
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#EAF1F8"
                        ctx.fillRect(0, 0, width, height)

                        ctx.strokeStyle = "#C8D6E8"
                        ctx.lineWidth = 1
                        for (var i = 0; i < 8; ++i) {
                            ctx.beginPath()
                            ctx.moveTo(0, i * height / 8)
                            ctx.lineTo(width, i * height / 8)
                            ctx.stroke()
                        }
                        for (var j = 0; j < 12; ++j) {
                            ctx.beginPath()
                            ctx.moveTo(j * width / 12, 0)
                            ctx.lineTo(j * width / 12, height)
                            ctx.stroke()
                        }

                        ctx.strokeStyle = "#2F6BFF"
                        ctx.lineWidth = 4
                        var path = appState.pathPoints
                        if (path.length > 1) {
                            ctx.beginPath()
                            ctx.moveTo(path[0].x * width, path[0].y * height)
                            for (var p = 1; p < path.length; ++p) {
                                ctx.lineTo(path[p].x * width, path[p].y * height)
                            }
                            ctx.stroke()
                        }

                        ctx.strokeStyle = "#F59E0B"
                        ctx.lineWidth = 2
                        ctx.setLineDash([8, 8])
                        var wps = appState.waypointPoints
                        if (wps.length > 1) {
                            ctx.beginPath()
                            ctx.moveTo(wps[0].x * width, wps[0].y * height)
                            for (var w = 1; w < wps.length; ++w) {
                                ctx.lineTo(wps[w].x * width, wps[w].y * height)
                            }
                            ctx.stroke()
                        }
                        ctx.setLineDash([])

                        for (var k = 0; k < wps.length; ++k) {
                            var wx = wps[k].x * width
                            var wy = wps[k].y * height
                            ctx.fillStyle = "#F59E0B"
                            ctx.beginPath()
                            ctx.arc(wx, wy, 7, 0, Math.PI * 2)
                            ctx.fill()
                        }

                        if (path.length > 0) {
                            var last = path[path.length - 1]
                            ctx.fillStyle = "#D14343"
                            ctx.beginPath()
                            ctx.arc(last.x * width, last.y * height, 10, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }
            }
        }

        Column {
            width: 320
            spacing: 10

            Rectangle {
                width: parent.width
                height: 156
                radius: 12
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text { text: "任务信息"; color: "#1D2A3A"; font.pixelSize: 18; font.bold: true }
                    Text { text: "当前位置: x 12.4 / y 6.8"; color: "#425469"; font.pixelSize: 13 }
                    Text { text: "下一航点: WP-03"; color: "#425469"; font.pixelSize: 13 }
                    Text { text: "剩余航点: 3"; color: "#425469"; font.pixelSize: 13 }
                    Text { text: "任务状态: " + appState.missionStage; color: "#17212E"; font.pixelSize: 15; font.bold: true }
                }
            }

            Rectangle {
                width: parent.width
                height: 334
                radius: 12
                color: "#F8FAFD"
                border.color: "#DCE4F2"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text { text: "航点列表"; color: "#1D2A3A"; font.pixelSize: 18; font.bold: true }

                    Repeater {
                        model: appState.waypointPoints

                        Rectangle {
                            width: 294
                            height: 46
                            radius: 10
                            color: "#FFFFFF"
                            border.color: "#E5ECF5"

                            Row {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                Text { text: "WP" + (index + 1); color: "#17212E"; font.pixelSize: 14; font.bold: true; width: 42 }
                                Text { text: "x " + Number(modelData.x * 100).toFixed(1) + " / y " + Number(modelData.y * 100).toFixed(1); color: "#72849A"; font.pixelSize: 13 }
                            }
                        }
                    }
                }
            }

            PrimaryButton { width: parent.width; text: "上传任务"; onClicked: appState.issueCommand(text) }
            PrimaryButton { width: parent.width; text: "开始任务"; fillColor: "#1D9B5F"; onClicked: appState.issueCommand(text) }
            PrimaryButton { width: parent.width; text: "暂停任务"; fillColor: "#B98516"; onClicked: appState.issueCommand(text) }
            PrimaryButton { width: parent.width; text: "返航"; fillColor: "#D14343"; onClicked: appState.issueCommand(text) }
        }
    }
}
