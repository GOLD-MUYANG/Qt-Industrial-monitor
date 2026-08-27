# Qt Industrial Monitoring

这是一个按里程碑逐步实现的 Qt 6 工业监控学习项目。当前已完成动态 Modbus TCP 插件与 VirtualPLC、每设备通信线程和断线重连、60 秒实时统计、报警生命周期、SQLite 独立读写线程，以及实时监控/设备管理/报警中心三个业务页面。Widgets 前端还增加了一个独立“视觉实验”页面，用 OpenCV 解码本地视频并按固定 HSV 规则检测高饱和红色物体。工程同时提供 `industrial_monitor` Widgets 前端和 `industrial_monitor_qml` Qt Quick 并行前端，两者共用同一个 `ApplicationController` 和通信/数据/报警/存储后端；视觉页面目前只在 Widgets 前端提供。

VirtualPLC 与 IndustrialMonitor 共用应用内嵌中文字体，在没有系统 CJK 字体时也能正常显示中文。完整历史页面、CSV 导出、通信日志页面和滚动文件日志当前尚未实现，不属于本次视觉实验增量。

## 本次 QML 并行界面更新

| 更新项 | 实际内容 |
| --- | --- |
| 新程序入口 | 新增 `industrial_monitor_qml`，原 `industrial_monitor` Widgets 程序保留 |
| 后端复用 | 两个前端共用 `ApplicationController`、动态协议插件、通信线程、数据线程、报警引擎和 SQLite Worker |
| QML/C++ 边界 | 新增 `QmlApplicationFacade`，QML 只传递基础类型，不持有 Worker、`QThread`、Modbus Client 或数据库连接 |
| 展示模型 | 新增实时、趋势、设备、协议和报警五个 QML 专用模型 |
| 业务页面 | 完成实时监控、设备管理和报警中心，支持连接/断开、1800 rpm 写入、配置保存和报警确认 |
| 安全边界 | 命令 busy 只由对应异步结果收敛；停用设备和非 `Stopped` 状态不会重复发起连接 |
| 测试 | 新增 QML Facade/模型单元测试、真实 Engine 冒烟测试和 Modbus QML 后端闭环测试 |

新前端的主要运行链是：

```text
VirtualPLC
-> ModbusTcpWorker                  [通信线程]
-> DeviceSession                    [UI 主线程编排]
-> DataPipeline/AlarmEngine         [数据线程]
   |-> StorageWorker                [SQLite 写线程]
   `-> ApplicationController        [UI 主线程]
       -> QmlApplicationFacade/QML 模型
       -> Qt Quick 页面
```

## OpenCV 视觉实验

“视觉实验”是与 Modbus、报警和 SQLite 隔离的学习模块，运行链为：

```text
VisionPage                         [UI 主线程]
-> ApplicationController
-> VisionSession                  [主线程管理生命周期]
-> VideoFileWorker                [视觉线程]
   -> cv::VideoCapture + QTimer
   -> ColorObjectDetector
   -> VisionFrameConverter
-> 独立 QImage 值对象
-> VisionPage 创建 QPixmap        [UI 主线程]
```

它支持选择本地视频、首帧预览、播放、暂停、继续、停止和结束后重播；每帧先按需缩小到最大 1280 像素宽，再做两段 HSV 红色阈值、3x3 开闭去噪、500 px 面积过滤、外接框和计数。跨线程结果中的 `QImage` 已深拷贝，不引用下一帧会覆盖的 `cv::Mat` 内存。

该模块是固定颜色规则实验，不是 AI、YOLO、通用目标识别或生产质检系统；它不访问摄像头，不保存检测结果，也不把视觉计数接入报警、Modbus 或 SQLite。详细能力边界见 [`文档/OpenCV视频目标检测设计.md`](文档/OpenCV视频目标检测设计.md)。

## 环境依赖

- CMake 3.22+
- 支持 C++17 的编译器
- Qt 6.2+：Core、Widgets、Network、Sql、SerialBus、Qml、Quick、QuickControls2、Charts、Test
- OpenCV 4：core、imgproc、videoio

Ubuntu 22.04 建议安装完整开发包：

```bash
sudo apt install qt6-base-dev qt6-declarative-dev libqt6charts6-dev \
    libqt6serialbus6-dev libqt6serialbus6-bin libqt6serialport6-dev \
    libopencv-dev \
    qml6-module-qtqml-workerscript qml6-module-qtquick \
    qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates qml6-module-qtquick-window \
    qml6-module-qtcharts
```

当前验证环境的 `libqt6serialbus6-dev` 缺少 `canbusutil` 和 Qt6SerialPort 开发配置。`cmake/ResolveQtSerialBus.cmake` 只在这种不完整安装下验证 SerialBus 头文件与共享库并创建 `Qt6::SerialBus` 导入目标；完整 Qt 安装仍使用官方 CMake 包。

2026-08-27 已在当前系统确认 `qml6-module-qtcharts`、`qml6-module-qtqml-workerscript` 和 `qml6-module-qtquick-templates` 均已安装；不设置临时 `QML2_IMPORT_PATH` 时，offscreen 真实 QML Engine 冒烟测试为 3/3 通过。

## 构建与测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

测试共 35 项：30 项不需要监听端口，覆盖协议/插件、通信线程、统计与数据质量、报警状态机、SQLite、Widgets 展示层、QML Facade/五个模型、QML offscreen 加载、中文字形，以及视觉算法、播放进度策略、图像所有权、视频 Worker、视觉线程、页面状态和控制器隔离；5 个集成测试通过本地 TCP 验证 VirtualPLC、周期 Worker、动态插件、第三周报警—SQLite 闭环和 QML 后端投影闭环。若执行环境禁止监听 socket，这 5 项需要在允许本地环回网络的终端运行。

字体字形测试使用 Qt offscreen 平台执行，不需要桌面环境：

```bash
QT_QPA_PLATFORM=offscreen ./build/bin/tst_virtual_plc_ui_font -o -,txt
```

内嵌字体的来源、版权和许可证见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 运行与演示

以下命令都在项目根目录执行。终端一启动带界面的 VirtualPLC，默认监听 `127.0.0.1:1502`、Unit ID `1`：

```bash
./build/bin/virtual_plc
```

终端二只启动一个前端。两个入口都从可执行文件同级的 `plugins/` 动态加载 `libmodbus_tcp_plugin.so`：

```bash
# Widgets 前端
./build/bin/industrial_monitor \
    --plugin-dir ./build/bin/plugins \
    --database /tmp/industrial-monitor-widgets.db

# 或 Qt Quick 并行前端
./build/bin/industrial_monitor_qml \
    --plugin-dir ./build/bin/plugins \
    --database /tmp/industrial-monitor-qml.db
```

两个前端是互斥的可选入口，不要同时用它们控制同一设备；也不要让两个进程同时打开同一 SQLite 文件。上面使用不同数据库文件，便于切换前端时保留各自的演示数据。

QML 界面启动后的最短使用路径：

1. 进入“设备管理”，确认协议为 Modbus TCP、主机为 `127.0.0.1`、端口为 `1502`、Unit ID 为 `1`。
2. 点击“保存并应用”，等待全局状态条显示保存成功。
3. 进入“实时监控”点击“连接设备”，等待状态变为“在线”。
4. 查看五个测点、60 秒趋势和统计；暂停显示不会停止采集、报警或入库。
5. 输入 `1800` 并写入目标转速；只有收到后端异步结果后界面才显示成功。
6. 在 VirtualPLC 注入高温或停止服务，然后在“报警中心”观察激活、确认、恢复和自动重连。

Widgets 视觉实验不要求 VirtualPLC 正在运行。进入“视觉实验”后选择一段包含红色杯子、卡片或瓶盖的本地视频，首帧会先显示检测结果；随后可播放、暂停、继续、停止，并在播放结束后点击“重播”从头打开。无红色目标是正常结果，页面显示 0；容器或编解码器不受当前 OpenCV 后端支持时，错误只显示在视觉页。

### 功能演示细节

连接成功后，“实时监控”页每 500 ms 更新五个测点、最近 60 秒统计和可选测点曲线。

目标转速写入演示：在“实时监控”页输入 `1800 rpm` 并点击“写入目标转速”。界面只在收到异步 `writeFinished` 后显示成功；VirtualPLC 的 40011 寄存器更新为 1800，实际转速按每 500 ms 最多 50 rpm 的步长逐步逼近。设备页的协议选项来自当前实际加载的插件描述，启用状态与连接参数一起保存到 SQLite；在线保存会先停止旧会话再以新配置重建。

高温报警演示：

1. 在 VirtualPLC 点击“启用高温 86.3 ℃”。
2. 连续 3 个 Good 样本后，IndustrialMonitor 的“报警中心”出现一条 Critical 温度报警。
3. 可选填写备注并确认；确认不会伪造恢复。
4. 在 VirtualPLC 清除高温，温度回到带 2 ℃ 回差的正常区间并连续 3 点后，同一报警记录更新为已恢复。

通信报警演示：

1. 在 VirtualPLC 点击“停止”，上位机经历 `Online -> Reconnecting`，最后值显示为 Stale，并生成一条 Communication/Critical 报警。
2. 再次启动 VirtualPLC，Worker 按退避策略自动连接；恢复有效采集后，同一通信报警更新为已恢复。

如果插件或数据库不在默认位置，Widgets 和 QML 两个入口都可以显式指定：

```bash
./build/bin/industrial_monitor \
    --plugin-dir ./build/bin/plugins \
    --database /tmp/industrial-monitor-demo.db

./build/bin/industrial_monitor_qml \
    --plugin-dir ./build/bin/plugins \
    --database /tmp/industrial-monitor-qml-demo.db
```

未指定 `--database` 时，SQLite 文件写入 `QStandardPaths::AppDataLocation`，不写入源码目录。空数据库首次迁移的默认设备、6 个测点和 5 条报警规则来自编译进程序资源的 `src/monitor/data/default_config.json`；首版明确只接受一台设备，避免多行配置时静默选错连接目标。

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
