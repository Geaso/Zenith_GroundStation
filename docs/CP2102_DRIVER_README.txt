CP2102 / CP210x 驱动安装说明
============================

随地面站提供的驱动：Silicon Labs CP210x Universal Windows Driver 11.5.0
适用系统：Windows 10 1803 及以上（x64/x86）、Windows 11（x64）
适用芯片：CP2102、CP2102N、CP2103、CP2104、CP2105、CP2108、CP2109

安装方法
--------
1. 双击地面站目录中的 Install_CP2102_Driver.cmd。
2. 阅读 Silicon Labs 驱动许可，关闭许可窗口后按 Y 接受并继续。
3. 在 Windows 管理员授权窗口中选择“是”。
4. 看到 [OK] 后，重新插拔 USB 设备。
5. 在 Windows 设备管理器的“端口 (COM 和 LPT)”中确认 COM 口。

如果一键入口被安全策略拦截
--------------------------
进入 drivers\CP210x，右键 silabser.inf，选择“安装”。

注意
----
- 驱动文件和数字签名均保持官方原样；安装入口调用 Windows 自带的 pnputil。
- 驱动安装不等于地面站自动选中串口，首次连接仍需确认实际 COM 号。
- drivers\CP210x\UpdateParam.bat 是官方高级参数工具，普通客户不要运行。
- Silicon Labs 许可原文位于 drivers\CP210x\SLAB_License_Agreement_VCP_Windows.txt。

官方来源
--------
https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers
https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip

官方 ZIP SHA256
7CBA499E944F0CD6C6DE4A3C80A4646E9B0307D6704BCFA155A11F05774345E8
