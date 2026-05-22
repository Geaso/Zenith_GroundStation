import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

Item {
    id: root

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
                            Text { text: appState.connected ? Number(appState.positionZ).toFixed(1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "m"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "SPD"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? Number(appState.speed).toFixed(1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "m/s"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "VSPD"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? Number(appState.velocityZ).toFixed(1) : "0.0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "HDG"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? Number(appState.yaw).toFixed(0) : "0"; color: "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "\u00B0"; color: "#6E7681"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { width: 1; height: 16; color: "#30363D"; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            spacing: 3; anchors.verticalCenter: parent.verticalCenter
                            Text { text: "BAT"; color: "#6E7681"; font.pixelSize: 8; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: appState.connected ? Number(appState.batteryVoltage).toFixed(1) : "0.0"; color: appState.batteryPercent < 0.2 ? "#F85149" : "#E6EDF3"; font.pixelSize: 13; font.bold: true; font.family: "Consolas"; anchors.verticalCenter: parent.verticalCenter }
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
                        ctx.fillText((appState.yaw || 0).toFixed(0) + "\u00B0", cx, cy + r + 6)
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
                                Text { text: appState.connected ? appState.execState : "N/A"; color: "#E6EDF3"; font.pixelSize: 14; font.bold: true }
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
                            DashDot { dotColor: appState.connected && appState.heartbeatLink === "OK" ? "#3FB950" : "#F85149"; label: "Heartbeat" }
                            DashDot { dotColor: appState.connected && appState.videoLink === "OK" ? "#3FB950" : "#484F58"; label: "Video" }
                            DashDot { dotColor: appState.connected && appState.rcLink === "OK" ? "#3FB950" : "#484F58"; label: "RC" }
                        }
                    }

                    Rectangle { width: parent.width - 20; height: 1; color: "#262C36" }

                    // ── TELEMETRY — 2-column card grid ──
                    Column {
                        width: parent.width - 20; spacing: 6
                        Text { text: "TELEMETRY"; color: "#8B949E"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }

                        Grid {
                            columns: 2; spacing: 6; width: parent.width

                            TelCard { icon: "\u2699"; iconColor: "#58A6FF"; label: "CTRL MODE"; value: appState.connected ? appState.controllerMode : "--"; unit: "" }
                            TelCard { icon: "\u25B6"; iconColor: "#3FB950"; label: "CTRL STATE"; value: appState.connected ? appState.controlState : "--"; unit: "" }
                            TelCard { icon: "\u2691"; iconColor: "#BC8CFF"; label: "MISSION"; value: appState.connected ? appState.missionMode : "--"; unit: "" }
                            TelCard { icon: "\u27A4"; iconColor: "#FFA657"; label: "STAGE"; value: appState.connected ? appState.missionStage : "--"; unit: "" }
                            TelCard { icon: "\u2295"; iconColor: "#58A6FF"; label: "LOC SRC"; value: appState.connected ? appState.locationSource : "--"; unit: "" }
                            TelCard { icon: "\u2302"; iconColor: "#3FB950"; label: "HOME DIST"; value: appState.connected ? Number(appState.homeDistance).toFixed(1) : "--"; unit: "m" }
                            TelCard { icon: "\u21C4"; iconColor: "#FFA657"; label: "CMD SRC"; value: appState.connected ? appState.activeCommandSource : "--"; unit: "" }
                            TelCard { icon: "\u26A0"; iconColor: appState.alertLevel === "NONE" || !appState.connected ? "#3FB950" : "#F85149"; label: "ALERT"; value: appState.connected ? appState.alertLevel : "--"; unit: "" }
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
                            values: appState.connected ? [ Number(appState.positionX).toFixed(2), Number(appState.positionY).toFixed(2), Number(appState.positionZ).toFixed(2) ] : ["--","--","--"] }
                        TelemetryGroup { title: "速度 [m/s]"; labels: ["X","Y","Z"]
                            values: appState.connected ? [ Number(appState.velocityX).toFixed(2), Number(appState.velocityY).toFixed(2), Number(appState.velocityZ).toFixed(2) ] : ["--","--","--"] }
                        TelemetryGroup { title: "姿态 [deg]"; labels: ["R","P","Y"]
                            values: appState.connected ? [ Number(appState.roll).toFixed(1), Number(appState.pitch).toFixed(1), Number(appState.yaw).toFixed(1) ] : ["--","--","--"] }
                        TelemetryGroup { title: "期望位置 [m]"; labels: ["X","Y","Z"]
                            values: appState.connected ? [ Number(appState.desiredPositionX).toFixed(2), Number(appState.desiredPositionY).toFixed(2), Number(appState.desiredPositionZ).toFixed(2) ] : ["--","--","--"] }
                        TelemetryGroup { title: "期望速度 [m/s]"; labels: ["X","Y","Z"]
                            values: appState.connected ? [ Number(appState.desiredVelocityX).toFixed(2), Number(appState.desiredVelocityY).toFixed(2), Number(appState.desiredVelocityZ).toFixed(2) ] : ["--","--","--"] }
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
                            PrimaryButton { width: parent.width; height: 26; text: "当前点悬停"; fillColor: "#1F4E8C"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                            PrimaryButton { width: parent.width; height: 26; text: "初始点悬停"; fillColor: "#1A5C30"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
                            PrimaryButton { width: parent.width; height: 26; text: "降落"; fillColor: "#6E1A1A"; enabled: appState.connected; onClicked: appState.issueCommand(text) }
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
                                width: parent.width; height: 30; text: "上传指令"; fillColor: "#1F6FEB"
                                onClicked: appState.sendManualMove(manualModeBox.currentText, Number(xField.val || 0), Number(yField.val || 0), Number(zField.val || 0), Number(yawField.val || 0))
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
        for (var i = 0; i < waypointModel.count; ++i) {
            var wp = waypointModel.get(i)
            appState.sendManualMove("XYZ_POS", wp.wx, wp.wy, wp.wz, wp.wyaw)
        }
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

    component MiniField: TextField {
        id: rootField
        property alias val: rootField.text
        color: "#E6EDF3"; font.pixelSize: 11; placeholderTextColor: "#6E7681"
        background: Rectangle { radius: 5; color: "#21262D"; border.color: "#30363D" }
    }
}
