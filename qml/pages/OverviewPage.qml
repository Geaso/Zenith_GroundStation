import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick3D
import Zenith3D 1.0
import QtQuick.Layouts 1.15
import ZenithUI 1.0

Item {
    id: root

    // Helper: eliminate -0.0 display
    function fmt(val, decimals) {
        var s = Number(val).toFixed(decimals)
        return (s.charAt(0) === '-' && parseFloat(s) === 0) ? s.substring(1) : s
    }

    // ── View index: 0=Map  1=Video  2=Data ──
    property int centerView: 0

    // ── Map mode: 0=ENU grid, 1=GPS tiles ──
    // userMapPref: 0=auto, 1=forceENU, 2=forceGPS
    property int userMapPref: 0
    property bool hasGps: appState.connected && appState.gpsFix >= 2
    property int effectiveMapMode: {
        if (!hasGps) return 0                         // no GPS → ENU only
        if (userMapPref === 1) return 0               // user forced ENU
        return 1                                       // auto or forceGPS → GPS tiles
    }

    // ── GPS map state ──
    property real gpsLat: 31.03
    property real gpsLon: 121.45
    property int gpsZoom: 16
    property real gpsPanStartX: 0
    property real gpsPanStartY: 0
    property real gpsPanStartLat: 0
    property real gpsPanStartLon: 0

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
        //  Center: Map / Video / Data
        // ═══════════════════════════════════════
        Rectangle {
            id: centerArea
            width: parent.width - 258
            height: parent.height
            radius: 10
            color: "#0D1117"
            border.color: "#21262D"
            clip: true

            // ── Floating View Tabs (top-right, z above map) ──
            Row {
                anchors.top: parent.top; anchors.right: parent.right
                anchors.topMargin: 8; anchors.rightMargin: 8
                spacing: 4; z: 20

                Repeater {
                    model: [
                        { label: "地图", view: 0 },
                        { label: "栅格", view: 3 },
                        { label: "视频", view: 1 },
                        { label: "数据", view: 2 }
                    ]
                    delegate: Rectangle {
                        width: 42; height: 22; radius: 4
                        color: centerView === modelData.view ? "#1F6FEB" : "#21262DCC"
                        border.color: centerView === modelData.view ? "#1F6FEB" : "#30363D"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: centerView === modelData.view ? "#FFFFFF" : "#8B949E"
                            font.pixelSize: 10; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: centerView = modelData.view }
                    }
                }
            }

            // ── Map View ──
            Item {
                anchors.fill: parent
                visible: centerView === 0

                // Floating map toolbar (top-right, below view tabs)
                Row {
                    anchors.top: parent.top; anchors.right: parent.right
                    anchors.topMargin: 38; anchors.rightMargin: 8
                    spacing: 4; z: 15

                    MapBtn { text: "+";   onClicked: { mapScale = Math.min(80, mapScale * 1.3); mapCanvas.requestPaint() } }
                    MapBtn { text: "-";   onClicked: { mapScale = Math.max(1, mapScale / 1.3); mapCanvas.requestPaint() } }
                    MapBtn { text: "UAV"; onClicked: { mapCenterX = appState.positionX; mapCenterY = appState.positionY; mapCanvas.requestPaint() } }
                    MapBtn { text: "原点"; onClicked: { mapCenterX = 0; mapCenterY = 0; mapCanvas.requestPaint() } }
                    MapBtn { text: waypointPanelVisible ? "隐藏航点" : "航点"; onClicked: waypointPanelVisible = !waypointPanelVisible }
                }

                // ── 摄像头录像控件 (前/下 各 Start/Stop), 工具栏下方 ──
                Column {
                    anchors.top: parent.top; anchors.right: parent.right
                    anchors.topMargin: 74; anchors.rightMargin: 8
                    spacing: 4; z: 15

                    // 前摄
                    Row {
                        spacing: 4
                        Rectangle {
                            width: 70; height: 22; radius: 4
                            color: "#1F2937CC"; border.color: "#30363D80"
                            Text { anchors.centerIn: parent; text: "前摄录像"; color: "#9CA3AF"; font.pixelSize: 9 }
                        }
                        MapBtn {
                            text: "● Rec"
                            onClicked: appState.runScriptAction(
                                "Front Cam Record Start",
                                "bash ~/Zenith_ws/src/zenith_apriltag/scripts/cam_record_toggle.sh front start",
                                "Send To Current UAV")
                        }
                        MapBtn {
                            text: "■ Stop"
                            onClicked: appState.runScriptAction(
                                "Front Cam Record Stop",
                                "bash ~/Zenith_ws/src/zenith_apriltag/scripts/cam_record_toggle.sh front stop",
                                "Send To Current UAV")
                        }
                    }

                    // 下摄
                    Row {
                        spacing: 4
                        Rectangle {
                            width: 70; height: 22; radius: 4
                            color: "#1F2937CC"; border.color: "#30363D80"
                            Text { anchors.centerIn: parent; text: "下摄录像"; color: "#9CA3AF"; font.pixelSize: 9 }
                        }
                        MapBtn {
                            text: "● Rec"
                            onClicked: appState.runScriptAction(
                                "Down Cam Record Start",
                                "bash ~/Zenith_ws/src/zenith_apriltag/scripts/cam_record_toggle.sh down start",
                                "Send To Current UAV")
                        }
                        MapBtn {
                            text: "■ Stop"
                            onClicked: appState.runScriptAction(
                                "Down Cam Record Stop",
                                "bash ~/Zenith_ws/src/zenith_apriltag/scripts/cam_record_toggle.sh down stop",
                                "Send To Current UAV")
                        }
                    }
                }

                // ── Floating HUD overlay (top-left) ──
                Rectangle {
                    id: hudOverlay
                    anchors.top: parent.top; anchors.left: parent.left
                    anchors.topMargin: 8; anchors.leftMargin: 8
                    width: hudRow.width + 20; height: 32; radius: 6
                    color: "#161B22DD"; border.color: "#30363D80"
                    z: 16

                    Row {
                        id: hudRow; anchors.centerIn: parent; spacing: 14

                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "ALT"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? fmt(appState.positionZ, 1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "m"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "SPD"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? fmt(appState.speed, 1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "m/s"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "VSPD"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? fmt(appState.velocityZ, 1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "HDG"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? fmt(appState.yaw, 0) : "0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "\u00B0"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "BAT"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: (appState.connected && appState.batteryValid) ? fmt(appState.batteryVoltage, 1) : "--"; color: appState.batteryPercent < 0.2 ? "#F85149" : "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "V"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }

                // ── PFD Attitude Indicator (below HUD) ──
                Canvas {
                    id: pfdCanvas
                    anchors.top: hudOverlay.bottom; anchors.left: parent.left
                    anchors.topMargin: 8; anchors.leftMargin: 8
                    width: 130; height: 130; z: 16

                    Connections {
                        target: appState
                        function onTelemetryChanged() { pfdCanvas.requestPaint() }
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()

                        var cx = width / 2, cy = height / 2, r = 58
                        var rollDeg = appState.roll || 0
                        var pitchDeg = appState.pitch || 0
                        var rollRad = rollDeg * Math.PI / 180
                        var pitchPx = pitchDeg * (r / 30) // 30 deg = full radius

                        // Clip circle
                        ctx.save()
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, Math.PI * 2)
                        ctx.clip()

                        // Sky + Ground (rotated by roll, shifted by pitch)
                        ctx.save()
                        ctx.translate(cx, cy)
                        ctx.rotate(-rollRad)

                        // Sky
                        ctx.fillStyle = "#1A4B8C"
                        ctx.fillRect(-r * 2, -r * 2 + pitchPx, r * 4, r * 2)
                        // Ground
                        ctx.fillStyle = "#6B4226"
                        ctx.fillRect(-r * 2, pitchPx, r * 4, r * 2)
                        // Horizon line
                        ctx.strokeStyle = "#FFFFFF"
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        ctx.moveTo(-r * 2, pitchPx)
                        ctx.lineTo(r * 2, pitchPx)
                        ctx.stroke()

                        // Pitch ladder (every 10 degrees)
                        ctx.strokeStyle = "#FFFFFFAA"
                        ctx.fillStyle = "#FFFFFFCC"
                        ctx.font = "8px Consolas"
                        ctx.textAlign = "center"
                        ctx.lineWidth = 1
                        for (var p = -30; p <= 30; p += 10) {
                            if (p === 0) continue
                            var py = pitchPx - p * (r / 30)
                            var lw = 18
                            ctx.beginPath()
                            ctx.moveTo(-lw, py); ctx.lineTo(lw, py)
                            ctx.stroke()
                            ctx.fillText(Math.abs(p).toString(), lw + 10, py + 3)
                        }

                        ctx.restore()

                        // Roll indicator arc (fixed at top)
                        ctx.strokeStyle = "#FFFFFF80"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.arc(cx, cy, r - 4, Math.PI * 1.2, Math.PI * 1.8)
                        ctx.stroke()

                        // Roll triangle pointer
                        ctx.save()
                        ctx.translate(cx, cy)
                        ctx.rotate(-rollRad)
                        ctx.fillStyle = "#FFFFFFCC"
                        ctx.beginPath()
                        ctx.moveTo(0, -(r - 4))
                        ctx.lineTo(-5, -(r - 12))
                        ctx.lineTo(5, -(r - 12))
                        ctx.closePath()
                        ctx.fill()
                        ctx.restore()

                        // Center aircraft symbol (fixed)
                        ctx.strokeStyle = "#FFD700"
                        ctx.lineWidth = 2.5
                        ctx.beginPath()
                        ctx.moveTo(cx - 22, cy); ctx.lineTo(cx - 8, cy)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(cx + 8, cy); ctx.lineTo(cx + 22, cy)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(cx, cy, 3, 0, Math.PI * 2)
                        ctx.stroke()

                        ctx.restore()

                        // Outer ring
                        ctx.strokeStyle = "#30363D"
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.arc(cx, cy, r + 2, 0, Math.PI * 2)
                        ctx.stroke()

                        // Heading readout at bottom
                        ctx.fillStyle = "#161B22DD"
                        ctx.fillRect(cx - 22, cy + r - 6, 44, 16)
                        ctx.fillStyle = "#E6EDF3"
                        ctx.font = "bold 10px Consolas"
                        ctx.textAlign = "center"
                        var yawStr = fmt(appState.yaw || 0, 0)
                        ctx.fillText(yawStr + "\u00B0", cx, cy + r + 6)
                    }
                }

                // Map mode label (below PFD)
                Row {
                    id: mapModeLabel
                    anchors.top: pfdCanvas.bottom; anchors.left: parent.left
                    anchors.topMargin: 4; anchors.leftMargin: 12
                    spacing: 8; z: 15
                    Text { text: effectiveMapMode === 0 ? "ENU" : "GPS"; color: "#58A6FF"; font.pixelSize: 10; font.bold: true }
                    Text { text: effectiveMapMode === 0 ? "1m=" + mapScale.toFixed(0) + "px" : "Z" + gpsZoom; color: "#6E7681"; font.pixelSize: 9 }
                }

                // ── Map Mode Switcher (bottom-left) ──
                Rectangle {
                    anchors.left: parent.left; anchors.bottom: parent.bottom
                    anchors.leftMargin: waypointPanelVisible ? 234 : 8; anchors.bottomMargin: 8
                    width: switchRow.width + 12; height: 26; radius: 5
                    color: "#161B22DD"; border.color: "#30363D80"
                    z: 18

                    Row {
                        id: switchRow; anchors.centerIn: parent; spacing: 4
                        Rectangle {
                            width: 38; height: 20; radius: 4
                            color: effectiveMapMode === 0 ? "#1F6FEB" : (enuMA.containsMouse ? "#30363D" : "transparent")
                            Text { anchors.centerIn: parent; text: "ENU"; color: effectiveMapMode === 0 ? "#FFF" : "#8B949E"; font.pixelSize: 9; font.bold: true }
                            MouseArea { id: enuMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: userMapPref = 1 }
                        }
                        Rectangle {
                            width: 38; height: 20; radius: 4
                            color: effectiveMapMode === 1 ? "#1F6FEB" : (gpsMA.containsMouse && hasGps ? "#30363D" : "transparent")
                            opacity: hasGps ? 1.0 : 0.35
                            Text { anchors.centerIn: parent; text: "GPS"; color: effectiveMapMode === 1 ? "#FFF" : "#8B949E"; font.pixelSize: 9; font.bold: true }
                            MouseArea { id: gpsMA; anchors.fill: parent; hoverEnabled: true; cursorShape: hasGps ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { if (hasGps) userMapPref = 2 } }
                        }
                        Rectangle {
                            width: 38; height: 20; radius: 4
                            color: userMapPref === 0 ? "#1F6FEB40" : (autoMA.containsMouse ? "#30363D" : "transparent")
                            border.color: userMapPref === 0 ? "#1F6FEB" : "transparent"
                            Text { anchors.centerIn: parent; text: "Auto"; color: userMapPref === 0 ? "#58A6FF" : "#6E7681"; font.pixelSize: 9; font.bold: true }
                            MouseArea { id: autoMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: userMapPref = 0 }
                        }
                    }
                }

                // ═══════════════════════════════════════
                //  ENU Grid Canvas
                // ═══════════════════════════════════════
                Canvas {
                    id: mapCanvas
                    anchors.fill: parent
                    visible: effectiveMapMode === 0

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
                            "(" + fmt(appState.positionX, 1) + ", " +
                            fmt(appState.positionY, 1) + ", " +
                            fmt(appState.positionZ, 1) + ")",
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

                // ═══════════════════════════════════════
                //  GPS Tile Map (OSM tiles via HTTP)
                // ═══════════════════════════════════════
                Item {
                    id: gpsTileMap
                    anchors.fill: parent
                    visible: effectiveMapMode === 1
                    clip: true

                    // Tile math functions
                    function lon2tileX(lon, z) { return (lon + 180.0) / 360.0 * Math.pow(2, z) }
                    function lat2tileY(lat, z) {
                        var r = lat * Math.PI / 180.0
                        return (1.0 - Math.log(Math.tan(r) + 1.0 / Math.cos(r)) / Math.PI) / 2.0 * Math.pow(2, z)
                    }
                    function tileX2lon(x, z) { return x / Math.pow(2, z) * 360.0 - 180.0 }
                    function tileY2lat(y, z) {
                        var n = Math.PI - 2.0 * Math.PI * y / Math.pow(2, z)
                        return 180.0 / Math.PI * Math.atan(0.5 * (Math.exp(n) - Math.exp(-n)))
                    }

                    // Current center in tile-float coords
                    property real txf: lon2tileX(gpsLat !== 0 ? gpsLon : 121.45, gpsZoom)
                    property real tyf: lat2tileY(gpsLat !== 0 ? gpsLat : 31.03, gpsZoom)
                    property int baseTX: Math.floor(txf)
                    property int baseTY: Math.floor(tyf)
                    property real fracX: txf - baseTX
                    property real fracY: tyf - baseTY

                    property int tilesH: Math.ceil(width / 256) + 2
                    property int tilesV: Math.ceil(height / 256) + 2

                    // Tile grid
                    Repeater {
                        model: gpsTileMap.tilesH * gpsTileMap.tilesV
                        delegate: Image {
                            property int col: (index % gpsTileMap.tilesH) - Math.floor(gpsTileMap.tilesH / 2)
                            property int row: Math.floor(index / gpsTileMap.tilesH) - Math.floor(gpsTileMap.tilesV / 2)
                            property int tx: gpsTileMap.baseTX + col
                            property int ty: gpsTileMap.baseTY + row
                            property int maxTile: Math.pow(2, gpsZoom) - 1

                            x: gpsTileMap.width / 2 + (col - gpsTileMap.fracX) * 256
                            y: gpsTileMap.height / 2 + (row - gpsTileMap.fracY) * 256
                            width: 256; height: 256

                            source: (tx >= 0 && ty >= 0 && tx <= maxTile && ty <= maxTile)
                                    ? "https://tile.openstreetmap.org/" + gpsZoom + "/" + tx + "/" + ty + ".png"
                                    : ""
                            asynchronous: true; cache: true
                            fillMode: Image.PreserveAspectFit
                            opacity: status === Image.Ready ? 1.0 : 0.3

                            Rectangle {
                                anchors.fill: parent; color: "#0D1117"
                                visible: parent.status !== Image.Ready
                                Text { anchors.centerIn: parent; text: parent.tx + "/" + parent.ty; color: "#30363D"; font.pixelSize: 9 }
                            }
                        }
                    }

                    // UAV marker overlay on GPS map
                    Canvas {
                        id: gpsOverlayCanvas
                        anchors.fill: parent; z: 5

                        Connections {
                            target: appState
                            function onTelemetryChanged() { gpsOverlayCanvas.requestPaint() }
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()

                            // For now, UAV is at center (lat/lon matches GPS position)
                            // TODO: convert actual lat/lon when available from telemetry
                            var cx = width / 2, cy = height / 2
                            var headingRad = (appState.yaw || 0) * Math.PI / 180

                            // UAV triangle marker
                            ctx.save()
                            ctx.translate(cx, cy)
                            ctx.rotate(-(headingRad - Math.PI / 2))
                            var sz = 14
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

                            // Position label
                            ctx.fillStyle = "#161B22CC"
                            ctx.fillRect(cx + 18, cy - 10, 120, 20)
                            ctx.fillStyle = "#E6EDF3"; ctx.font = "10px Consolas"; ctx.textAlign = "left"
                            ctx.fillText(gpsLat.toFixed(5) + ", " + gpsLon.toFixed(5), cx + 22, cy + 4)
                        }
                    }

                    // GPS map pan/zoom mouse area
                    MouseArea {
                        anchors.fill: parent; z: 4
                        hoverEnabled: true
                        property bool isPanning: false

                        onPressed: function(mouse) {
                            isPanning = true
                            gpsPanStartX = mouse.x; gpsPanStartY = mouse.y
                            gpsPanStartLat = gpsLat; gpsPanStartLon = gpsLon
                        }
                        onPositionChanged: function(mouse) {
                            if (isPanning) {
                                var dxPx = mouse.x - gpsPanStartX
                                var dyPx = mouse.y - gpsPanStartY
                                var lonPerPx = 360.0 / (Math.pow(2, gpsZoom) * 256)
                                gpsLon = gpsPanStartLon - dxPx * lonPerPx
                                // Latitude: approximate using same scale (Mercator distortion ignored for small pans)
                                var latRad = gpsPanStartLat * Math.PI / 180
                                var latPerPx = lonPerPx * Math.cos(latRad)
                                gpsLat = gpsPanStartLat + dyPx * latPerPx
                            }
                        }
                        onReleased: function(mouse) { isPanning = false }
                        onWheel: function(wheel) {
                            if (wheel.angleDelta.y > 0)
                                gpsZoom = Math.min(18, gpsZoom + 1)
                            else
                                gpsZoom = Math.max(2, gpsZoom - 1)
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
                    anchors.left: parent.left; anchors.top: mapModeLabel.bottom; anchors.bottom: parent.bottom
                    anchors.topMargin: 6; anchors.leftMargin: 6; anchors.bottomMargin: 6
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

                        // Mission status line
                        Rectangle {
                            width: parent.width; height: 22; radius: 4
                            color: appState.missionState === "RUNNING" ? "#1A3A4F"
                                  : appState.missionState === "DONE" ? "#1A4A2E"
                                  : appState.missionState === "ABORTED" ? "#6E1A1A" : "#21262D"
                            border.color: "#30363D"
                            Text {
                                anchors.centerIn: parent
                                color: "#C9D1D9"; font.pixelSize: 10; font.family: "Consolas"
                                text: {
                                    if (appState.missionState === "RUNNING")
                                        return "任务进行中  " + (appState.missionCurrentIndex + 1) + " / " + appState.missionTotal
                                    if (appState.missionState === "DONE")
                                        return "任务完成（末点悬停）"
                                    if (appState.missionState === "ABORTED")
                                        return "任务已中止"
                                    return "待发送 " + waypointModel.count + " 个航点"
                                }
                            }
                        }

                        // Mission controls
                        PrimaryButton {
                            width: parent.width; height: 26
                            text: appState.missionState === "RUNNING" ? "中止任务" : "上传航点"
                            fillColor: appState.missionState === "RUNNING" ? "#6E1A1A" : "#1F4E8C"
                            onClicked: {
                                if (appState.missionState === "RUNNING") {
                                    appState.abortMission()
                                } else {
                                    uploadWaypoints()
                                }
                            }
                        }
                        PrimaryButton {
                            width: parent.width; height: 26; text: "返航"; fillColor: "#6E1A1A"
                            onClicked: appState.issueCommand("Land")
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
                id: dataView
                anchors.fill: parent
                visible: centerView === 2

                // 本地累积的 Zenith 状态快照（stdout 风格）
                property string zenithStateLog: ""
                property bool waitingLogged: false

                Timer {
                    running: dataView.visible && appState.connected
                    interval: 1000
                    repeat: true
                    onTriggered: {
                        function f(v, d) { return Number(v).toFixed(d === undefined ? 2 : d) }
                        function pad(s, n) { s = String(s); while (s.length < n) s += " "; return s }
                        var now = new Date().toTimeString().substring(0, 8)
                        if (!appState.telemetryStable) {
                            // 机载重启/换电池后 bridge 先上线、UAVSTATE 后到。这段窗口里
                            // 所有数值都还是默认值 0.00，打成快照会被当成真实读数。
                            if (!dataView.waitingLogged) {
                                dataView.zenithStateLog = "[" + now + "] 已建立连接，等待遥测数据…

"
                                                        + dataView.zenithStateLog
                                dataView.waitingLogged = true
                            }
                            return
                        }
                        dataView.waitingLogged = false
                        var snap = "[" + now + "] " + ">>>>>>>>>>>>>>>>>>>> UAV State <<<<<<<<<<<<<<<<<<<<\n"
                            + "PX4 Status   : [ " + (appState.connected ? "Connected" : "Disconnected") + " ] "
                            + "[ " + (appState.armed ? "Armed" : "DisArmed") + " ] "
                            + "[ " + (appState.flightMode || "UNKNOWN") + " ]\n"
                            + "Location     : [ " + (appState.locationSource || "?") + " ]\n"
                            + "Odom Status  : [ " + (appState.odomValid ? "Valid" : "Invalid") + " ]\n"
                            + "Arm Check    : [ " + (appState.armed ? "飞行中，不适用"
                                : !appState.preflightValid ? "无数据"
                                : !appState.preflightArmOk ? "不可解锁"
                                : (appState.preflightPrearmBit ? "可解锁" : "传感器自检通过")) + " ]"
                            + ((!appState.armed && appState.preflightValid
                                && !appState.preflightArmOk && appState.preflightFail)
                                ? "  故障: " + appState.preflightFail : "")
                            + ((!appState.armed && appState.preflightArmAck > 0)
                                ? "  上次解锁被拒: " + appState.preflightArmAckText : "") + "\n"
                            + "VINS_pos [m] : X=" + f(appState.vinsPositionX) + "  Y=" + f(appState.vinsPositionY) + "  Z=" + f(appState.vinsPositionZ) + "\n"
                            + "UAV_pos [m]  : X=" + f(appState.positionX) + "  Y=" + f(appState.positionY) + "  Z=" + f(appState.positionZ) + "\n"
                            + "UAV_vel [m/s]: X=" + f(appState.velocityX) + "  Y=" + f(appState.velocityY) + "  Z=" + f(appState.velocityZ) + "\n"
                            + "UAV_att [deg]: R=" + f(appState.roll) + "  P=" + f(appState.pitch) + "  Y=" + f(appState.yaw) + "\n"
                            + "Battery      : " + f(appState.batteryVoltage) + " V  " + f(appState.batteryPercent * 100, 0) + "%\n\n"
                        dataView.zenithStateLog = snap + dataView.zenithStateLog
                        // 限制总长度，保留约最近 50 条快照
                        if (dataView.zenithStateLog.length > 30000) {
                            dataView.zenithStateLog = dataView.zenithStateLog.substring(0, 30000)
                        }
                    }
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    // Message feedback
                    Rectangle {
                        width: parent.width; height: 60
                        radius: 8; color: "#161B22"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 4
                            Text { text: "消息反馈"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                            Rectangle { width: parent.width; height: 1; color: "#262C36" }
                            Text {
                                width: parent.width
                                text: appState.commandAck.length > 0 ? appState.commandAck : "Zenith Link Ready"
                                color: "#58A6FF"; font.pixelSize: 12; wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // ── 任务实时日志（机载 /rosout 经数传转发）──
                    Rectangle {
                        width: parent.width; height: 220
                        radius: 8; color: "#161B22"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 6

                            Row {
                                width: parent.width; spacing: 8
                                Text {
                                    text: "任务实时日志"; color: "#8B949E"
                                    font.pixelSize: 11; font.bold: true
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Item { width: parent.width - 150; height: 1 }
                                Rectangle {
                                    width: 44; height: 18; radius: 4
                                    color: "#21262D"; border.color: "#30363D"; border.width: 1
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text { anchors.centerIn: parent; text: "清空"; color: "#8B949E"; font.pixelSize: 9 }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: appState.clearMissionLog()
                                    }
                                }
                            }
                            Rectangle { width: parent.width; height: 1; color: "#262C36" }

                            ScrollView {
                                width: parent.width
                                height: parent.height - 32
                                clip: true
                                TextArea {
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextArea.NoWrap
                                    font.family: "Consolas"
                                    font.pixelSize: 10
                                    color: "#C9D1D9"
                                    background: null
                                    text: appState.missionLogText.length > 0
                                          ? appState.missionLogText
                                          : "（等待机载任务日志…启动任务后这里会实时刷新）"
                                }
                            }
                        }
                    }

                    // Flight recorder
                    Rectangle {
                        width: parent.width; height: 72
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

                    // Zenith 状态控制台（1Hz 累计快照，stdout 风格）
                    Rectangle {
                        width: parent.width
                        height: (parent.height - 60 - 72 - 30) * 0.6
                        radius: 8; color: "#0D1117"; border.color: "#30363D"

                        Column {
                            anchors.fill: parent; anchors.margins: 10; spacing: 6
                            Row {
                                width: parent.width; spacing: 8
                                Text { text: "Zenith 状态"; color: "#8B949E"; font.pixelSize: 11; font.bold: true }
                                Item { width: parent.width - 80; height: 1 }
                                Text {
                                    text: "清空"; color: "#58A6FF"; font.pixelSize: 10
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: dataView.zenithStateLog = ""
                                    }
                                }
                            }
                            Rectangle { width: parent.width; height: 1; color: "#262C36" }
                            ScrollView {
                                width: parent.width
                                height: parent.parent.height - 40
                                clip: true
                                TextArea {
                                    readOnly: true; wrapMode: TextArea.NoWrap
                                    text: dataView.zenithStateLog
                                    font.pixelSize: 11; font.family: "Consolas"; color: "#7EE787"
                                    background: null
                                }
                            }
                        }
                    }

                    // Protocol log
                    Rectangle {
                        width: parent.width
                        height: (parent.height - 60 - 72 - 30) * 0.4
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

            // ═══════════════════════════════════════
            //  3D Grid Map View (centerView === 3)
            // ═══════════════════════════════════════
            Item {
                id: gridMapRoot
                anchors.fill: parent
                visible: centerView === 3

                property real camYaw: 45
                property real camPitch: -35
                property real camDist: 15
                property real camTX: 0
                property real camTY: 0
                property real camTZ: 0
                property bool goalMode: false
                property real goalX: 0
                property real goalY: 0
                property real goalZ: 0
                property bool goalVisible: false

                // EGO 目标高度（米）。EGO 只取点击处的水平坐标，高度由地面站指定。
                //
                // 上限必须**低于**机载 ego_planner_odin.launch 里的 max_height(1.5)：
                // 那个值同时是 grid_map 的 z 顶，而目标点落在地图外会被判为占据，
                // 规划直接失败、traj_server 零输出（213 实测 z=2.5 时 position_cmd
                // 一帧都没有）。留 0.1m 余量。
                readonly property real goalAltMin: 0.5
                readonly property real goalAltMax: 1.4
                property real goalAlt: 1.2

                readonly property string egoTaskId: "ego_planner_odin"
                readonly property string egoTaskPath: "/home/jetson/task_ws/src/user_tasks/ego_planner_odin.launch"
                readonly property bool egoRunning:
                    appState.managedTaskActive && appState.managedTaskName === egoTaskId

                function startEgo() {
                    appState.startCustomManagedTask(egoTaskId, egoTaskPath)
                }

                // 目标点下发。机载 bridge 里没有 goal 的发布通道，只能借 CUSTOMMODE 的
                // system() 在机上跑 rostopic pub。x/y/z 是 ENU（map 系）。
                function sendGoal(enuX, enuY, enuZ) {
                    var goalCmd = "rostopic pub -1 /move_base_simple/goal geometry_msgs/PoseStamped "
                        + "'{header: {frame_id: \"world\"}, pose: {position: {x: "
                        + enuX.toFixed(2) + ", y: " + enuY.toFixed(2) + ", z: " + enuZ.toFixed(2)
                        + "}, orientation: {w: 1}}}'"
                    appState.sendRemoteScript(goalCmd)
                }

                function updateCam() {
                    var yr = camYaw * Math.PI / 180
                    var pr = camPitch * Math.PI / 180
                    var cp = Math.cos(pr)
                    gridCam.position = Qt.vector3d(
                        camTX + camDist * cp * Math.sin(yr),
                        camTY - camDist * Math.sin(pr),
                        camTZ + camDist * cp * Math.cos(yr)
                    )
                    gridCam.lookAt(Qt.vector3d(camTX, camTY, camTZ))
                }

                Component.onCompleted: updateCam()

                View3D {
                    id: gridView3d
                    anchors.fill: parent

                    environment: SceneEnvironment {
                        clearColor: "#0D1117"
                        backgroundMode: SceneEnvironment.Color
                    }

                    PerspectiveCamera { id: gridCam; clipNear: 0.1; clipFar: 500 }

                    DirectionalLight {
                        eulerRotation.x: -45; eulerRotation.y: 30
                        brightness: 0.8; ambientColor: Qt.rgba(0.4, 0.4, 0.45, 1.0)
                    }

                    // Invisible pick plane (large, at Y=0)
                    Model {
                        id: pickPlane
                        position: Qt.vector3d(0, -0.01, 0)
                        scale: Qt.vector3d(2.0, 0.0001, 2.0)
                        source: "#Cube"
                        pickable: true
                        materials: PrincipledMaterial { baseColor: "black"; opacity: 0.0 }
                    }

                    // Goal marker
                    Node {
                        visible: gridMapRoot.goalVisible
                        position: Qt.vector3d(gridMapRoot.goalX, 0, gridMapRoot.goalY)

                        // Vertical pole
                        Model {
                            position: Qt.vector3d(0, 1.5, 0)
                            scale: Qt.vector3d(0.00008, 0.03, 0.00008)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: "#F0883E"; lighting: PrincipledMaterial.NoLighting }
                        }
                        // Diamond top
                        Model {
                            position: Qt.vector3d(0, 3.2, 0)
                            eulerRotation.z: 45
                            scale: Qt.vector3d(0.002, 0.002, 0.002)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: "#F0883E"; lighting: PrincipledMaterial.NoLighting }
                        }
                        // Base ring
                        Model {
                            scale: Qt.vector3d(0.004, 0.0003, 0.004)
                            source: "#Cylinder"
                            materials: PrincipledMaterial { baseColor: "#F0883E"; opacity: 0.5; lighting: PrincipledMaterial.NoLighting }
                        }
                    }

                    // Ground grid lines (Z=0 plane, 20m range, 1m spacing)
                    Repeater3D {
                        model: 41
                        Model {
                            property real offset: (index - 20)
                            position: Qt.vector3d(offset, 0, 0)
                            scale: Qt.vector3d(0.00005, 0.00005, 0.40)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: Math.abs(offset) < 0.01 ? "#3A4258" : "#1C2333"; lighting: PrincipledMaterial.NoLighting }
                        }
                    }
                    Repeater3D {
                        model: 41
                        Model {
                            property real offset: (index - 20)
                            position: Qt.vector3d(0, 0, offset)
                            scale: Qt.vector3d(0.40, 0.00005, 0.00005)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: Math.abs(offset) < 0.01 ? "#3A4258" : "#1C2333"; lighting: PrincipledMaterial.NoLighting }
                        }
                    }

                    // Origin axes: Red=X(East), Green=Y(North→-Z), Blue=Z(Up→Y)
                    Model { position: Qt.vector3d(1, 0, 0); scale: Qt.vector3d(0.02, 0.0001, 0.0001); source: "#Cube"; materials: PrincipledMaterial { baseColor: "#F85149"; lighting: PrincipledMaterial.NoLighting } }
                    Model { position: Qt.vector3d(0, 0, -1); scale: Qt.vector3d(0.0001, 0.0001, 0.02); source: "#Cube"; materials: PrincipledMaterial { baseColor: "#3FB950"; lighting: PrincipledMaterial.NoLighting } }
                    Model { position: Qt.vector3d(0, 1, 0); scale: Qt.vector3d(0.0001, 0.02, 0.0001); source: "#Cube"; materials: PrincipledMaterial { baseColor: "#58A6FF"; lighting: PrincipledMaterial.NoLighting } }

                    // Voxels
                    Model {
                        source: "#Cube"
                        instancing: VoxelInstanceTable { id: overviewVoxelTable; store: telemetryStore }
                        materials: PrincipledMaterial { baseColor: "white" }
                    }

                    // UAV trail (blue dots)
                    Model {
                        source: "#Sphere"
                        instancing: TrailInstanceTable { store: telemetryStore }
                        materials: PrincipledMaterial { baseColor: "white"; lighting: PrincipledMaterial.NoLighting }
                    }

                    // EGO planned path (green dots)
                    Model {
                        source: "#Sphere"
                        instancing: PlannedPathInstanceTable { store: telemetryStore }
                        materials: PrincipledMaterial { baseColor: "white"; lighting: PrincipledMaterial.NoLighting }
                    }

                    // UAV body + yaw-aligned axes
                    Node {
                        visible: appState.connected
                        position: Qt.vector3d(appState.positionX, appState.positionZ, -appState.positionY)
                        eulerRotation.y: appState.yaw

                        // UAV body (small sphere)
                        Model {
                            scale: Qt.vector3d(0.002, 0.002, 0.002)
                            source: "#Sphere"
                            materials: PrincipledMaterial { baseColor: "#E6EDF3"; lighting: PrincipledMaterial.NoLighting }
                        }
                        // Forward axis (red, X/East in body frame)
                        Model {
                            position: Qt.vector3d(0.3, 0, 0)
                            scale: Qt.vector3d(0.006, 0.00008, 0.00008)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: "#F85149"; lighting: PrincipledMaterial.NoLighting }
                        }
                        // Left axis (green, Y/North in body frame)
                        Model {
                            position: Qt.vector3d(0, 0, -0.3)
                            scale: Qt.vector3d(0.00008, 0.00008, 0.006)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: "#3FB950"; lighting: PrincipledMaterial.NoLighting }
                        }
                        // Up axis (blue, Z)
                        Model {
                            position: Qt.vector3d(0, 0.3, 0)
                            scale: Qt.vector3d(0.00008, 0.006, 0.00008)
                            source: "#Cube"
                            materials: PrincipledMaterial { baseColor: "#58A6FF"; lighting: PrincipledMaterial.NoLighting }
                        }
                    }
                }

                // Orbit + Goal click controls
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    property real lx: 0; property real ly: 0; property int btn: 0
                    property bool dragged: false

                    onPressed: function(m) { lx = m.x; ly = m.y; btn = m.button; dragged = false }
                    onReleased: function(m) {
                        if (btn === Qt.LeftButton && gridMapRoot.goalMode && !dragged) {
                            var result = gridView3d.pick(m.x, m.y)
                            if (result.objectHit) {
                                var sp = result.scenePosition
                                // Qt3D: X=East, Z=-North → ENU
                                gridMapRoot.goalX = sp.x
                                gridMapRoot.goalY = sp.z
                                gridMapRoot.goalZ = gridMapRoot.goalAlt
                                gridMapRoot.goalVisible = true
                                gridMapRoot.sendGoal(sp.x, -sp.z, gridMapRoot.goalAlt)
                            }
                        }
                        btn = 0
                    }
                    onPositionChanged: function(m) {
                        if (!btn) return
                        var dx = m.x - lx, dy = m.y - ly
                        if (Math.abs(dx) > 3 || Math.abs(dy) > 3) dragged = true
                        lx = m.x; ly = m.y

                        if (gridMapRoot.goalMode && btn === Qt.LeftButton) return

                        if (btn === Qt.LeftButton) {
                            parent.camYaw += dx * 0.3
                            parent.camPitch = Math.max(-89, Math.min(89, parent.camPitch - dy * 0.3))
                        } else {
                            var sp = parent.camDist * 0.003, yr = parent.camYaw * Math.PI / 180
                            parent.camTX -= (dx * Math.cos(yr) + dy * Math.sin(yr)) * sp * 0.5
                            parent.camTZ += (dx * Math.sin(yr) - dy * Math.cos(yr)) * sp * 0.5
                        }
                        parent.updateCam()
                    }
                    onWheel: function(w) {
                        parent.camDist *= w.angleDelta.y > 0 ? 0.9 : 1.1
                        parent.camDist = Math.max(1, Math.min(100, parent.camDist))
                        parent.updateCam()
                    }
                }

                // Info overlay (bottom-left)
                Row {
                    anchors.bottom: parent.bottom; anchors.left: parent.left
                    anchors.margins: 8; spacing: 6; z: 10

                    Rectangle {
                        width: voxInfo.width + 12; height: 22; radius: 4; color: "#21262DCC"
                        Text { id: voxInfo; anchors.centerIn: parent; text: overviewVoxelTable.voxelCount + " voxels"; color: "#3FB950"; font.pixelSize: 10 }
                    }
                    Rectangle {
                        width: focBtn.width + 12; height: 22; radius: 4; color: focMa.containsMouse ? "#30363D" : "#21262DCC"
                        Text { id: focBtn; anchors.centerIn: parent; text: "Focus UAV"; color: "#C9D1D9"; font.pixelSize: 10 }
                        MouseArea { id: focMa; anchors.fill: parent; hoverEnabled: true; onClicked: { gridMapRoot.camTX = appState.positionX; gridMapRoot.camTY = appState.positionZ; gridMapRoot.camTZ = -appState.positionY; gridMapRoot.updateCam() } }
                    }
                    Rectangle {
                        width: goalBtn.width + 12; height: 22; radius: 4
                        color: gridMapRoot.goalMode ? "#1F6FEB" : (goalMa.containsMouse ? "#30363D" : "#21262DCC")
                        border.color: gridMapRoot.goalMode ? "#58A6FF" : "transparent"
                        Text { id: goalBtn; anchors.centerIn: parent; text: gridMapRoot.goalMode ? "Goal Mode ON" : "Set Goal"; color: gridMapRoot.goalMode ? "#FFF" : "#C9D1D9"; font.pixelSize: 10 }
                        MouseArea { id: goalMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: gridMapRoot.goalMode = !gridMapRoot.goalMode }
                    }
                    Rectangle {
                        visible: gridMapRoot.goalVisible
                        width: clearGoalBtn.width + 12; height: 22; radius: 4; color: clearMa.containsMouse ? "#6E1A1A" : "#21262DCC"
                        Text { id: clearGoalBtn; anchors.centerIn: parent; text: "Clear Goal"; color: "#F85149"; font.pixelSize: 10 }
                        MouseArea { id: clearMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: gridMapRoot.goalVisible = false }
                    }
                }

                // Planner controls (bottom-right)
                Row {
                    anchors.bottom: parent.bottom; anchors.right: parent.right
                    anchors.margins: 8; spacing: 6; z: 10

                    // 目标高度：EGO 只用点击位置的水平坐标，高度由这里给
                    Rectangle {
                        width: altRow.width + 16; height: 26; radius: 6; color: "#161B22CC"; border.color: "#30363D"
                        Row {
                            id: altRow; anchors.centerIn: parent; spacing: 4
                            Text { text: "目标高度"; color: "#8B949E"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                            Rectangle {
                                width: 16; height: 18; radius: 3
                                color: altDownMa.containsMouse ? "#30363D" : "#21262D"
                                anchors.verticalCenter: parent.verticalCenter
                                Text { anchors.centerIn: parent; text: "−"; color: "#C9D1D9"; font.pixelSize: 11 }
                                MouseArea {
                                    id: altDownMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: gridMapRoot.goalAlt = Math.max(gridMapRoot.goalAltMin, gridMapRoot.goalAlt - 0.1)
                                }
                            }
                            Text {
                                text: gridMapRoot.goalAlt.toFixed(1) + " m"
                                color: "#E6EDF3"; font.pixelSize: 10; font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Rectangle {
                                width: 16; height: 18; radius: 3
                                color: altUpMa.containsMouse ? "#30363D" : "#21262D"
                                anchors.verticalCenter: parent.verticalCenter
                                Text { anchors.centerIn: parent; text: "+"; color: "#C9D1D9"; font.pixelSize: 11 }
                                MouseArea {
                                    id: altUpMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: gridMapRoot.goalAlt = Math.min(gridMapRoot.goalAltMax, gridMapRoot.goalAlt + 0.1)
                                }
                            }
                        }
                    }

                    // EGO：走机载任务管理器（Managed Task API），不再下发 shell
                    Rectangle {
                        width: egoRow.width + 16; height: 26; radius: 6; color: "#161B22CC"
                        border.color: gridMapRoot.egoRunning ? "#2EA043" : "#30363D"
                        Row {
                            id: egoRow; anchors.centerIn: parent; spacing: 5
                            Text { text: "EGO"; color: "#58A6FF"; font.pixelSize: 10; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: gridMapRoot.egoRunning ? appState.managedTaskState : "未启动"
                                color: gridMapRoot.egoRunning ? "#3FB950" : "#6E7681"
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Rectangle {
                                width: startLbl.width + 12; height: 18; radius: 3
                                readonly property bool canStart: appState.protocolConnected && !appState.managedTaskActive
                                color: !canStart ? "#21262D" : (startMa.containsMouse ? Qt.lighter("#1A4A2E", 1.3) : "#1A4A2E")
                                anchors.verticalCenter: parent.verticalCenter
                                Text { id: startLbl; anchors.centerIn: parent; text: "启动"; color: parent.canStart ? "#E6EDF3" : "#6E7681"; font.pixelSize: 9 }
                                MouseArea {
                                    id: startMa; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: parent.canStart ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: if (parent.canStart) gridMapRoot.startEgo()
                                }
                            }
                            Rectangle {
                                width: stopLbl.width + 12; height: 18; radius: 3
                                readonly property bool canStop: appState.protocolConnected && gridMapRoot.egoRunning
                                color: !canStop ? "#21262D" : (stopMa.containsMouse ? Qt.lighter("#6E1A1A", 1.3) : "#6E1A1A")
                                anchors.verticalCenter: parent.verticalCenter
                                Text { id: stopLbl; anchors.centerIn: parent; text: "停止"; color: parent.canStop ? "#E6EDF3" : "#6E7681"; font.pixelSize: 9 }
                                MouseArea {
                                    id: stopMa; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: parent.canStop ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: if (parent.canStop) appState.stopManagedTask(gridMapRoot.egoTaskId)
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { width: 8; height: 1 }   // spacer

        // ═══════════════════════════════════════
        //  Right: Dashboard Panel (250px)
        // ═══════════════════════════════════════
        Rectangle {
            width: 250; height: parent.height
            color: "#161B22"; radius: 8; border.color: "#21262D"

            Flickable {
                anchors.fill: parent; anchors.margins: 1
                contentHeight: dashCol.height + 16
                clip: true; boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: dashCol
                    width: parent.width; padding: 10; spacing: 10

                    // ── Dashboard title ──
                    Column {
                        width: parent.width - 20; spacing: 2
                        Text { text: "Dashboard"; color: "#E6EDF3"; font.pixelSize: 14; font.bold: true }
                        Text { text: "Zenith 上位机状态"; color: "#6E7681"; font.pixelSize: 10 }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── FLIGHT STATUS ──
                    Column {
                        width: parent.width - 20; spacing: 6
                        Text { text: "FLIGHT STATUS"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }

                        Row {
                            width: parent.width; spacing: 8
                            Column {
                                spacing: 2
                                Text { text: "Exec State"; color: "#6E7681"; font.pixelSize: 9 }
                                Text { text: appState.telemetryStable ? appState.execState : "N/A"; color: "#E6EDF3"; font.pixelSize: 14; font.bold: true }
                            }
                            Item { width: parent.width - 150; height: 1 }
                            Rectangle {
                                width: safeBadge.width + 14; height: 22; radius: 4
                                color: appState.failsafe ? Qt.rgba(0.973, 0.318, 0.286, 0.15) : Qt.rgba(0.247, 0.725, 0.314, 0.15)
                                anchors.verticalCenter: parent.verticalCenter
                                Text { id: safeBadge; anchors.centerIn: parent; text: appState.failsafe ? "FAILSAFE" : "SAFE"; color: appState.failsafe ? "#F85149" : "#3FB950"; font.pixelSize: 10; font.bold: true }
                            }
                        }

                        Row {
                            spacing: 16
                            DashDot { dotColor: appState.connected ? "#3FB950" : "#F85149"; label: "Link" }
                            DashDot { dotColor: appState.connected && appState.armed ? "#FFA657" : "#484F58"; label: "Armed" }
                            DashDot { dotColor: appState.connected && appState.locationSource !== "UNKNOWN" ? "#3FB950" : "#484F58"; label: "VIO" }
                        }

                        // ── 紧凑状态行 ──
                        Row {
                            width: parent.width; spacing: 0
                            StatusChip { label: appState.telemetryStable ? appState.locationSource : "--"; chipColor: "#1F3D6F" }
                            Item { width: 4; height: 1 }
                            StatusChip { label: appState.telemetryStable ? Number(appState.homeDistance).toFixed(1) + "m" : "--"; chipColor: "#1A3D2E" }
                            Item { width: 4; height: 1 }
                            StatusChip { label: (appState.connected && appState.batteryValid) ? Number(appState.batteryVoltage).toFixed(1) + "V" : "--"; chipColor: appState.batteryVoltage > 0 && appState.batteryVoltage < 14.0 ? "#6E1A1A" : "#1A3D2E" }
                        }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── COMMAND FEEDBACK ──
                    Column {
                        width: parent.width - 20; spacing: 4
                        Text { text: "COMMAND"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                        Row {
                            width: parent.width; spacing: 6
                            Text { text: "最后指令"; color: "#6E7681"; font.pixelSize: 10 }
                            Text { text: appState.lastCommand || "--"; color: "#E6EDF3"; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight; width: parent.width - 70 }
                        }
                        Row {
                            width: parent.width; spacing: 6
                            Text { text: "指令回执"; color: "#6E7681"; font.pixelSize: 10 }
                            Text { text: appState.commandAck || "--"; color: "#58A6FF"; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight; width: parent.width - 70 }
                        }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── NAVIGATION ──
                    Column {
                        width: parent.width - 20; spacing: 6
                        Text { text: "NAVIGATION"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }

                        TelemetryGroup { title: "位置 [m]"; labels: ["X","Y","Z"]
                            values: appState.telemetryStable ? [ fmt(appState.positionX, 2), fmt(appState.positionY, 2), fmt(appState.positionZ, 2) ] : ["--","--","--"] }
                        TelemetryGroup { title: "速度 [m/s]"; labels: ["X","Y","Z"]
                            values: appState.telemetryStable ? [ fmt(appState.velocityX, 2), fmt(appState.velocityY, 2), fmt(appState.velocityZ, 2) ] : ["--","--","--"] }
                        TelemetryGroup { title: "姿态 [deg]"; labels: ["R","P","Y"]
                            values: appState.telemetryStable ? [ fmt(appState.roll, 1), fmt(appState.pitch, 1), fmt(appState.yaw, 1) ] : ["--","--","--"] }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── QUICK COMMANDS (collapsible) ──
                    Column {
                        id: quickCmdSection
                        width: parent.width - 20; spacing: 6
                        property bool expanded: true

                        Item {
                            width: parent.width; height: 16
                            Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "QUICK COMMANDS"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                            Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: quickCmdSection.expanded ? "\u25BC" : "\u25B6"; color: "#6E7681"; font.pixelSize: 8 }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: quickCmdSection.expanded = !quickCmdSection.expanded }
                        }

                        Column {
                            visible: quickCmdSection.expanded; width: parent.width; spacing: 4

                            // ── 解锁前检查：PX4 SYS_STATUS 传感器健康位（机载 preflight_reporter 上报）──
                            Rectangle {
                                id: preflightPanel
                                // 解锁检查是起飞前判据。飞机一旦解锁（尤其低电量飞行时
                                // SYS_STATUS 的电池健康位会翻假），再把它渲染成红色"不可解锁"
                                // 就是误报——此时既不需要解锁，也无法据此做任何处置。
                                property bool pfApplicable: !appState.armed
                                width: parent.width
                                height: pfCol.implicitHeight + 12
                                radius: 5
                                color: "#0D1117"
                                border.width: 1
                                border.color: (!preflightPanel.pfApplicable || !appState.preflightValid) ? "#30363D"
                                            : (appState.preflightArmOk ? "#2A5A34" : "#6E2B2B")

                                Column {
                                    id: pfCol
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 3

                                    Row {
                                        spacing: 6
                                        Rectangle {
                                            width: 8; height: 8; radius: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: (!preflightPanel.pfApplicable || !appState.preflightValid) ? "#484F58"
                                                 : (appState.preflightArmOk ? "#3FB950" : "#F85149")
                                        }
                                        Text {
                                            // 措辞按可信度区分：飞控上报 PREARM_CHECK 位时才是权威判据，
                                            // 否则只代表"传感器自检"，不能等同于飞控允许解锁。
                                            text: !preflightPanel.pfApplicable ? "飞行中 — 解锁检查不适用"
                                                : !appState.preflightValid ? "解锁检查 — 无数据"
                                                : !appState.preflightArmOk ? "不可解锁"
                                                : (appState.preflightPrearmBit ? "可安全解锁" : "传感器自检通过")
                                            color: (!preflightPanel.pfApplicable || !appState.preflightValid) ? "#8B949E"
                                                 : (appState.preflightArmOk ? "#3FB950" : "#F85149")
                                            font.pixelSize: 11; font.bold: true
                                        }
                                    }

                                    // 自检通过但飞控未提供权威位时，明确提示这不是完整判据
                                    Text {
                                        visible: preflightPanel.pfApplicable && appState.preflightValid
                                                 && appState.preflightArmOk && !appState.preflightPrearmBit
                                        width: parent.width
                                        wrapMode: Text.WordWrap
                                        text: "仅传感器健康位，未覆盖遥控/模式/安全开关/参数等检查"
                                        color: "#8B949E"; font.pixelSize: 9
                                    }

                                    // 上次解锁尝试被飞控拒绝 —— 这是 100% 真实的拒绝信号
                                    Row {
                                        // 解锁成功后，上一次被拒的记录已经是历史，继续挂红字只会误导
                                        visible: preflightPanel.pfApplicable && appState.preflightArmAck > 0
                                        spacing: 5
                                        Text { text: "⚠"; color: "#D29922"; font.pixelSize: 10 }
                                        Text {
                                            text: "上次解锁被飞控拒绝：" + appState.preflightArmAckText
                                            color: "#D29922"; font.pixelSize: 10
                                        }
                                    }

                                    // 仅列出故障项；全部正常时不占地方
                                    Repeater {
                                        model: appState.preflightChecks
                                        delegate: Row {
                                            visible: preflightPanel.pfApplicable && !modelData.healthy
                                            spacing: 5
                                            Text { text: "✕"; color: "#F85149"; font.pixelSize: 10 }
                                            Text {
                                                text: modelData.name + (modelData.enabled ? "" : "（未启用）")
                                                color: "#D9A2A2"; font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }

                            // ── 重启飞控 + ODIN 定位 + 控制状态机（获得干净状态机）──
                            // 双重保护：已解锁时禁用 + 长按 1.5s 确认。
                            // 机载脚本自身也会再查一次 armed，解锁状态下拒绝执行。
                            Rectangle {
                                id: rebootBtn
                                width: parent.width; height: 32; radius: 5
                                property bool ready: appState.connected && !appState.armed
                                property real holdProgress: 0.0
                                color: ready ? (holdProgress > 0 ? "#7A2E2E" : "#4A2020") : "#2A1C1C"
                                border.color: ready ? "#C25050" : "#4A3535"; border.width: 1

                                Rectangle {
                                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                    width: parent.width * rebootBtn.holdProgress; radius: 5
                                    color: "#C25050"; opacity: 0.45
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: !appState.connected ? "重启机载栈 — 未连接"
                                        : appState.armed ? "重启机载栈 — 已解锁，禁止"
                                        : (rebootBtn.holdProgress > 0 ? "重启中… 松手取消"
                                                                      : "⟳ 重启飞控+定位+状态机（长按 1.5s）")
                                    color: rebootBtn.ready ? "#FFD9D9" : "#6E5050"
                                    font.pixelSize: 11; font.bold: true
                                }
                                Timer {
                                    id: rebootTimer; interval: 50; repeat: true
                                    onTriggered: {
                                        rebootBtn.holdProgress += 0.05 / 1.5
                                        if (rebootBtn.holdProgress >= 1.0) {
                                            rebootTimer.stop()
                                            rebootBtn.holdProgress = 0.0
                                            appState.sendRemoteScript("bash /opt/zenith/bin/zenith_reboot_stack.sh")
                                        }
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: rebootBtn.ready
                                    cursorShape: rebootBtn.ready ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onPressed: { rebootBtn.holdProgress = 0.0; rebootTimer.start() }
                                    onReleased: { rebootTimer.stop(); rebootBtn.holdProgress = 0.0 }
                                    onCanceled: { rebootTimer.stop(); rebootBtn.holdProgress = 0.0 }
                                }
                            }

                            // 起飞按钮：长按 1.2s 确认（防误触）
                            // ready 条件加入解锁前检查：有上报且判定不可解锁时禁用（无上报则不阻断，保持旧行为）
                            Rectangle {
                                id: takeoffBtn
                                width: parent.width; height: 32; radius: 5
                                property bool ready: !appState.armed && appState.odomValid && appState.connected && !appState.failsafe
                                                     && (!appState.preflightValid || appState.preflightArmOk)
                                property real holdProgress: 0.0
                                color: ready ? (holdProgress > 0 ? "#B85A1A" : "#7A3F0E") : "#3A2A1A"
                                border.color: ready ? "#E07A30" : "#5A3F30"; border.width: 1
                                // 进度条
                                Rectangle {
                                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                    width: parent.width * takeoffBtn.holdProgress; radius: 5
                                    color: "#E07A30"; opacity: 0.45
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: takeoffBtn.ready ? (takeoffBtn.holdProgress > 0 ? "起飞中… 松手取消" : "▲ 起飞到 1m（长按 1.2s）") : "起飞 — 不可用"
                                    color: takeoffBtn.ready ? "#FFEED8" : "#6E5A48"; font.pixelSize: 11; font.bold: true
                                }
                                Timer {
                                    id: takeoffTimer; interval: 50; repeat: true
                                    onTriggered: {
                                        takeoffBtn.holdProgress += 0.05 / 1.2
                                        if (takeoffBtn.holdProgress >= 1.0) {
                                            takeoffBtn.holdProgress = 0
                                            stop()
                                            appState.takeoffTo(1.0)
                                        }
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: takeoffBtn.ready
                                    onPressed: { takeoffBtn.holdProgress = 0.001; takeoffTimer.start() }
                                    onReleased: { takeoffTimer.stop(); takeoffBtn.holdProgress = 0 }
                                    onCanceled: { takeoffTimer.stop(); takeoffBtn.holdProgress = 0 }
                                }
                            }

                            PrimaryButton { width: parent.width; height: 26; text: "当前点悬停"; fillColor: "#1F4E8C"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                            PrimaryButton { width: parent.width; height: 26; text: "初始点悬停"; fillColor: "#1A5C30"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                            PrimaryButton { width: parent.width; height: 26; text: "降落"; fillColor: "#6E1A1A"; enabled: appState.connected; onClicked: appState.issueCommand(text) }

                            Rectangle { width: parent.width; height: 1; color: "#262C36" }

                            PrimaryButton { width: parent.width; height: 26; text: "VIO 重启"; fillColor: "#4A3060"; enabled: appState.connected
                                onClicked: appState.sendRemoteScript("bash /home/orangepi/opi-drone-cxr-demo/vio_recovery.sh") }
                        }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── MANUAL CONTROL (collapsible) ──
                    Column {
                        id: manualCtrlSection
                        width: parent.width - 20; spacing: 6
                        property bool expanded: false

                        Item {
                            width: parent.width; height: 16
                            Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "MANUAL CONTROL"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                            Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: manualCtrlSection.expanded ? "\u25BC" : "\u25B6"; color: "#6E7681"; font.pixelSize: 8 }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: manualCtrlSection.expanded = !manualCtrlSection.expanded }
                        }

                        Column {
                            visible: manualCtrlSection.expanded; width: parent.width; spacing: 6

                            ComboBox {
                                id: manualModeBox
                                width: parent.width; height: 26
                                model: ["XYZ_POS", "XYZ_VEL", "XYZ_POS_BODY", "LAT_LON_ALT"]
                                background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
                                contentItem: Text { leftPadding: 8; text: manualModeBox.currentText; color: "#E6EDF3"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            }

                            Row {
                                width: parent.width; spacing: 6
                                Column {
                                    width: (parent.width - 6) / 2; spacing: 3
                                    Text { text: "X [m]"; color: "#8B949E"; font.pixelSize: 9 }
                                    MiniField { id: xField; width: parent.width; height: 26; liveValue: appState.positionX }
                                }
                                Column {
                                    width: (parent.width - 6) / 2; spacing: 3
                                    Text { text: "Y [m]"; color: "#8B949E"; font.pixelSize: 9 }
                                    MiniField { id: yField; width: parent.width; height: 26; liveValue: appState.positionY }
                                }
                            }
                            Row {
                                width: parent.width; spacing: 6
                                Column {
                                    width: (parent.width - 6) / 2; spacing: 3
                                    Text { text: "Z [m]"; color: "#8B949E"; font.pixelSize: 9 }
                                    MiniField { id: zField; width: parent.width; height: 26; liveValue: appState.positionZ }
                                }
                                Column {
                                    width: (parent.width - 6) / 2; spacing: 3
                                    Text { text: "Yaw [deg]"; color: "#8B949E"; font.pixelSize: 9 }
                                    MiniField { id: yawField; width: parent.width; height: 26; liveValue: appState.yaw }
                                }
                            }

                            Row {
                                width: parent.width; spacing: 6
                                PrimaryButton {
                                    width: (parent.width - 6) * 0.65; height: 30; text: "上传指令"; fillColor: "#1F6FEB"
                                    onClicked: appState.sendManualMove(
                                        manualModeBox.currentText,
                                        xField.userDirty ? Number(xField.text || 0) : appState.positionX,
                                        yField.userDirty ? Number(yField.text || 0) : appState.positionY,
                                        zField.userDirty ? Number(zField.text || 0) : appState.positionZ,
                                        yawField.userDirty ? Number(yawField.text || 0) : appState.yaw)
                                }
                                PrimaryButton {
                                    width: (parent.width - 6) * 0.35; height: 30; text: "重置"; fillColor: "#484F58"
                                    onClicked: {
                                        xField.userDirty = false; xField.text = Number(appState.positionX).toFixed(2)
                                        yField.userDirty = false; yField.text = Number(appState.positionY).toFixed(2)
                                        zField.userDirty = false; zField.text = Number(appState.positionZ).toFixed(2)
                                        yawField.userDirty = false; yawField.text = Number(appState.yaw).toFixed(2)
                                    }
                                }
                            }
                        }
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
        var list = []
        for (var i = 0; i < waypointModel.count; ++i) {
            var wp = waypointModel.get(i)
            list.push({ wx: wp.wx, wy: wp.wy, wz: wp.wz })
        }
        // yaw 由 AppState 按航迹方向计算（B 方案），无需在 QML 端处理
        appState.startMission(list)
    }

    // ═══════════════════════════════════════
    //  Inline Components
    // ═══════════════════════════════════════

    // ── Telemetry card for 2-column grid ──
    component TelCard: Rectangle {
        property string icon: ""
        property color iconColor: "#58A6FF"
        property string label: ""
        property string value: "0.0"
        property string unit: ""

        width: (parent.width - 6) / 2; height: 56; radius: 6
        color: "#0D1117"; border.color: "#21262D"

        Row {
            anchors.fill: parent; anchors.margins: 8; spacing: 6
            Rectangle {
                width: 28; height: 28; radius: 6
                color: Qt.rgba(iconColor.r, iconColor.g, iconColor.b, 0.12)
                anchors.verticalCenter: parent.verticalCenter
                Text { anchors.centerIn: parent; text: icon; color: iconColor; font.pixelSize: 14; font.bold: true }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter; spacing: 1
                Text { text: label; color: "#6E7681"; font.pixelSize: 7; font.bold: true; font.letterSpacing: 0.5 }
                Row {
                    spacing: 2
                    Text { text: value; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas" }
                    Text { text: unit; color: "#6E7681"; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
    }

    // ── Status dot with label for dashboard ──
    component DashDot: Row {
        property color dotColor: "#3FB950"
        property string label: ""
        spacing: 4
        Rectangle { width: 6; height: 6; radius: 3; color: dotColor; anchors.verticalCenter: parent.verticalCenter }
        Text { text: label; color: "#6E7681"; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
    }

    component DashKV: Item {
        property string label: ""
        property string value: ""
        property color valueColor: "#E6EDF3"
        width: parent.width; height: 18
        Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: label; color: "#6E7681"; font.pixelSize: 10 }
        Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: value; color: valueColor; font.pixelSize: 10; font.bold: true; font.family: "Consolas"; elide: Text.ElideRight; width: 130; horizontalAlignment: Text.AlignRight }
    }

    component TelemetryGroup: Rectangle {
        property string title: ""
        property var labels: ["X","Y","Z"]
        property var values: []

        width: parent.width; height: 54; radius: 6
        color: "#0D1117"; border.color: "#21262D"

        Column {
            anchors.fill: parent; anchors.margins: 6; spacing: 2
            Text { text: title; color: "#9AA4B2"; font.pixelSize: 9; font.bold: true }
            Row {
                width: parent.width; height: 26; spacing: 0
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

    component StatusChip: Rectangle {
        property string label: ""
        property color chipColor: "#1F3D6F"
        width: (parent.width - 8) / 3; height: 20; radius: 4
        color: chipColor
        Text { anchors.centerIn: parent; text: label; color: "#E6EDF3"; font.pixelSize: 10; font.bold: true; font.family: "Consolas" }
    }

    component MiniField: TextField {
        id: rootField
        property alias val: rootField.text
        property real liveValue: 0.0
        property bool userDirty: false
        color: rootField.userDirty ? "#FFA657" : "#7EE787"
        font.pixelSize: 11; placeholderTextColor: "#6E7681"
        background: Rectangle {
            radius: 5
            color: "#21262D"
            border.color: rootField.userDirty ? "#58A6FF" : "#30363D"
        }
        Component.onCompleted: text = Number(liveValue).toFixed(2)
        onTextEdited: userDirty = true
        onLiveValueChanged: if (!userDirty) text = Number(liveValue).toFixed(2)
    }
}
