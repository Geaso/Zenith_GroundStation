# Zenith GroundStation

Zenith 的 Windows 地面站，使用 Qt 6 / QML 和 C++，提供遥测显示、三维地图、航线任务、参数管理、机载任务及 LR24 数传配对。

## 代码结构

| 目录 | 职责 |
| --- | --- |
| `src` | 协议、传输、遥测模型、命令调度和 Qt/QML 接口 |
| `qml` | 页面与可复用界面组件 |
| `config` | 机载任务和数传配对配置 |
| `tests` | 协议、模型、地图与配对回归测试 |
| `third_party` | 固定版本第三方依赖及许可证 |
| `tools` | Windows 构建和发布打包 |

机载配套仓库：[Zenith_ws](https://github.com/Geaso/Zenith_ws)。

## 构建与测试

已使用的工具链是 Qt 6.6.3 MinGW 64 位和 MinGW 13.1。将 Qt 与编译器的 `bin` 目录加入当前终端 PATH 后：

```powershell
cmake -S . -B build-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/mingw_64
cmake --build build-release --parallel 3
ctest --test-dir build-release --output-on-failure
```

发布入口为 `tools/make_release.bat`，交付配置位于 `config/`。软件测试不代替串口硬件联调或实飞验证。

## 分支

`main` 为整合基线，`develop` 用于日常开发，`refactor/mavlink2-architecture` 用于 MAVLink 2 和工程结构重构。
