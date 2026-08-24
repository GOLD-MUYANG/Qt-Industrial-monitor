# Qt Industrial Monitoring

这是一个按四周边界逐步实现的 Qt 6 工业监控学习项目。当前已完成前两周：第一周建立 CMake 多目标骨架、动态协议 SDK、Modbus TCP 插件和 VirtualPLC；第二周加入每设备通信线程、500 ms 周期采集、单读请求在途、结构化错误、断线退避重连，以及独立数据线程中的质量状态与最近 60 秒滚动统计。VirtualPLC 内嵌中文字体，在没有系统 CJK 字体时也能正常显示中文。

当前 `industrial_monitor` 是第二周控制台验收入口；完整上位机 Widgets 页面、报警、SQLite 和历史查询属于后续周次，尚未实现。

## 环境依赖

- CMake 3.22+
- 支持 C++17 的编译器
- Qt 6.2+：Core、Widgets、Network、Sql、SerialBus、Charts、Test

Ubuntu 22.04 建议安装完整开发包：

```bash
sudo apt install qt6-base-dev libqt6charts6-dev \
    libqt6serialbus6-dev libqt6serialbus6-bin libqt6serialport6-dev
```

当前验证环境的 `libqt6serialbus6-dev` 缺少 `canbusutil` 和 Qt6SerialPort 开发配置。`cmake/ResolveQtSerialBus.cmake` 只在这种不完整安装下验证 SerialBus 头文件与共享库并创建 `Qt6::SerialBus` 导入目标；完整 Qt 安装仍使用官方 CMake 包。

## 构建与测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

测试共 12 项：9 个单元测试验证协议元类型、寄存器、确定性模拟、Codec、插件失败路径、滚动统计、数据质量、通信线程和中文字形；3 个集成测试通过本地 TCP 验证 VirtualPLC Server、Worker 在途/重连状态机，以及动态插件—通信线程—数据线程完整链路。若执行环境禁止监听 socket，3 个集成测试需要在允许本地环回网络的终端运行。

字体字形测试使用 Qt offscreen 平台执行，不需要桌面环境：

```bash
QT_QPA_PLATFORM=offscreen ./build/bin/tst_virtual_plc_ui_font -o -,txt
```

内嵌字体的来源、版权和许可证见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 第二周演示

终端一启动带界面的 VirtualPLC，默认监听 `127.0.0.1:1502`、Unit ID `1`：

```bash
./build/bin/virtual_plc
```

终端二运行控制台验收入口。它从可执行文件同级的 `plugins/` 动态加载 `libmodbus_tcp_plugin.so`，在通信线程周期读取 5 个寄存器，再由数据线程输出当前值、min/max/average、质量和窗口样本数。默认运行 15 秒并顺序停止两个线程：

```bash
./build/bin/industrial_monitor
```

为了手动演示断线重连，可运行 30 秒，在此期间关闭并重新启动终端一的 VirtualPLC；输出会经历 `Online -> Reconnecting -> Online`，数据质量会经历 `Good -> Stale -> Good`：

```bash
./build/bin/industrial_monitor --duration-ms 30000
```

也可指定端点和运行时长；`--duration-ms 0` 表示不自动停止：

```bash
./build/bin/industrial_monitor --host 127.0.0.1 --port 1502 --duration-ms 15000
```

## 寄存器地址

| Qt 地址 | 工业表示 | 数据 |
| ---: | ---: | --- |
| 0 | 40001 | 温度，`qint16 / 10` ℃ |
| 1 | 40002 | 压力，`quint16 / 100` MPa |
| 2 | 40003 | 实际转速，rpm |
| 3 | 40004 | 电压，`quint16 / 10` V |
| 4 | 40005 | 运行状态位 |
| 10 | 40011 | 目标转速，可写，rpm |

传给 `QModbusDataUnit` 的是从 0 开始的 Qt 地址，不是 40001 形式的工程表示。

详细架构、后续周次范围和验收标准见 [`文档/工业监控系统设计.md`](文档/工业监控系统设计.md)。
