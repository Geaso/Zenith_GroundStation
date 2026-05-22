# GroundStationQt GUI 美化工作记录

**日期：** 2026-03-25
**项目路径：** `C:\Users\13655\Desktop\GroundStationQt`
**修改文件：** `qml/Main.qml` · `qml/pages/OverviewPage.qml` · `qml/pages/MissionPage.qml` · `qml/pages/ScriptsPage.qml`

---

## 一、整体风格变更

| 项目 | 改造前 | 改造后 |
|------|--------|--------|
| 主色调 | 浅灰白 `#EEF3F9` | 深色 `#0D1117`（GitHub Dark 风格） |
| 卡片背景 | `#F8FAFD` | `#161B22` |
| 次级元素 | `#F7FAFD` | `#21262D` |
| 边框 | `#D9E3F0` | `#30363D` |
| 强调色 | `#2F6BFF` | `#1F6FEB` / `#58A6FF` |

---

## 二、Main.qml — Header / Footer 重构

### Header（72px，三段式）

```
[ZENITH / Ground Station] [机型▼] [连接设置]  |  [概览][地图][脚本]  [链路徽章][解锁徽章][电量徽章][飞控][模式][GPS][RC][任务]  |  时间 / 载具 / 摘要
```

**关键改进：**
- **HeaderBadge 内联组件**：每个状态以"小标签+值"徽章形式显示，背景色随 `accent` 动态着色
- **动态变色逻辑：**
  - 链路：已连接 → 绿色 `#3FB950`，断开 → 红色 `#F85149`
  - 解锁：Armed → 红色警示，Disarmed → 绿色安全
  - 电量：`< 20%` → 红色，`< 40%` → 黄色 `#E3B341`，正常 → 绿色
  - GPS：3D Fix → 绿色，其他 → 黄色
- Tab 导航从第二行移入 Header 中段，节省垂直空间

### Footer（30px）

- 显示：UDP / TCP / Heartbeat 链路状态、控制器模式、飞行状态、航向角、回家距离、上次指令
- 链路断开时文字变红
- **FooterItem 内联组件** 统一样式

---

## 三、OverviewPage.qml — 三栏重构

### 布局

```
┌─────────────┬──────────────────────────────┬─────────────┐
│  遥测数据   │       视频 / 画面区          │  状态 + 操控 │
│  220px      │         弹性宽度             │   240px     │
│             │                              │             │
│ 位置 [m]   │  网格背景 Canvas             │ 状态徽章    │
│ 速度 [m/s] │  LIVE 标签                   │ 快捷指令    │
│ 姿态 [deg] │  高度叠加显示                │ 手动控制    │
│ 期望位置   │  时间戳                      │ X/Y/Z/Yaw  │
│ 期望速度   │                              │ 上传指令    │
│ 消息反馈   │                              │             │
└─────────────┴──────────────────────────────┴─────────────┘
```

**关键改进：**
- **TelemetryGroup 内联组件**：每组"标题 + 三列数值"，高度固定 96px，数值使用等宽字体防抖动
- 数值 `elide: Text.ElideRight` 防止长数字越界
- **StatusBadge 内联组件**：替换原 StatusPill，带动态 accent 颜色背景
- **MiniField 内联组件**：X/Y/Z/Yaw 两两并排，节省垂直空间
- 取消原左侧 92px 无实质内容的分类栏
- 所有内容高度通过 `root.height - 固定高度之和` 计算，适配视口无滚动

---

## 四、MissionPage.qml — 数据绑定 + 布局优化

**关键改进：**
- 任务信息面板全部绑定真实 `appState` 属性（原为硬编码占位符）：
  - 当前位置 → `appState.positionX / positionY`
  - 当前高度 → `appState.altitude`
  - 剩余航点 → `appState.waypointPoints.length`
  - 任务阶段 → `appState.missionStage`（紫色高亮）
  - 回家距离 → `appState.homeDistance`
- 航点列表加 `clip: true`，ListView 高度自适应，防越界
- 地图 Canvas 添加 `Connections` 监听 `pathChanged` / `telemetryChanged` 信号，数据变化自动重绘
- 图例添加到地图右下角（实际轨迹/航点路径/当前位置）
- **LegendDot / MissionRow** 内联组件统一样式
- 航点编号改为带蓝色圆角标签，视觉更清晰

---

## 五、ScriptsPage.qml — 主题统一

- 配色切换为深色主题，与其他页面统一
- 列表项高度 50px，包含名称下方的蓝色装饰线
- 运行/删除按钮改为深色底色，减少视觉噪音
- 显示脚本总数计数 `scriptActionModel.count + " 条脚本"`

---

## 六、新增内联组件一览

| 组件名 | 所在文件 | 用途 |
|--------|----------|------|
| `HeaderBadge` | Main.qml | Header 状态徽章，支持动态 accent 颜色 |
| `FooterItem`  | Main.qml | Footer 键值对显示，断开时变红 |
| `TelemetryGroup` | OverviewPage.qml | 三列遥测数值块，等宽字体防抖动 |
| `StatusBadge` | OverviewPage.qml | 状态行，背景色跟随 accent |
| `MiniField`   | OverviewPage.qml | 小型输入框，两两并排 |
| `LegendDot`   | MissionPage.qml | 地图图例点 |
| `MissionRow`  | MissionPage.qml | 任务信息键值行 |

---

## 七、未改动内容

- 所有 C++ 后端文件（`AppState`、`TelemetryStore`、`ZenithProtocolClient`、`CommandDispatcher`）**未修改**
- 组件库文件（`PrimaryButton.qml`、`StatusPill.qml`、`MetricTile.qml`、`InfoCard.qml`、`SectionTitle.qml`）**未修改**
- `CMakeLists.txt` / `resources/qml.qrc` **未修改**
