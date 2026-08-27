import QtQuick 2.15
import QtQuick.Controls 2.15
import ZenithUI 1.0

Item {
    id: root
    property string pendingTaskId: ""
    property string pendingTaskPath: ""
    property int pendingDeleteRow: -1
    property string pendingDeleteName: ""

    function requestStart(taskId, taskName, taskPath, builtIn) {
        pendingTaskId = taskId
        pendingTaskPath = taskPath
        if (!builtIn) {
            customTaskConfirm.open()
        } else if (taskId === "c10_apriltag_landing") {
            landingConfirm.open()
        } else if (taskId === "ego_planner_odin") {
            egoConfirm.open()
        } else if (taskId === "super_planner_odin") {
            superConfirm.open()
        } else if (taskPath.length > 0) {
            // 内置但不在机载注册表里的任务，按绝对路径启动
            appState.startCustomManagedTask(taskId, taskPath)
        } else {
            appState.startManagedTask(taskId)
        }
    }

    Dialog {
        id: superConfirm
        anchors.centerIn: parent
        modal: true
        title: "确认启动 SUPER 自主避障导航"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appState.startCustomManagedTask(
                        root.pendingTaskId, root.pendingTaskPath)

        contentItem: Text {
            width: 470
            wrapMode: Text.WordWrap
            color: "#E6EDF3"
            text: "该入口会启动 SUPER / ROG-Map、消息兼容适配器和独立 Zenith bridge；不经过 EGO 的 1.5m 硬限高器。\n\n"
                  + "启动任务本身不会让飞机动；只有切入 COMMAND_CONTROL 后，规划指令才会进入控制器。\n\n"
                  + "启动后到 3D 栅格图查看 ROG-Map 体素，并点击目标点。SUPER 直接使用页面设置的目标高度。\n\n"
                  + "当前近场过滤半径为 0.8m，尚未完成真实飞行闭环验证。请先确认体素地图、ODIN 点云和飞行空域正常。\n\n"
                  + "停止时请使用「安全停止」；紧急情况下可拨回遥控器中位或执行降落。"
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#8B5A2B"
        }
    }

    Dialog {
        id: egoConfirm
        anchors.centerIn: parent
        modal: true
        title: "确认启动 EGO 自主避障导航"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appState.startCustomManagedTask(
                        root.pendingTaskId, root.pendingTaskPath)

        contentItem: Text {
            width: 470
            wrapMode: Text.WordWrap
            color: "#E6EDF3"
            text: "该入口会启动 EGO-Planner（ODIN 点云建图）、traj_server 和 zenith_ego_bridge。\n\n"
                  + "启动任务本身不会让飞机动。规划出的位置指令只有在飞机处于 COMMAND_CONTROL 时才生效，"
                  + "而 COMMAND_CONTROL 只能由遥控器三段开关从 RC_POS_CONTROL 切入。\n\n"
                  + "到 3D 栅格图点击目标点，飞机将自主规划路径并避障飞过去。\n\n"
                  + "中止方式：三段开关拨回中位交回手动 / 降落开关 / 地面站降落按钮（不受控制模式限制）。"
                  + "注意拨回中位只是让指令被丢弃，EGO 仍在追旧目标点，不再继续请一并「安全停止」。\n\n"
                  + "空中执行「安全停止」会先当前点悬停再交回遥控器，不会失控下坠。\n\n"
                  + "请确认：飞行空域已清场、ODIN 点云正常。"
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#8B5A2B"
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

    Dialog {
        id: customTaskConfirm
        anchors.centerIn: parent
        modal: true
        title: "确认启动自定义任务"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appState.startCustomManagedTask(
                        root.pendingTaskId, root.pendingTaskPath)

        contentItem: Text {
            width: 500
            wrapMode: Text.WrapAnywhere
            color: "#E6EDF3"
            text: "任务 ID：" + root.pendingTaskId
                  + "\n机载路径：" + root.pendingTaskPath
                  + "\n\n将执行 Jetson 上该路径对应的程序，并由任务管理器托管其进程。请确认文件来源和飞行行为。"
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#8B5A2B"
        }
    }

    Dialog {
        id: deleteConfirm
        anchors.centerIn: parent
        modal: true
        title: "删除自定义任务"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: scriptActionModel.removeCustomAction(root.pendingDeleteRow)

        contentItem: Text {
            width: 420
            wrapMode: Text.WordWrap
            color: "#E6EDF3"
            text: "确定从地面站任务库删除“" + root.pendingDeleteName + "”吗？\n只删除地面站记录，不删除 Jetson 上的任务文件。"
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#7D2525"
        }
    }

    Dialog {
        id: taskEditor
        anchors.centerIn: parent
        modal: true
        title: editRow < 0 ? "新增自定义任务" : "编辑自定义任务"
        standardButtons: Dialog.NoButton
        property int editRow: -1

        function openForCreate() {
            editRow = -1
            taskNameField.text = ""
            taskIdField.text = ""
            taskPathField.text = "/home/jetson/task_ws/src/user_tasks/"
            taskNoteField.text = ""
            editorError.text = ""
            open()
            taskNameField.forceActiveFocus()
        }

        function openForEdit(row, name, taskId, taskPath, note) {
            editRow = row
            taskNameField.text = name
            taskIdField.text = taskId
            taskPathField.text = taskPath
            taskNoteField.text = note
            editorError.text = ""
            open()
            taskNameField.forceActiveFocus()
        }

        function saveTask() {
            var ok
            if (editRow < 0) {
                ok = scriptActionModel.addCustomAction(
                            taskNameField.text, taskIdField.text,
                            taskPathField.text, taskNoteField.text)
            } else {
                ok = scriptActionModel.updateCustomAction(
                            editRow, taskNameField.text, taskIdField.text,
                            taskPathField.text, taskNoteField.text)
            }
            if (ok) {
                close()
            } else {
                editorError.text = scriptActionModel.lastError
            }
        }

        contentItem: Column {
            width: 560
            height: 356
            spacing: 7

            Text { text: "任务名称"; color: "#8B949E"; font.pixelSize: 11 }
            TextField {
                id: taskNameField
                width: parent.width
                height: 34
                maximumLength: 64
                placeholderText: "例如：仓库巡检"
                color: "#E6EDF3"
                placeholderTextColor: "#6E7681"
                selectByMouse: true
                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
            }

            Text { text: "任务 ID（唯一）"; color: "#8B949E"; font.pixelSize: 11 }
            TextField {
                id: taskIdField
                width: parent.width
                height: 34
                maximumLength: 64
                placeholderText: "例如：warehouse_inspection"
                color: "#E6EDF3"
                placeholderTextColor: "#6E7681"
                selectByMouse: true
                background: Rectangle {
                    radius: 6
                    color: "#0D1117"
                    border.color: taskIdField.text.length === 0
                                  || (/^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$/).test(taskIdField.text.trim())
                                  ? "#30363D" : "#F85149"
                }
            }

            Text { text: "Jetson 任务绝对路径"; color: "#8B949E"; font.pixelSize: 11 }
            TextField {
                id: taskPathField
                width: parent.width
                height: 34
                maximumLength: 512
                placeholderText: "/home/jetson/task_ws/src/user_tasks/.../my_task.launch"
                color: "#E6EDF3"
                placeholderTextColor: "#6E7681"
                selectByMouse: true
                background: Rectangle {
                    radius: 6
                    color: "#0D1117"
                    border.color: taskPathField.text.length === 0
                                  || taskPathField.text.trim().startsWith(
                                      "/home/jetson/task_ws/src/user_tasks/")
                                  ? "#30363D" : "#F85149"
                }
            }

            Text { text: "任务说明（可选）"; color: "#8B949E"; font.pixelSize: 11 }
            TextField {
                id: taskNoteField
                width: parent.width
                height: 34
                maximumLength: 200
                placeholderText: "说明任务用途、前置条件或注意事项"
                color: "#E6EDF3"
                placeholderTextColor: "#6E7681"
                selectByMouse: true
                background: Rectangle { radius: 6; color: "#0D1117"; border.color: "#30363D" }
            }

            Text {
                id: editorError
                width: parent.width
                height: 18
                color: "#F85149"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Row {
                anchors.right: parent.right
                spacing: 8
                PrimaryButton {
                    width: 82; height: 32
                    text: "取消"
                    fillColor: "#21262D"
                    onClicked: taskEditor.close()
                }
                PrimaryButton {
                    width: 100; height: 32
                    text: taskEditor.editRow < 0 ? "添加任务" : "保存修改"
                    fillColor: "#1A4A2E"
                    enabled: taskNameField.text.trim().length > 0
                             && taskIdField.text.trim().length > 0
                             && taskPathField.text.trim().startsWith(
                                 "/home/jetson/task_ws/src/user_tasks/")
                    onClicked: taskEditor.saveTask()
                }
            }
        }
        background: Rectangle {
            radius: 10
            color: "#161B22"
            border.color: "#30363D"
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
                text: "机载任务库"
                color: "#E6EDF3"
                font.pixelSize: 16
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: scriptActionModel.builtInCount + " 个内置 · "
                      + scriptActionModel.customCount + " 个自定义"
                color: "#6E7681"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: 20; height: 1 }
            PrimaryButton {
                width: 92
                height: 28
                text: "新增任务"
                fillColor: "#1A4A2E"
                enabled: !appState.managedTaskActive
                anchors.verticalCenter: parent.verticalCenter
                onClicked: taskEditor.openForCreate()
            }
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
                    spacing: 4
                    Text { text: "任务 ID / 路径"; color: "#8B949E"; font.pixelSize: 11 }
                    Text {
                        width: parent.width
                        text: appState.managedTaskName.length > 0 ? appState.managedTaskName : "--"
                        color: "#E6EDF3"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: appState.managedTaskPath
                        visible: text.length > 0
                        color: "#8B949E"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
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
                text: "内置任务受保护；自定义任务持久保存。停止任务时先停止输出，再按当前飞行状态执行悬停交接。"
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
                required property string taskPath
                required property string note
                required property string category
                required property bool builtIn
                readonly property bool isActive:
                    appState.managedTaskActive && appState.managedTaskName === command
                readonly property bool isLast:
                    !appState.managedTaskActive && appState.managedTaskName === command

                width: ListView.view.width
                height: 96
                radius: 10
                color: isActive ? "#15251C" : "#161B22"
                border.color: isActive ? "#2EA043" : "#30363D"

                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 14

                    Rectangle {
                        width: 64
                        height: 24
                        radius: 12
                        color: !builtIn ? "#2A2038"
                              : category === "降落" ? "#3A211C"
                              : category === "跟踪" ? "#17243A" : "#1A2D24"
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: builtIn ? category : "自定义"
                            color: !builtIn ? "#BC8CFF"
                                  : category === "降落" ? "#F0883E"
                                  : category === "跟踪" ? "#58A6FF" : "#3FB950"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Column {
                        width: parent.width - actionRow.width - 92
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Row {
                            spacing: 10
                            Text { text: name; color: "#E6EDF3"; font.pixelSize: 14; font.bold: true }
                            Text {
                                text: builtIn ? "内置保护" : command
                                color: builtIn ? "#6E7681" : "#BC8CFF"
                                font.pixelSize: 10
                            }
                            Text {
                                text: isActive ? appState.managedTaskState : (isLast ? appState.managedTaskState : "")
                                color: isActive ? "#3FB950" : "#8B949E"
                                font.pixelSize: 11
                            }
                        }
                        Text {
                            width: parent.width
                            text: note.length > 0 ? note : "未填写任务说明"
                            color: "#8B949E"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            visible: taskPath.length > 0
                            text: taskPath
                            color: "#6E7681"
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                    }

                    Row {
                        id: actionRow
                        width: builtIn ? 196 : 328
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        PrimaryButton {
                            width: builtIn ? 92 : 70
                            height: 32
                            text: category === "降落" ? "启动精降" : "启动"
                            fillColor: category === "降落" ? "#6E3B1F" : "#1A4A2E"
                            enabled: appState.protocolConnected && !appState.managedTaskActive
                            onClicked: root.requestStart(command, name, taskPath, builtIn)
                        }
                        PrimaryButton {
                            width: builtIn ? 92 : 70
                            height: 32
                            text: "安全停止"
                            fillColor: isActive ? "#7D2525" : "#21262D"
                            textColor: isActive ? "#FFFFFF" : "#6E7681"
                            enabled: appState.protocolConnected && isActive
                            onClicked: appState.stopManagedTask(command)
                        }
                        PrimaryButton {
                            visible: !builtIn
                            width: 70
                            height: 32
                            text: "编辑"
                            fillColor: "#21262D"
                            enabled: !appState.managedTaskActive
                            onClicked: taskEditor.openForEdit(
                                           index, name, command, taskPath, note)
                        }
                        PrimaryButton {
                            visible: !builtIn
                            width: 70
                            height: 32
                            text: "删除"
                            fillColor: "#4A2020"
                            enabled: !appState.managedTaskActive
                            onClicked: {
                                root.pendingDeleteRow = index
                                root.pendingDeleteName = name
                                deleteConfirm.open()
                            }
                        }
                    }
                }
            }
        }
    }
}
