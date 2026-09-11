# MAVLink2 迁移说明

当前地面站运行链路使用标准 MAVLink2 帧。TCP 为同一连接上的双向数据流；UDP 接收作为 TCP 未连接时的后备，LR24 配对配置完成后在串口上传输同一套 MAVLink2 帧。LR24 的本地配对协议仍由 `Lr24RadioProtocol` 独立处理。

正常串口流由 `PacedSerialWriter` 异步发送：同一个 FIFO 保存完整业务消息及心跳，每次最多写 200 字节，等待实际波特率的 8N1 线速时间加 8 ms；上一块尚在 Qt 输出缓冲区时不再追加。短写继续发送剩余部分，驱动堵塞超过 5 秒触发断链，关闭、重新连接或重新配对会取消残留。队列同时限制内存、消息数和预计发送时间（1 秒），超限整条拒绝并记录日志。LR24 的短配置命令独占配对阶段，TCP 不使用该节奏。

这是实机链路兼容措施：测试曾发现下行数据出现相差 255 字节的内容覆盖，以及一次 399 字节上行任务 STOP 请求未到达；不能只凭该现象定位到某个硬件或驱动。`paced_serial_writer` 回归覆盖分块时间、短写、背压、尾块堵塞、重入重连和队列边界；实际无线链路另行验证。

## 标准遥测与业务扩展

- 地面站源地址为 system 255 / component 190；逻辑飞机 ID 为 1..254，机载桥 component 为 191。
- FCU 原生 `HEARTBEAT`、`LOCAL_POSITION_NED`、`ATTITUDE`、`SYS_STATUS`、`BATTERY_STATUS`、GPS、高度、下视距离和 `STATUSTEXT` 等标准消息进入 `ZenithMavlinkCodec`。位置和速度从 NED 转 ENU，姿态从 FRD/NED 转 FLU/ENU。
- 机载 `UAVSTATE` 业务扩展中的 `fcu_system_id` 关联实际 FCU system ID 与逻辑飞机 ID。该关联按接收字节流隔离，断线或切换目标时清除。机载附加字段不能覆盖 FCU 的解锁、模式、位置和电池等字段。
- 仅伴随计算机心跳不能使飞控显示在线。控制链路要求新鲜 FCU 遥测、心跳以及已完成的握手。
- Zenith 业务使用 common.xml 的 `V2_EXTENSION`（message ID 248），扩展内部携带业务 kind、事务 ID 和 JSON 分片。CRC 校验作用于每个 MAVLink2 帧；扩展重组检查长度、偏移和分片边界，随后解析检查 JSON 格式。使用共享 `include/zenith_protocol/mavlink_wire.hpp` 与 `third_party/mavlink/`，没有旧 am/MsgPack 运行分支。

| 业务 kind | JSON 形状及地面站适配 |
| --- | --- |
| 202 MODESELECTION | `mode/selectId/use_mode/cmd`，新连接先发 GCS HEARTBEAT 再进行握手 |
| 108 UAVCOMMAND | 明确的 `position_ref/velocity_ref/acceleration_ref/att_ref` 数组；`Command_ID` 保留 uint32，航点保留 `waypoint_mission` |
| 109 UAVSETUP | `cmd/arming/px4_mode/control_state` |
| 110 PARAMSETTINGS | `param_module/params:[{type,param_name,param_value}]`；同模块按参数名称合并多批响应，保留尚未上传的本地编辑 |
| 113 CUSTOMDATASEGMENT_1 | 线上统一为 `datas:[{name,type,value}]`；编码器把旧 UI 索引模型转成数组，解码器转回模型供任务与预检状态使用，`value` 为字符串 |
| 114 PROTOCOL_ACK | `request_kind/transaction_id/status`；`RECEIVED` 表示桥端接收处理请求，不能当作飞控已执行、任务已成功或飞行验证 |
| 255 GOAL | `{position:[x,y,z],yaw,frame_id:"world"}`；SUPER 由 AppState/CommandDispatcher 的同一接口发送，机载直接发布目标，不经过 shell；EGO 保留独立的 goal + trigger 流程 |
| 11 / 12 / 13 | `gm_data/pp_data/vm_data` 在 JSON 中为 base64，解码后为字节数组；规划路径为小端 float32 XYZ，体素仍按既有掩码/RLE 分片重组 |

轨迹消息 `{pp_num_points:0,pp_data:""}` 清除已结束的路径；非法长度、超限点数和非有限坐标不会替换当前有效路径。二维地图与体素显示保持原有优先关系。

## 构建和本地回归

```powershell
$env:PATH = 'C:\Qt\6.6.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/mingw_64
cmake --build build-release --parallel 3
ctest --test-dir build-release --output-on-failure
```

`mavlink_groundstation` 使用生产 codec、连接客户端、任务分发器和 TelemetryStore 验证本机 TCP 回环、帧校验、源隔离、坐标转换、业务数组、参数多批合并、地图和航点反馈。它不会连接外部飞机。

旧 MsgPack 解码器仅保存在 `tests/legacy/`，用于历史体素基线；相应 CTest 名为 `voxelmap_legacy_fixture`，带 `legacy` 标签。现存 am/JSON Python 工具的文件头已标为 `HISTORICAL LEGACY PROTOCOL`，不能用于验证当前 MAVLink2 桥。

## 手工生产连接探针

`ZenithMavlinkProbe` 是单独的无界面 CMake 目标，不注册为外网 CTest。支持隔离测试桥和已配对串口的通信验证；涉及任务启动或规划目标时，使用已核对控制输出隔离的试验环境。

下面命令中的 `TEST_BRIDGE_IP` 需替换为测试主机的 IP。测试桥使用 TCP 56555；本例模拟源为 FCU 1、逻辑飞机 214、未解锁 POSCTL、ENU 位置 `[1.25,-2.5,3.75]`、电池 15.2 V。

```powershell
& .\build-release\ZenithMavlinkProbe.exe TEST_BRIDGE_IP 56555 214 `
  --fcu-id 1 --expect-position '1.25,-2.5,3.75' `
  --expect-battery 15.2 --expect-mode POSCTL `
  --require-custom-data --require-voxel-map --require-planned-path `
  --timeout-ms 20000 2>&1 | Out-String
```

探针默认仅发送生产握手、GCS 心跳及一次只读参数查询，默认查询模块 2；指定 `--param-name /absolute/ros/parameter` 则使用模块 5 SEARCH，`--skip-params` 可跳过参数查询。它没有解锁、手动运动或参数写入选项。

串口模式明确指定逻辑 MAVLink ID，独立于 LR24 物理地址：

```powershell
& .\build-release\ZenithMavlinkProbe.exe --serial COM5 --baud 921600 --uav-id 11 `
  --fcu-id 1 --timeout-ms 20000
```

串口探针显式启用 `ZenithProtocolClient::setRadioPairingReadOnly(true)`：读取当前已配对地址后直接验证完成，禁止发送 SET，不将物理地址保存到应用配对配置。产品默认配对流程保持原样。输出包括实际无线地址、配对状态、串口收发字节和独立的逻辑飞机 ID。

以下选项必须显式指定；每次运行最多一种操作，且探针仅在 FCU 遥测新鲜、链路就绪、未解锁时发送：

- `--task-query`：使用生产 CommandDispatcher 发送任务 STATUS。
- `--task-start NAME [--task-path /absolute/path]`：发送 START，yaw enable 固定为 false。
- `--task-stop NAME --wait-task-state STOPPED`：发送 STOP，并等待机载任务管理器的 STOPPED 状态。
- `--super-goal x,y,z`：使用与界面相同的生产接口发送 kind 255，world 坐标、yaw 为 0。范围为 x/y ±1000 m，z ±100 m。

`--wait-task-state RUNNING` 等选项要求匹配任务生命周期；`--observe-ms 3000` 在操作发送后继续接收至少 3 秒。JSON 分别记录 `protocol_acks`、`task_events`、`task_request_id`、任务 ACK/状态/原因。规划目标的 `RECEIVED` 仅表示目标发布，任务协议 ACK 与机载任务生命周期也分别判读。每个新客户端使用随机非零事务序列起点，避免快速重启后与桥端的近期去重缓存碰撞。

成功条件包括握手 ACK、参数查询 ACK 和响应、实际 FCU 关联、新鲜遥测、稳定遥测、未解锁状态，以及位置/速度/姿态/电池首值。选项还可要求任务/预检数据、完整体素图或规划路径。探针打印一行 JSON，包含各条件、实际遥测和 ACK；通过退出 0，超时退出 1，参数错误退出 2。它使用临时 UDP 接收端口，不占生产 8889。

TCP 回环结果不能当作实际 LR24 链路证据；串口模式需要另行连接并核对真实设备。两者均不能代替真实控制闭环或飞行验收。

## 成对回滚

地面站和机载桥必须使用配套的协议实现。回滚旧版时，两端一起回滚到兼容版本，并重新核对地址、配置和握手；仅替换其中一端可造成链路打开但业务帧无法解析。保留历史 fixture 不代表当前运行程序支持自动降级。
