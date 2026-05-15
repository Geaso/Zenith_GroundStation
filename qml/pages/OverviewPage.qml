import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

Item {
    id: root

    // ── Left sidebar view index: 0=Map  1=Video  2=Data ──
    property int centerView: 0

    // ── Map state ──
    property real mapScale: 10.0
    property real mapCenterX: 0.0
    property real mapCenterY: 0.0
    property real defaultAlt: 1.5
    property int selectedWaypoint: -1
    property bool waypointPanelVisible: true

    // pan state
    property real panStartX: 0
    property real panStartY: 0
    property real panStartCenterX: 0
    property real panStartCenterY: 0

    // ── Waypoint model ──
    ListModel { id: waypointModel }

    Row {
        anchors.fill: parent
        spacing: 0

        // ═══════════════════════════════════════
        //  Left: Icon Sidebar  (56px)
        // ═══════════════════════════════════════
        Rectangle {
            width: 56
            height: parent.height
            color: "#161B22"
            radius: 10

            Column {
                anchors.fill: parent
                anchors.topMargin: 8
                spacing: 4

                SidebarIcon {
                    icon: "🗺"
                    label: "地图"
                    active: centerView === 0
                    onClicked: centerView = 0
                }
                SidebarIcon {
                    icon: "📹"
                    label: "视频"
                    active: centerView === 1
                    onClicked: centerView = 1
                }
                SidebarIcon {
                    icon: "📊"
                    label: "数据"
                    active: centerView === 2
                    onClicked: centerView = 2
                }
            }
        }

        Item { width: 8; height: 1 }

        // ═══════════════════════════════════════
        //  Center: Map / Video / Data
        // ═══════════════════════════════════════
        Rectangle {
            id: centerArea
            width: parent.width - 56 - 250 - 24
            height: parent.height
            radius: 10
            color: "#0D1117"
            border.color: "#21262D"
            clip: true

            // ── Map View ──
            Item {
                anchors.fill: parent
                visible: centerView === 0

                // Map title bar
                Rectangle {
                    id: mapHeader
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    height: 34; color: "transparent"; z: 10

                    Row {
                        anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12; spacing: 12
                        Text { text: "ENU 地图"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                        Text { text: "1m = " + mapScale.toFixed(0) + "px"; color: "#6E7681"; font.pixelSize: 9 }
                        Text { text: "(" + mapCenterX.toFixed(1) + ", " + mapCenterY.toFixed(1) + ")"; color: "#6E7681"; font.pixelSize: 9 }
                    }

                    Row {
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 12; spacing: 4

                        MapBtn { text: "+";   onClicked: { mapScale = Math.min(80, mapScale * 1.3); mapCanvas.requestPaint() } }
                        MapBtn { text: "-";   onClicked: { mapScale = Math.max(1, mapScale / 1.3); mapCanvas.requestPaint() } }
                        MapBtn { text: "UAV"; onClicked: { mapCenterX = appState.positionX; mapCenterY = appState.positionY; mapCanvas.requestPaint() } }
                        MapBtn { text: "原点"; onClicked: { mapCenterX = 0; mapCenterY = 0; mapCanvas.requestPaint() } }
                        MapBtn { text: waypointPanelVisible ? "隐藏航点" : "航点面板"; onClicked: waypointPanelVisible = !waypointPanelVisible }
                    }
                }

                Canvas {
                    id: mapCanvas
                    anchors.fill: parent
                    anchors.topMargin: 34

                    Connections {
                        target: appState
                        function onTelemetryChanged() { mapCanvas.requestPaint() }
                        function onPathChanged()      { mapCanvas.requestPaint() }
                    }

                    function enuToPixelX(ex) { return width / 2 + (ex - mapCenterX) * mapScale }
                    function enuToPixelY(ey) { return height / 2 - (ey - mapCenterY) * mapScale }
                    function pixelToEnuX(px) { return (px - width / 2) / mapScale + mapCenterX }
                    function pixelToEnuY(py) { return -(py - height / 2) / mapScale + mapCenterY }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#0D1117"
                        ctx.fillRect(0, 0, width, height)

                        // ── Grid ──
                        var gridSpacing = 1.0
                        if (mapScale < 4) gridSpacing = 10
                        else if (mapScale < 8) gridSpacing = 5
                        else if (mapScale < 20) gridSpacing = 2
                        else if (mapScale > 40) gridSpacing = 0.5

                        var left = pixelToEnuX(0), right = pixelToEnuX(width)
                        var top_ = pixelToEnuY(0), bottom_ = pixelToEnuY(height)

                        var gridStart = Math.floor(Math.min(left, right) / gridSpacing) * gridSpacing
                        var gridEnd = Math.ceil(Math.max(left, right) / gridSpacing) * gridSpacing

                        ctx.font = "9px monospace"; ctx.fillStyle = "#484F58"; ctx.textAlign = "center"
                        for (var gx = gridStart; gx <= gridEnd; gx += gridSpacing) {
                            var px = enuToPixelX(gx)
                            if (px < 0 || px > width) continue
                            ctx.strokeStyle = (Math.abs(gx) < 0.001) ? "#3A4258" : "#1C2333"
                            ctx.lineWidth = (Math.abs(gx) < 0.001) ? 1.2 : 0.6
                            ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                            if (mapScale >= 4 || Math.abs(gx % 5) < 0.01)
                                ctx.fillText(gx.toFixed(gridSpacing < 1 ? 1 : 0), px, height - 4)
                        }

                        var gridStartY = Math.floor(Math.min(bottom_, top_) / gridSpacing) * gridSpacing
                        var gridEndY = Math.ceil(Math.max(bottom_, top_) / gridSpacing) * gridSpacing
                        ctx.textAlign = "left"
                        for (var gy = gridStartY; gy <= gridEndY; gy += gridSpacing) {
                            var py = enuToPixelY(gy)
                            if (py < 0 || py > height) continue
                            ctx.strokeStyle = (Math.abs(gy) < 0.001) ? "#3A4258" : "#1C2333"
                            ctx.lineWidth = (Math.abs(gy) < 0.001) ? 1.2 : 0.6
                            ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(width, py); ctx.stroke()
                            if (mapScale >= 4 || Math.abs(gy % 5) < 0.01)
                                ctx.fillText(gy.toFixed(gridSpacing < 1 ? 1 : 0), 4, py - 3)
                        }

                        // ── Origin ──
                        var ox = enuToPixelX(0), oy = enuToPixelY(0)
                        ctx.fillStyle = "#3A4258"
                        ctx.beginPath(); ctx.arc(ox, oy, 4, 0, Math.PI * 2); ctx.fill()

                        // ── Axis labels ──
                        ctx.fillStyle = "#58A6FF"; ctx.font = "bold 11px sans-serif"; ctx.textAlign = "center"
                        var axLabelX = enuToPixelX(Math.min(right, mapCenterX + width / 2 / mapScale - 2))
                        ctx.fillText("X(E)", axLabelX, enuToPixelY(0) - 8)
                        ctx.textAlign = "left"
                        var axLabelY = enuToPixelY(Math.max(top_, mapCenterY + height / 2 / mapScale - 1))
                        ctx.fillText("Y(N)", enuToPixelX(0) + 8, axLabelY)

                        // ── Waypoint path ──
                        if (waypointModel.count > 1) {
                            ctx.strokeStyle = "#F59E0B"; ctx.lineWidth = 2; ctx.setLineDash([8, 6])
                            ctx.beginPath()
                            ctx.moveTo(enuToPixelX(waypointModel.get(0).wx), enuToPixelY(waypointModel.get(0).wy))
                            for (var w = 1; w < waypointModel.count; ++w)
                                ctx.lineTo(enuToPixelX(waypointModel.get(w).wx), enuToPixelY(waypointModel.get(w).wy))
                            ctx.stroke(); ctx.setLineDash([])
                        }

                        // ── Waypoint markers ──
                        for (var k = 0; k < waypointModel.count; ++k) {
                            var wp = waypointModel.get(k)
                            var wpx = enuToPixelX(wp.wx), wpy = enuToPixelY(wp.wy)
                            var isSelected = (k === selectedWaypoint)
                            var radius = isSelected ? 10 : 7

                            ctx.fillStyle = isSelected ? "#FF7B00" : "#F59E0B"
                            ctx.beginPath(); ctx.arc(wpx, wpy, radius, 0, Math.PI * 2); ctx.fill()

                            if (isSelected) {
                                ctx.strokeStyle = "#FFFFFF"; ctx.lineWidth = 2
                                ctx.beginPath(); ctx.arc(wpx, wpy, radius + 3, 0, Math.PI * 2); ctx.stroke()
                            }

                            ctx.fillStyle = "#0D1117"; ctx.font = "bold 9px sans-serif"
                            ctx.textAlign = "center"; ctx.textBaseline = "middle"
                            ctx.fillText(String(k + 1), wpx, wpy)
                        }

                        // ── UAV marker ──
                        var uavPx = enuToPixelX(appState.positionX)
                        var uavPy = enuToPixelY(appState.positionY)
                        var headingRad = appState.yaw * Math.PI / 180

                        ctx.save()
                        ctx.translate(uavPx, uavPy)
                        ctx.rotate(-(headingRad - Math.PI / 2))

                        var sz = 12
                        ctx.fillStyle = "#F85149"
                        ctx.beginPath()
                        ctx.moveTo(0, -sz)
                        ctx.lineTo(-sz * 0.6, sz * 0.5)
                        ctx.lineTo(sz * 0.6, sz * 0.5)
                        ctx.closePath()
                        ctx.fill()

                        ctx.fillStyle = "#FFFFFF"
                        ctx.beginPath(); ctx.arc(0, 0, 3, 0, Math.PI * 2); ctx.fill()
                        ctx.restore()

                        ctx.fillStyle = "#C9D1D9"; ctx.font = "10px monospace"; ctx.textAlign = "left"
                        ctx.fillText(
                            "(" + Number(appState.positionX).toFixed(1) + ", " +
                            Number(appState.positionY).toFixed(1) + ", " +
                            Number(appState.positionZ).toFixed(1) + ")",
                            uavPx + 16, uavPy + 4
                        )
                    }

                    MouseArea {
                        id: mapMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        property bool isPanning: false

                        onPressed: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                var ex = mapCanvas.pixelToEnuX(mouse.x)
                                var ey = mapCanvas.pixelToEnuY(mouse.y)
                                waypointModel.append({ wx: ex, wy: ey, wz: defaultAlt, wyaw: 0 })
                                selectedWaypoint = waypointModel.count - 1
                                mapCanvas.requestPaint()
                            } else {
                                var hitIdx = hitTestWaypoint(mouse.x, mouse.y)
                                if (hitIdx >= 0) {
                                    selectedWaypoint = hitIdx
                                    mapCanvas.requestPaint()
                                } else {
                                    isPanning = true
                                    panStartX = mouse.x; panStartY = mouse.y
                                    panStartCenterX = mapCenterX; panStartCenterY = mapCenterY
                                    selectedWaypoint = -1
                                    mapCanvas.requestPaint()
                                }
                            }
                        }

                        onPositionChanged: function(mouse) {
                            if (isPanning) {
                                mapCenterX = panStartCenterX - (mouse.x - panStartX) / mapScale
                                mapCenterY = panStartCenterY + (mouse.y - panStartY) / mapScale
                                mapCanvas.requestPaint()
                            }
                        }

                        onReleased: function(mouse) { isPanning = false }

                        onWheel: function(wheel) {
                            var factor = wheel.angleDelta.y > 0 ? 1.2 : (1.0 / 1.2)
                            var mx = mapCanvas.pixelToEnuX(wheel.x)
                            var my = mapCanvas.pixelToEnuY(wheel.y)
                            mapScale = Math.max(1, Math.min(80, mapScale * factor))
                            var newMx = mapCanvas.pixelToEnuX(wheel.x)
                            var newMy = mapCanvas.pixelToEnuY(wheel.y)
                            mapCenterX += (mx - newMx); mapCenterY += (my - newMy)
                            mapCanvas.requestPaint()
                        }

                        function hitTestWaypoint(px, py) {
                            for (var i = waypointModel.count - 1; i >= 0; --i) {
                                var wp = waypointModel.get(i)
                                var dx = mapCanvas.enuToPixelX(wp.wx) - px
                                var dy = mapCanvas.enuToPixelY(wp.wy) - py
                                if (dx * dx + dy * dy < 15 * 15) return i
                            }
                            return -1
                        }
                    }
                }

                // Legend
                Row {
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12
                    spacing: 14; z: 5
                    LegendDot { dotColor: "#F85149"; label: "UAV" }
                    LegendDot { dotColor: "#F59E0B"; label: "航点" }
                    LegendDot { dotColor: "#1F6FEB"; label: "轨迹" }
                }

                // Mouse coordinate tooltip
                Text {
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 12
                    text: {
                        var mx = mapCanvas.pixelToEnuX(mapMouse.mouseX)
                        var my = mapCanvas.pixelToEnuY(mapMouse.mouseY)
                        return "鼠标: (" + mx.toFixed(2) + ", " + my.toFixed(2) + ")"
                    }
                    color: "#6E7681"; font.pixelSize: 9; font.family: "Consolas"; z: 5
                }

                // ── Waypoint Overlay Panel ──
                Rectangle {
                    visible: waypointPanelVisible
                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.topMargin: 38; anchors.leftMargin: 6; anchors.bottomMargin: 6
                    width: 220
                    radius: 8
                    color: "#161B22"
                    border.color: "#30363D"
                    opacity: 0.95
                    z: 8

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        // Title + buttons
                        Row {
                            width: parent.width; spacing: 4
                            Text { text: "航点列表"; color: "#8B949E"; font.pixelSize: 10; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Item { width: parent.width - 120; height: 1 }
                            MapBtn { text: "清空"; onClicked: { waypointModel.clear(); selectedWaypoint = -1; mapCanvas.requestPaint() } }
                            MapBtn { text: "删除"; onClicked: {
                                if (selectedWaypoint >= 0 && selectedWaypoint < waypointModel.count) {
                                    waypointModel.remove(selectedWaypoint); selectedWaypoint = -1; mapCanvas.requestPaint()
                                }
                            }}
                        }

                        // Waypoint list
                        ListView {
                            id: wpListView
                            width: parent.width
                            height: parent.parent.height - 160
                            model: waypointModel
                            spacing: 3
                            clip: true

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 30
                                radius: 5
                                color: index === selectedWaypoint ? "#1F3D6F" : "#21262D"
                                border.color: index === selectedWaypoint ? "#1F6FEB" : "#30363D"

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { selectedWaypoint = index; mapCanvas.requestPaint() }
                                }

                                Row {
                                    anchors.fill: parent; anchors.margins: 6; spacing: 6
                                    Rectangle {
                                        width: 20; height: 16; radius: 3; color: "#F59E0B"
                                        anchors.verticalCenter: parent.verticalCenter
                                        Text { anchors.centerIn: parent; text: String(index + 1); color: "#0D1117"; font.pixelSize: 8; font.bold: true }
                                    }
                                    Text {
                                        text: "(" + Number(model.wx).toFixed(1) + ", " + Number(model.wy).toFixed(1) + ") H:" + Number(model.wz).toFixed(1)
                                        color: "#C9D1D9"; font.pixelSize: 10; font.family: "Consolas"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }

                        // Default altitude
                        Row {
                            width: parent.width; spacing: 6
                            Text { text: "默认高度:"; color: "#8B949E"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                            TextField {
                                id: altField; width: 50; height: 24
                                text: defaultAlt.toFixed(1)
                                color: "#E6EDF3"; font.pixelSize: 11
                                background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
                                onEditingFinished: { var v = Number(text); if (!isNaN(v) && v > 0) defaultAlt = v }
                            }
                            Text { text: "m"; color: "#8B949E"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }

                        // Mission controls
                        PrimaryButton {
                            width: parent.width; height: 26; text: "上传航点"; fillColor: "#1F4E8C"
                            onClicked: uploadWaypoints()
                        }
                        PrimaryButton {
                            width: parent.width; height: 26; text: "开始任务"; fillColor: "#1A4A2E"
                            onClicked: appState.issueCommand("开始任务")
                        }
                        PrimaryButton {
                            width: parent.width; height: 26; text: "返航"; fillColor: "#6E1A1A"
                            onClicked: appState.issueCommand("返航")
                        }
                    }
                }
            }

            // ── Video View ──
            Item {
                anchors.fill: parent
                visible: centerView === 1

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Text {
                        text: "📹"
                        font.pixelSize: 48
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "未连接视频流"
                        color: "#6E7681"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "RTSP 视频流将在后续版本接入"
                        color: "#484F58"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            // ── Data View ──
            Item {
                anchors.fill: parent
                visible: centerView === 2

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    // Message feedback
                    Rectangle {
                        width: parent.width; height: 80
                        radius: 8; color: "#161B22"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 6
                            Text { text: "消息反馈"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                            Rectangle { width: parent.width; height: 1; color: "#262C36" }
                            Text {
                                width: parent.width
                                text: appState.commandAck.length > 0 ? appState.commandAck : "Zenith Link Ready"
                                color: "#58A6FF"; font.pixelSize: 12; wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // Flight recorder
                    Rectangle {
                        width: parent.width; height: 80
                        radius: 8; color: "#161B22"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 6

                            Row {
                                width: parent.width; spacing: 8
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: appState.recording ? "#F85149" : "#484F58"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: appState.recording
                                          ? "录制中 " + Number(appState.recordingElapsed).toFixed(0) + "s / " + appState.recordingSamples + " 条"
                                          : "数据录制"
                                    color: appState.recording ? "#F85149" : "#8B949E"
                                    font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Row {
                                width: parent.width; spacing: 6
                                PrimaryButton {
                                    width: (parent.width - 6) / 2; height: 28
                                    text: "开始录制"; fillColor: "#1A5C30"
                                    enabled: !appState.recording
                                    onClicked: appState.startRecording()
                                }
                                PrimaryButton {
                                    width: (parent.width - 6) / 2; height: 28
                                    text: "停止录制"; fillColor: "#6E1A1A"
                                    enabled: appState.recording
                                    onClicked: appState.stopRecording()
                                }
                            }
                        }
                    }

                    // Protocol log
                    Rectangle {
                        width: parent.width
                        height: parent.height - 80 - 80 - 20
                        radius: 8; color: "#161B22"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 6
                            Text { text: "协议日志"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                            Rectangle { width: parent.width; height: 1; color: "#262C36" }
                            ScrollView {
                                width: parent.width
                                height: parent.parent.height - 40
                                TextArea {
                                    readOnly: true; wrapMode: TextArea.Wrap
                                    text: appState.protocolLogText
                                    font.pixelSize: 11; font.family: "Consolas"; color: "#8B949E"
                                    background: null
                                    onTextChanged: cursorPosition = length
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { width: 8; height: 1 }

        // ═══════════════════════════════════════
        //  Right: Telemetry + Controls  (250px)
        // ═══════════════════════════════════════
        Column {
            width: 250
            height: parent.height
            spacing: 6

            // ── Telemetry (show "--" when no telemetry link) ──
            TelemetryGroup { title: "位置 [m]"; labels: ["X","Y","Z"]
                values: appState.connected ? [ Number(appState.positionX).toFixed(2), Number(appState.positionY).toFixed(2), Number(appState.positionZ).toFixed(2) ] : ["--","--","--"] }

            TelemetryGroup { title: "速度 [m/s]"; labels: ["X","Y","Z"]
                values: appState.connected ? [ Number(appState.velocityX).toFixed(2), Number(appState.velocityY).toFixed(2), Number(appState.velocityZ).toFixed(2) ] : ["--","--","--"] }

            TelemetryGroup { title: "姿态 [deg]"; labels: ["R","P","Y"]
                values: appState.connected ? [ Number(appState.roll).toFixed(1), Number(appState.pitch).toFixed(1), Number(appState.yaw).toFixed(1) ] : ["--","--","--"] }

            TelemetryGroup { title: "期望位置 [m]"; labels: ["X","Y","Z"]
                values: appState.connected ? [ Number(appState.desiredPositionX).toFixed(2), Number(appState.desiredPositionY).toFixed(2), Number(appState.desiredPositionZ).toFixed(2) ] : ["--","--","--"] }

            TelemetryGroup { title: "期望速度 [m/s]"; labels: ["X","Y","Z"]
                values: appState.connected ? [ Number(appState.desiredVelocityX).toFixed(2), Number(appState.desiredVelocityY).toFixed(2), Number(appState.desiredVelocityZ).toFixed(2) ] : ["--","--","--"] }

            // ── Separator ──
            Rectangle { width: parent.width; height: 1; color: "#262C36" }

            // ── Quick Commands ──
            Rectangle {
                width: parent.width
                height: 134
                radius: 8
                color: "#161B22"
                border.color: "#30363D"

                Column {
                    anchors.fill: parent; anchors.margins: 10; spacing: 6
                    Text { text: "快捷指令"; color: "#9AA4B2"; font.pixelSize: 10; font.bold: true }
                    PrimaryButton { width: parent.width; height: 26; text: "当前点悬停"; fillColor: "#1F4E8C"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                    PrimaryButton { width: parent.width; height: 26; text: "初始点悬停"; fillColor: "#1A5C30"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                    PrimaryButton { width: parent.width; height: 26; text: "降落"; fillColor: "#6E1A1A"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                }
            }

            // ── Manual Control ──
            Rectangle {
                width: parent.width
                height: parent.height - 5 * 68 - 134 - 1 - 7 * 6
                radius: 8
                color: "#161B22"
                border.color: "#30363D"

                Column {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8

                    Text { text: "手动控制"; color: "#9AA4B2"; font.pixelSize: 10; font.bold: true }

                    ComboBox {
                        id: manualModeBox
                        width: parent.width; height: 26
                        model: ["XYZ_POS", "XYZ_VEL", "XYZ_POS_BODY", "LAT_LON_ALT"]
                        background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
                        contentItem: Text {
                            leftPadding: 8; text: manualModeBox.currentText
                            color: "#E6EDF3"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Row {
                        width: parent.width; spacing: 6
                        Column {
                            width: (parent.width - 6) / 2; spacing: 3
                            Text { text: "X [m]"; color: "#8B949E"; font.pixelSize: 9 }
                            MiniField { id: xField; width: parent.width; height: 26 }
                        }
                        Column {
                            width: (parent.width - 6) / 2; spacing: 3
                            Text { text: "Y [m]"; color: "#8B949E"; font.pixelSize: 9 }
                            MiniField { id: yField; width: parent.width; height: 26 }
                        }
                    }

                    Row {
                        width: parent.width; spacing: 6
                        Column {
                            width: (parent.width - 6) / 2; spacing: 3
                            Text { text: "Z [m]"; color: "#8B949E"; font.pixelSize: 9 }
                            MiniField { id: zField; width: parent.width; height: 26 }
                        }
                        Column {
                            width: (parent.width - 6) / 2; spacing: 3
                            Text { text: "Yaw [deg]"; color: "#8B949E"; font.pixelSize: 9 }
                            MiniField { id: yawField; width: parent.width; height: 26 }
                        }
                    }

                    PrimaryButton {
                        width: parent.width; height: 30
                        text: "上传指令"; fillColor: "#1F6FEB"
                        onClicked: appState.sendManualMove(
                            manualModeBox.currentText,
                            Number(xField.val || 0), Number(yField.val || 0),
                            Number(zField.val || 0), Number(yawField.val || 0))
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════
    //  Upload waypoints
    // ═══════════════════════════════════════
    function uploadWaypoints() {
        if (waypointModel.count === 0) return
        for (var i = 0; i < waypointModel.count; ++i) {
            var wp = waypointModel.get(i)
            appState.sendManualMove("XYZ_POS", wp.wx, wp.wy, wp.wz, wp.wyaw)
        }
    }

    // ═══════════════════════════════════════
    //  Inline Components
    // ═══════════════════════════════════════

    component SidebarIcon: Rectangle {
        property string icon: ""
        property string label: ""
        property bool active: false
        signal clicked()

        width: 48; height: 48; radius: 8
        color: active ? "#1F6FEB" : (sideMA.containsMouse ? "#21262D" : "transparent")
        anchors.horizontalCenter: parent.horizontalCenter

        Column {
            anchors.centerIn: parent; spacing: 2
            Text { text: icon; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: label; color: active ? "#FFFFFF" : "#6E7681"; font.pixelSize: 9; font.bold: active; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter }
        }

        MouseArea {
            id: sideMA; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked()
        }
    }

    component TelemetryGroup: Rectangle {
        property string title: ""
        property var labels: ["X","Y","Z"]
        property var values: []

        width: 250; height: 68; radius: 8
        color: "#161B22"; border.color: "#30363D"

        Column {
            anchors.fill: parent; anchors.margins: 8; spacing: 4
            Text { text: title; color: "#9AA4B2"; font.pixelSize: 10; font.bold: true }
            Row {
                width: parent.width; height: 30; spacing: 0
                Repeater {
                    model: values
                    delegate: Item {
                        width: parent.width / 3; height: parent.height
                        Column {
                            anchors.centerIn: parent; spacing: 1
                            Text { text: labels[index]; color: "#6E7681"; font.pixelSize: 8; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter }
                            Text { text: modelData; color: modelData === "--" ? "#484F58" : "#E6EDF3"; font.pixelSize: 14; font.bold: true; font.family: "Consolas"; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter }
                        }
                        Rectangle {
                            visible: index < 2; anchors.right: parent.right
                            width: 1; height: parent.height * 0.5; anchors.verticalCenter: parent.verticalCenter; color: "#262C36"
                        }
                    }
                }
            }
        }
    }

    component MapBtn: Rectangle {
        property string text: ""
        signal clicked()
        width: Math.max(36, btnText.width + 12); height: 20; radius: 4
        color: btnMA.containsMouse ? "#30363D" : "#21262D"
        border.color: "#30363D"
        Text { id: btnText; anchors.centerIn: parent; text: parent.text; color: "#8B949E"; font.pixelSize: 9; font.bold: true }
        MouseArea { id: btnMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked() }
    }

    component LegendDot: Row {
        property color dotColor: "#FFFFFF"
        property string label: ""
        spacing: 4
        Rectangle { width: 8; height: 8; radius: 4; color: parent.dotColor; anchors.verticalCenter: parent.verticalCenter }
        Text { text: label; color: "#6E7681"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
    }

    component MiniField: TextField {
        id: rootField
        property alias val: rootField.text
        color: "#E6EDF3"; font.pixelSize: 11; placeholderTextColor: "#6E7681"
        background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
    }
}
