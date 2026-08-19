import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick3D
import Zenith3D 1.0
import ZenithUI 1.0

Item {
    id: root
    anchors.fill: parent

    // Camera orbit state
    property real camYaw: 45
    property real camPitch: -35
    property real camDist: 15
    property real camTargetX: 0
    property real camTargetY: 0
    property real camTargetZ: 0

    function fmt(val, d) {
        var s = Number(val).toFixed(d)
        return (s.charAt(0) === '-' && parseFloat(s) === 0) ? s.substring(1) : s
    }

    function updateCamera() {
        var yr = camYaw * Math.PI / 180
        var pr = camPitch * Math.PI / 180
        var cosPitch = Math.cos(pr)
        camera.position = Qt.vector3d(
            camTargetX + camDist * cosPitch * Math.sin(yr),
            camTargetY - camDist * Math.sin(pr),
            camTargetZ + camDist * cosPitch * Math.cos(yr)
        )
        camera.lookAt(Qt.vector3d(camTargetX, camTargetY, camTargetZ))
    }

    Component.onCompleted: updateCamera()

    // ── Toolbar ──
    Rectangle {
        id: toolbar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 36; color: "#161B22"; z: 10
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#30363D" }

        Row {
            anchors.centerIn: parent; spacing: 12
            Text { text: "3D Grid Map"; color: "#58A6FF"; font.pixelSize: 13; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
            Rectangle { width: 1; height: 20; color: "#30363D" }
            Text { text: voxelTable.voxelCount + " voxels"; color: voxelTable.voxelCount > 0 ? "#3FB950" : "#8B949E"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
            Rectangle { width: 1; height: 20; color: "#30363D" }
            Text { text: "UAV: (" + fmt(appState.positionX,1) + ", " + fmt(appState.positionY,1) + ", " + fmt(appState.positionZ,1) + ")"; color: "#C9D1D9"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
            Rectangle { width: 1; height: 20; color: "#30363D" }

            // Center on UAV button
            Rectangle {
                width: focusLabel.width + 12; height: 24; radius: 4
                color: focusMa.containsMouse ? "#30363D" : "#21262D"; border.color: "#30363D"
                anchors.verticalCenter: parent.verticalCenter
                Text { id: focusLabel; anchors.centerIn: parent; text: "Focus UAV"; color: "#C9D1D9"; font.pixelSize: 10 }
                MouseArea { id: focusMa; anchors.fill: parent; hoverEnabled: true; onClicked: { camTargetX = appState.positionX; camTargetY = appState.positionZ; camTargetZ = -appState.positionY; updateCamera() } }
            }

            // Reset view button
            Rectangle {
                width: resetLabel.width + 12; height: 24; radius: 4
                color: resetMa.containsMouse ? "#30363D" : "#21262D"; border.color: "#30363D"
                anchors.verticalCenter: parent.verticalCenter
                Text { id: resetLabel; anchors.centerIn: parent; text: "Reset View"; color: "#C9D1D9"; font.pixelSize: 10 }
                MouseArea { id: resetMa; anchors.fill: parent; hoverEnabled: true; onClicked: { camYaw = 45; camPitch = -35; camDist = 15; camTargetX = 0; camTargetY = 0; camTargetZ = 0; updateCamera() } }
            }
        }
    }

    // ── 3D View ──
    View3D {
        id: view3d
        anchors.top: toolbar.bottom; anchors.left: parent.left
        anchors.right: parent.right; anchors.bottom: parent.bottom

        environment: SceneEnvironment {
            clearColor: "#0D1117"
            backgroundMode: SceneEnvironment.Color
        }

        PerspectiveCamera {
            id: camera
            clipNear: 0.1
            clipFar: 500
        }

        DirectionalLight {
            eulerRotation.x: -45; eulerRotation.y: 30
            brightness: 0.8
            ambientColor: Qt.rgba(0.4, 0.4, 0.45, 1.0)
        }

        // ── Ground plane (20m x 20m, 1 unit grid) ──
        Model {
            position: Qt.vector3d(0, -0.005, 0)
            scale: Qt.vector3d(0.2, 0.0001, 0.2)
            source: "#Cube"
            materials: PrincipledMaterial { baseColor: "#161B22"; opacity: 0.6 }
        }

        // ── Origin axes (thin, 2m each) ──
        Model { // X axis (red, East)
            position: Qt.vector3d(1, 0, 0)
            scale: Qt.vector3d(0.02, 0.0002, 0.0002)
            source: "#Cube"
            materials: PrincipledMaterial { baseColor: "#F85149"; lighting: PrincipledMaterial.NoLighting }
        }
        Model { // Y axis (green, Up)
            position: Qt.vector3d(0, 1, 0)
            scale: Qt.vector3d(0.0002, 0.02, 0.0002)
            source: "#Cube"
            materials: PrincipledMaterial { baseColor: "#3FB950"; lighting: PrincipledMaterial.NoLighting }
        }
        Model { // Z axis (blue, North→-Z in Qt3D)
            position: Qt.vector3d(0, 0, -1)
            scale: Qt.vector3d(0.0002, 0.0002, 0.02)
            source: "#Cube"
            materials: PrincipledMaterial { baseColor: "#58A6FF"; lighting: PrincipledMaterial.NoLighting }
        }

        // ── Instanced voxel cubes ──
        Model {
            source: "#Cube"
            instancing: VoxelInstanceTable {
                id: voxelTable
                store: telemetryStore
            }
            materials: PrincipledMaterial {
                baseColor: "white"
            }
        }

        // ── UAV marker (sphere) ──
        Model {
            visible: appState.connected
            position: Qt.vector3d(appState.positionX, appState.positionZ, -appState.positionY)
            scale: Qt.vector3d(0.003, 0.003, 0.003)
            source: "#Sphere"
            materials: PrincipledMaterial { baseColor: "#58A6FF"; lighting: PrincipledMaterial.NoLighting }
        }
    }

    // ── Mouse orbit/pan/zoom ──
    MouseArea {
        anchors.top: toolbar.bottom; anchors.left: parent.left
        anchors.right: parent.right; anchors.bottom: parent.bottom
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true

        property real lastX: 0
        property real lastY: 0
        property int activeButton: 0

        onPressed: function(mouse) {
            lastX = mouse.x; lastY = mouse.y
            activeButton = mouse.button
        }
        onReleased: { activeButton = 0 }

        onPositionChanged: function(mouse) {
            if (activeButton === 0) return
            var dx = mouse.x - lastX
            var dy = mouse.y - lastY
            lastX = mouse.x; lastY = mouse.y

            if (activeButton === Qt.LeftButton) {
                camYaw += dx * 0.3
                camPitch = Math.max(-89, Math.min(89, camPitch - dy * 0.3))
            } else if (activeButton === Qt.RightButton || activeButton === Qt.MiddleButton) {
                var speed = camDist * 0.003
                var yr = camYaw * Math.PI / 180
                camTargetX -= (dx * Math.cos(yr) + dy * Math.sin(yr)) * speed * 0.5
                camTargetZ += (dx * Math.sin(yr) - dy * Math.cos(yr)) * speed * 0.5
            }
            updateCamera()
        }

        onWheel: function(wheel) {
            camDist *= wheel.angleDelta.y > 0 ? 0.9 : 1.1
            camDist = Math.max(1, Math.min(100, camDist))
            updateCamera()
        }
    }
}
