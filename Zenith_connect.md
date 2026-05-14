# 地面站与 Jetson 上位机通信优化北极星规划

## 1. 北极星目标

在 Windows 地面站与 Jetson 上位机已完成 Wi-Fi 配网、处于同一局域网的前提下，建立一套可观测、可恢复、可安全降级的通信链路：地面站启动后 10 秒内稳定发现并连接 Jetson，飞行过程中任一侧短暂断网、进程重启或 TCP 断开后，双方都能进入明确状态、禁止误发控制命令，并在链路恢复后自动完成重新握手、状态同步和控制权确认。

本阶段不追求复杂多机调度、不重写完整 Zenith 协议、不把 SSH 当作长期控制通道。SSH 只用于启动、维护和诊断；实时遥测、控制命令、心跳、参数同步必须走明确的应用层协议。

## 2. 成功验收标准

- 连接建立：地面站输入 Jetson IP 后，能在 10 秒内完成 TCP 控制连接、UDP 遥测接收、双向心跳确认、ModeSelection 建链，并显示统一的 `CONNECTED / DEGRADED / DISCONNECTED / FAILSAFE` 状态。
- 正常运行：遥测刷新不低于 5 Hz，心跳 1 Hz，控制命令从点击到 Jetson 收到的链路延迟小于 200 ms，所有关键命令都有 `Command_ID` 与 ACK/失败原因。
- 断开处理：拔掉 Wi-Fi、关闭地面站、重启 Jetson bridge、杀掉 ROS 节点时，双方在 3 到 6 秒内进入可解释状态，地面站 UI 禁止继续发送非安全命令。
- 自动恢复：网络恢复或进程重启后，地面站能自动重连 TCP，重新发送 ModeSelection，重新请求参数/脚本配置，恢复遥测显示，不需要手工反复改 IP 或重启应用。
- 安全边界：心跳超时只触发预先定义的安全策略，例如悬停、返航或降落；不能因为一个临时 TCP 发送失败就直接进入不可逆危险动作。
- 可诊断性：地面站和 Jetson 都记录连接事件、peer IP、端口、心跳计数、最后一条命令、最后一条遥测时间、断线原因，能在现场 1 分钟内判断是网络、进程、协议还是飞控状态问题。

## 3. 当前链路事实（2026-05-14 更新）

### 地面站侧

- `ZenithProtocolClient` 负责 UDP 监听、TCP 控制连接、本地 heartbeat server、自动重连、协议编解码和日志。
- 默认端口是 UDP `8889`、TCP 控制 `55555`、TCP heartbeat `55556`。
- **已实现持久 TCP**：`start()` 后绑定 UDP，监听 heartbeat，启动 1 Hz heartbeat timer，然后 `connectToHost()` 连接 Jetson TCP。
- TCP 断开后，如果不是手动断开，会通过 3 秒定时器自动重连。
- 新 TCP 连接建立后会发送一次 `ModeSelection(UM_CREATE, UAVBASIC)`，进入 `HANDSHAKING` 状态等待 Jetson ACK。
- **已实现完整状态机**：STOPPED / DISCONNECTED / CONNECTING / HANDSHAKING / RECONNECTING / DEGRADED / CONNECTED。
- **已实现 freshness 监控**：UDP 3s / Heartbeat 3.5s 超时检测，超时进入 DEGRADED。
- **已实现控制命令阻断**：`canSendControlCommands()` 要求 TCP connected + UDP fresh + HB fresh。
- `AppState::bootstrapDemoTelemetry()` **默认已关闭**，仅当 `QSettings(“demoMode”)=true` 时启用。
- **已实现命令 ACK 解析**：TextInfo 中 `CMD_ACK:` 前缀自动更新 UI 命令确认状态。

### Jetson 上位机侧

- `communication_bridge` 从 ROS 参数读取 `udp_port=8889`、`tcp_port=55555`、`tcp_heartbeat_port=55556`、`ground_station_ip`、`try_connect_num`（launch 中默认 5，C++ fallback 3）。
- **已重构为持久 TCP session**：`serverFun()` accept 后为每个连接启动 `handleClientSession()` 线程，循环 `recv()` + 流式 buffer + `extractNextTcpFrame()` 帧解析。
- **已实现 session 管理**：`ActiveSession` 结构（socket_fd / client_ip / session_id / connected / last_tcp_rx），`std::mutex` 保护，`atomic<uint64_t>` session 计数器。
- **已实现单地面站控制**：`allowSessionTakeoverLocked()` 仅允许同 IP 或 heartbeat 未就绪时接管。
- 收到 `ModeSelection(UAVBASIC, UM_CREATE)` 后创建 `UAVBasic`，**并通过 TextInfo 返回 `MODESELECTION_ACK:OK` 或 `MODESELECTION_ACK:FAIL`**。
- `UAVBasic` 通过 UDP 向地面站发送 `UAVSTATE`、`UAVCONTROLSTATE`、`TEXTINFO`、参数和自定义数据。
- Jetson 有 1 Hz 心跳逻辑，会向地面站 TCP heartbeat 端口发送系统信息和 ROS 节点列表。
- **心跳超时已改为分级降级**：`degraded_timeout`(3s) → 仅告警 / `hover_timeout`(6s) → 悬停 / `failsafe_timeout`(10s) → 降落。参数可通过 `bridge.launch` 配置。
- **已实现命令 ACK + 去重**：`recvData(UAVCommand)` 解析 `Command_ID`，环形 buffer（32条）去重，每条命令通过 TextInfo 回传 `CMD_ACK:ID=N:RECEIVED` 或 `CMD_ACK:ID=N:DUPLICATE`。

### 已解决的矛盾

- ~~TCP 生命周期不一致~~：双方均为持久 TCP，Jetson 支持同一连接多帧连续处理。✅
- ~~心跳超时直接 Land~~：已改为 3 级降级（DEGRADED → Hover → Land）。✅
- ~~ModeSelection 无 ACK~~：Jetson 创建 UAVBasic 后回传明确 ACK，地面站在 HANDSHAKING 状态等待。✅
- ~~命令无确认~~：UAVCommand 已有 ACK + 去重（UAVSetup 结构体无 Command_ID 字段，暂不支持）。✅

## 4. 目标通信模型

### 推荐链路分工

- TCP 控制通道：地面站主动连接 Jetson `55555`，保持长连接，发送 ModeSelection、UAVCommand、UAVSetup、ParamSettings、脚本命令。
- UDP 遥测通道：Jetson 主动发到地面站 `8889`，承载 UAVState、UAVControlState、TextInfo、参数回传和低频配置。
- 心跳通道：短期保留现有双向心跳，但要统一语义。地面站通过 TCP 控制通道发送 heartbeat，Jetson 通过地面站 `55556` 或 UDP 发送 heartbeat。长期建议改为同一 TCP session 内的双向 heartbeat，减少端口和状态复杂度。
- SSH 运维通道：只用于启动 tmux/systemd 服务、看日志、拉起 ROS launch，不参与飞控实时控制。

### 应用层会话状态

双方都应维护同一套状态：

- `IDLE`：未主动连接。
- `CONNECTING`：TCP 正在连接或等待 heartbeat。
- `HANDSHAKING`：TCP 已连上，正在发送 ModeSelection、确认 robot_id、同步 ground_station_ip。
- `CONNECTED`：TCP、UDP、heartbeat、ROS topic 都正常。
- `DEGRADED`：部分链路异常，例如 TCP 正常但 UDP 遥测超时，或 UDP 正常但 heartbeat 超时。
- `RECONNECTING`：连接丢失，自动重试中，禁止普通控制命令。
- `FAILSAFE`：超过安全阈值，进入预设动作，例如悬停/降落/返航。
- `STOPPED`：用户主动断开，双方释放控制权。

### 最小握手流程

1. 地面站连接 Jetson TCP `55555`。
2. 地面站发送 `HELLO` 或现有 `ModeSelection(UM_CREATE)`，携带 `protocol_version`、`robot_id`、`ground_station_id`、`udp_port`、`heartbeat_port`、`session_id`。
3. Jetson 返回 `WELCOME/ACK`，确认 `robot_id`、当前 `uav_` 状态、是否允许接管控制权、当前安全状态。
4. Jetson 更新 `ground_station_ip`，开始向地面站 UDP 发送遥测。
5. 地面站收到连续 2 到 3 帧有效遥测和 heartbeat 后，UI 才进入 `CONNECTED`。
6. 地面站主动请求参数、脚本配置、当前定位源和控制状态，避免恢复连接后 UI 使用旧缓存。

## 5. 问题清单与优化方向

### P0：统一 TCP 生命周期

当前 Jetson `serverFun()` 在 accept 后读取到数据就 close socket，而地面站 `ZenithProtocolClient` 已经实现持久 TCP。这必须优先统一。

改造目标：

- Jetson accept 后为每个连接启动 client handler，循环 `recv()`，维护每个连接的流式 buffer。
- 使用和地面站一致的帧解析：magic `0x61 0x6d`、payload size、msg_id、robot_id、JSON payload、CRC16-ARC。
- 同一连接内可连续处理 ModeSelection、Heartbeat、UAVCommand、ParamSettings。
- client 断开时只标记当前 session 断开，不直接让整个 bridge 退出。
- 同一时刻只允许一个 active ground station session，其他连接只能观察或被拒绝，并返回明确 TextInfo/ACK。

### P0：建立真正的连接状态机

目前地面站侧是 `udpState/tcpState/heartbeatState` 三个字符串，Jetson 侧是 `uav_ / is_heartbeat_ready_ / disconnect_flag / disconnect_num` 分散变量。需要把状态显式化。

改造目标：

- 地面站 `ZenithProtocolClient` 增加枚举状态和最后更新时间：`lastTcpRx`、`lastUdpRx`、`lastHeartbeatRx`、`lastCommandAck`。
- Jetson `CommunicationBridge` 增加 session 对象：`active_client_ip`、`session_id`、`last_tcp_rx`、`last_heartbeat_rx`、`last_udp_tx`、`control_owner`。
- UI 不再只显示“TCP Connected”，而是显示整体状态和具体缺失项，例如“TCP OK / UDP stale 4.2s / HB OK”。

### P0：断线后的安全策略要分级

当前 Jetson 心跳超时后直接 `triggerUAV()`，里面发 Land。这个策略过硬，容易把普通地面站重启、Wi-Fi 抖动和真正失控混在一起。

建议分级：

- 3 秒无地面站 heartbeat：进入 `DEGRADED`，禁止新任务，保留当前飞控控制源。
- 6 秒无 heartbeat 但飞控/RC 正常：发送 Hover 或保持当前模式，由参数决定是否自动降落。
- 10 秒以上且无 RC/任务接管：执行预设 failsafe，例如 Land 或 Return。
- 如果已经处于降落/上锁/地面状态，不重复发送 Land。
- 所有动作都必须通过 TextInfo/状态帧回报给地面站。

### P1：命令必须可确认、可去重

地面站现在发送 `Command_ID`，但没有完整 ACK 机制。断线恢复时可能重复发送或用户以为命令生效。

改造目标：

- 每条控制命令带 `Command_ID`、`session_id`、`timestamp_ms`、`source=GROUND_STATION`。
- Jetson 收到后立即 ACK：`RECEIVED / REJECTED / EXECUTING / DONE / FAILED`。
- Jetson 记录最近 N 条 `Command_ID`，重复命令只返回上次结果，不重复执行。
- 地面站 UI 在 ACK 前显示“待确认”，超时显示“未确认，不代表未执行”，避免误导用户重复点击。

### P1：恢复连接后必须重新同步

重连不是简单 TCP connected。恢复后必须重新确认控制权、参数和遥测 freshness。

恢复步骤：

- 清空地面站旧的 TCP receive buffer 和 pending command UI。
- 重新发送 ModeSelection/HELLO。
- 请求 `ParamSettings`、脚本配置、定位源、控制状态。
- 等待新的 UAVState 和 UAVControlState，不使用 demo telemetry 判断在线。
- 如果 Jetson 表示当前已有其他控制源，地面站只能进入观察模式，不能直接抢控制权。

### P1：清理 demo 数据对真实状态的干扰

`AppState::bootstrapDemoTelemetry()` 在真实链路调试阶段会掩盖“没有收到遥测”的问题。

改造目标：

- 增加 `demo_mode` 配置，默认关闭。
- 真实连接模式下，未收到真实 UAVState 时显示 `No Telemetry`，而不是显示假电量、假位置、假 OFFBOARD。
- mock 测试和 UI 展示需要 demo 时，通过显式开关进入。

### P2：发现与配置体验

目前依赖手动填 Jetson IP。短期可以接受，但要减少现场错误。

优化方向：

- 地面站提供“检测本机 IP / ping Jetson / TCP probe / UDP listen test / heartbeat test”的一键诊断。
- 保存 profile 时记录 SSID、Jetson IP、robot_id、端口、最后成功时间。
- Jetson 启动时打印当前 IP、监听端口、ground_station_ip、ROS master 状态。
- 后续可以加 UDP broadcast discovery，但不是第一阶段必要项。

## 6. 分阶段路线

### 阶段 1：把链路跑稳

目标：不改大协议，先让双方对 TCP 和心跳的理解一致。

- Jetson 改为持久 TCP session handler，支持一条连接上多帧连续处理。
- 地面站保留当前自动重连，但增加重连中的命令禁止和状态清空。
- Jetson 心跳超时改成分级策略，不要一上来就 Land。
- 地面站关闭默认 demo telemetry，真实链路无遥测就明确显示离线。
- 补齐 mock 测试：Jetson close TCP、地面站重连、UDP 暂停、heartbeat 暂停、多命令同连接。

阶段 1 验收：

- 地面站连续点击 10 次普通命令，Jetson 只建立 1 条 TCP session，能收到 10 条命令。
- 手动杀掉 Jetson bridge 后重启，地面站自动恢复到 CONNECTED。
- 暂停 UDP 遥测 5 秒，UI 进入 DEGRADED，不误显示在线。
- 手动断开地面站，Jetson 释放控制权并停止向旧 session 发 heartbeat。

### 阶段 2：把控制权和恢复流程做清楚

目标：连接恢复后不会旧状态污染，不会多地面站抢控制。

- 引入 `session_id` 和 `control_owner`。
- ModeSelection 返回明确 ACK。
- 命令 ACK/去重机制落地。
- Jetson 对新 client 的接管规则参数化：禁止接管、heartbeat 超时后允许接管、手动确认后接管。
- UI 增加“观察模式 / 控制模式 / 接管请求”。

阶段 2 验收：

- 两台地面站同时连接时，只有一台能发控制命令，另一台看到被拒原因。
- 地面站断线再恢复后，旧 pending command 不会被当作新命令重复执行。
- Jetson 重启后，地面站能重新拉取参数和脚本列表。

### 阶段 3：把现场使用做成 SOP

目标：普通使用不依赖开发者记忆。

- 写入开机启动方式：systemd 或 tmux 固定 session。
- 地面站提供连接诊断页和最近日志导出。
- Jetson 提供 `bridge_status` 命令或 ROS service，返回端口、session、心跳、最近命令、topic freshness。
- 建立飞行前检查表和异常恢复表。

阶段 3 验收：

- 换一台电脑，只要在同一 Wi-Fi、知道 Jetson IP，就能按 SOP 连接成功。
- 现场断网后，操作者能根据 UI 状态判断“等自动恢复 / 手动重连 / 切 RC / 降落”。

## 7. 正常使用 SOP

### 启动前

- 确认 Windows 地面站和 Jetson 在同一 Wi-Fi 或同一局域网。
- Windows 上 `ping <Jetson IP>`，确认基础网络可达。
- SSH 到 Jetson 只用于确认服务：`roscore`、`communication_bridge`、`uav_control_main` 是否运行。
- 确认 Jetson `bridge.launch` 中 `robot_id/uav_id/is_simulation/ground_station_ip` 与实际一致。
- 确认防火墙允许 `8889/UDP`、`55555/TCP`、`55556/TCP`。

### 建立连接

- 地面站选择或新建连接 profile：Jetson IP、UDP `8889`、TCP `55555`、Heartbeat `55556`。
- 点击“连接测试”，只能说明 TCP 端口可达，不能代表完整链路在线。
- 点击“开始连接”，等待状态从 `CONNECTING` 到 `HANDSHAKING` 再到 `CONNECTED`。
- 只有同时满足 TCP connected、UDP telemetry fresh、heartbeat fresh、ModeSelection ACK 后，才允许执行控制命令。

### 正常飞行中

- 关注整体状态，不只看 TCP：UDP stale 或 heartbeat stale 都应视为降级。
- 控制命令发出后必须看 ACK/反馈，不要连续重复点击。
- 如果 UI 显示 `DEGRADED`，暂停新任务，确认遥测和 RC 状态。
- 如果进入 `RECONNECTING`，等待自动恢复；不要通过 SSH 反复重启多个 bridge 实例。

### 主动断开

- 地面站点击“断开连接”时，应发送 `ModeSelection(UM_DELETE)` 或等效 release-control 消息。
- Jetson 收到后释放 `uav_` 或至少释放地面站 control owner。
- 地面站关闭 TCP、UDP listener、heartbeat server，并清空 protocol connected 状态。

### 异常恢复

- TCP 断开但 UDP 还在：地面站进入 `DEGRADED/RECONNECTING`，自动重连 TCP，禁止普通控制命令。
- UDP 没有遥测但 TCP 还在：提示 Jetson ROS topic 或 bridge 内部异常，优先检查 `/zenith/state`。
- heartbeat 丢失但遥测正常：提示 heartbeat 通道异常，不应立即认定飞机失控。
- Jetson bridge 重启：地面站自动重连后必须重新 ModeSelection 和参数同步。
- 地面站崩溃重开：Jetson 应允许同一 IP/同一 session 恢复，或在旧心跳超时后允许接管。

## 8. 建议代码落点

### GroundStationQt

- `src/ZenithProtocolClient.*`：增加枚举状态机、freshness timer、命令 ACK、重连后的 handshake pipeline。
- `src/AppState.*`：去掉默认 demo telemetry，增加 demo_mode；把 link state 暴露为结构化字段。
- `src/CommandDispatcher.*`：所有命令加 session、timestamp、ACK 跟踪。
- `src/TelemetryStore.*`：增加 telemetry timeout，不再因旧数据长期显示在线。
- `qml/Main.qml`：连接弹窗显示连接阶段、缺失项、最近错误、可执行恢复操作。
- `tests/`：扩展 mock，覆盖持久 TCP、多帧、断线、重连、UDP stale、heartbeat stale。

### Zenith_ws_skyborne

- `src/zenith_communication/src/communication_bridge.cpp`：重写 `serverFun()` 为持久 TCP session handler，拆分 `handleClient()`、`handleFrame()`、`updateSessionState()`。
- `src/zenith_communication/include/communication_bridge.hpp`：增加 session 状态、时间戳、control owner、mutex 保护。
- `src/zenith_communication/shard/include/communication.hpp`：如果预编译库限制太多，优先在 bridge 层增加流式 buffer 和会话管理，不强行改 `.so`。
- `src/zenith_communication/launch/bridge.launch`：暴露 heartbeat 超时、failsafe 策略、是否允许接管等参数。
- `src/zenith_control/src/uav_controller.cpp`：确认外部控制源切换、失联安全动作、Land/Hover/Return 的实际执行边界。

## 9. 不建议的方向

- 不建议把 SSH 作为实时控制链路。SSH 可以启动程序，但不能承担飞控命令和心跳。
- 不建议继续让 TCP 每条命令新建连接。这样很难做命令 ACK、重连恢复和控制权管理。
- 不建议只在 UI 上显示“已连接”。连接必须拆成 TCP、UDP、heartbeat、ROS topic、control owner 多个维度。
- 不建议在未区分场景时直接 Land。链路抖动、地面站重启、Jetson 进程重启都不等于飞机必须立即降落。
- 不建议让 demo telemetry 默认开启。真实调试阶段，假数据比空数据更危险。

## 10. 第一批实施任务（2026-05-14 状态更新）

1. ✅ Jetson：把 `serverFun()` 改成持久 TCP session，支持一条连接解析多帧。
2. ✅ 地面站：增加 telemetry/heartbeat freshness timeout，UI 显示 `DEGRADED`。
3. ✅ 地面站：关闭默认 demo telemetry，增加显式 demo mode。
4. ✅ 双方：ModeSelection ACK — Jetson 通过 TextInfo 回传 `MODESELECTION_ACK:OK/FAIL`，地面站增加 HANDSHAKING 状态等待 ACK（5 秒超时，重试 1 次）。
5. ✅ 双方：命令加 ACK 和去重 — UAVCommand 支持 `CMD_ACK:ID=N:RECEIVED/DUPLICATE`（注：UAVSetup 结构体无 Command_ID 字段，暂不支持去重）。
6. ❌ 测试：用 mock 覆盖”Jetson 主动断开””地面站重启””UDP 停止””heartbeat 停止””多命令同连接”。（待第二批实施）

### 已部署验证（2026-05-14）

- 代码已编译部署至 192.168.83.27（自用机），`catkin_make` 通过
- Bridge 节点启动正常，分级降级参数加载正确（degraded=3s, hover=6s, failsafe=10s）
- 无地面站连接时 bridge 不触发任何 failsafe 动作（`is_heartbeat_ready_=false` 跳过检测）
- 需地面站实际连接后进行完整联调验收
