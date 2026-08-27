# QML Parallel UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保留现有 Widgets 前端和后端线程模型的前提下，交付可独立构建、安装、测试和演示的 `industrial_monitor_qml`。

**Architecture:** 新目标只链接 `monitor_runtime`、`monitor_application` 和 Qt Quick 相关模块。`QmlApplicationFacade` 是 QML 唯一的 C++ 入口，在 UI 主线程把 `ApplicationController` 的值对象信号投影到 QML 专用模型，并把基础类型命令组装回现有 DTO。

**Tech Stack:** C++17、Qt 6.2.4、Qt QML、Qt Quick Controls 2、Qt Charts、Qt Test、CMake 3.22。

## Global Constraints

- 不修改现有通信、数据、领域、存储和 Widgets 展示层的 `.cpp/.h` 文件。
- 不修改 `industrial_monitor` 的入口和链接关系。
- QML 不持有协议 Worker、`QThread`、`QTimer`、`QModbusTcpClient` 或 `QSqlDatabase`。
- QML 只传基础类型，不直接构造协议、报警或存储 DTO。
- 目标环境为 Qt 6.2.4；入口使用资源 URL，不依赖较新版本才提供的 `QQmlApplicationEngine::loadFromModule()`。
- 当前工作区包含此前里程碑的未提交源码，所有提交动作暂停，避免把既有用户改动混入本任务提交。
- `qml6-module-qtcharts` 已由用户安装；系统尚缺 `qml6-module-qtqml-workerscript` 与 `qml6-module-qtquick-templates`。最终验证需如实区分系统 import 结果和使用同版本临时模块的运行结果。

---

### Task 1: 锁定 QML 目标、资源路径和最小加载边界

**Files:**
- Create: `src/monitor_qml/CMakeLists.txt`
- Create: `src/monitor_qml/main.cpp`
- Create: `src/monitor_qml/qml/Main.qml`
- Modify: `CMakeLists.txt`
- Modify: `cmake/InstallLayout.cmake`
- Test: `tests/unit/tst_qml_smoke.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ApplicationController(QString pluginDirectory, QString databasePath)`、`configureChineseUiFont()`。
- Produces: `industrial_monitor_qml`、资源 URL `qrc:/qt/qml/IndustrialMonitor/Qml/Main.qml`、QML URI `IndustrialMonitor.Qml`。

- [ ] **Step 1: 写最小 QML 加载失败测试**

```cpp
void QmlSmokeTest::loadsMainWindow()
{
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appFacade"), &m_facade);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/IndustrialMonitor/Qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);
}
```

- [ ] **Step 2: 运行测试并确认因目标/资源尚不存在而失败**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build --target tst_qml_smoke -j2`

Expected: FAIL，`tst_qml_smoke` 或 QML 资源尚未定义。

- [ ] **Step 3: 添加 Qt 组件、QML 模块和最小窗口**

```cmake
find_package(Qt6 6.2 REQUIRED COMPONENTS Qml Quick QuickControls2 Charts)
add_subdirectory(src/monitor_qml)

qt_add_executable(industrial_monitor_qml main.cpp)
qt_add_qml_module(industrial_monitor_qml
    URI IndustrialMonitor.Qml
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES qml/Main.qml
)
```

入口按 `QApplication -> configureChineseUiFont -> 参数解析 -> Controller -> Facade -> Engine load -> rootObjects 检查 -> Controller::start` 装配；`aboutToQuit` 只调用一次 `shutdown()`。

- [ ] **Step 4: 构建并确认最小 QML 目标通过**

Run: `cmake --build build --target industrial_monitor_qml tst_qml_smoke -j2`

Expected: 两个目标成功；若仅运行测试失败，应明确显示缺少 `QtCharts` QML import，而非资源 URL 错误。

### Task 2: 用 TDD 实现五个 QML 展示模型

**Files:**
- Create: `src/monitor_qml/models/QmlRealtimeModel.h`
- Create: `src/monitor_qml/models/QmlRealtimeModel.cpp`
- Create: `src/monitor_qml/models/QmlTrendModel.h`
- Create: `src/monitor_qml/models/QmlTrendModel.cpp`
- Create: `src/monitor_qml/models/QmlDeviceViewModel.h`
- Create: `src/monitor_qml/models/QmlDeviceViewModel.cpp`
- Create: `src/monitor_qml/models/QmlProtocolModel.h`
- Create: `src/monitor_qml/models/QmlProtocolModel.cpp`
- Create: `src/monitor_qml/models/QmlAlarmModel.h`
- Create: `src/monitor_qml/models/QmlAlarmModel.cpp`
- Test: `tests/unit/tst_qml_realtime_model.cpp`
- Test: `tests/unit/tst_qml_trend_model.cpp`
- Test: `tests/unit/tst_qml_device_view_model.cpp`
- Test: `tests/unit/tst_qml_protocol_model.cpp`
- Test: `tests/unit/tst_qml_alarm_model.cpp`

**Interfaces:**
- Consumes: `RealtimeSnapshotBatch`、`DeviceConfig`、`DeviceState`、`ProtocolDescriptor`、`AlarmRecord`。
- Produces: QML 命名角色；`QmlTrendModel` 的第 0/1 列分别为 epoch 毫秒和值；`QmlDeviceViewModel` 提供只读事实属性。

- [ ] **Step 1: 为实时模型写 RED 测试**

验证固定五行及 `tagId/displayName/unit/currentValue/minimumValue/maximumValue/averageValue/quality/qualityText/timestampText` 角色；未知 tag 被忽略；只更新命中行且数值保持 `double`。

- [ ] **Step 2: 实现最小实时模型并跑 GREEN**

Run: `cmake --build build --target tst_qml_realtime_model -j2 && ./build/bin/tst_qml_realtime_model`

Expected: PASS，且 `QSignalSpy(dataChanged)` 的范围只覆盖对应行。

- [ ] **Step 3: 为趋势模型写 RED 测试**

```cpp
QCOMPARE(model.columnCount(), 2);
QCOMPARE(model.selectedTagId(), QStringLiteral("temperature"));
model.applySnapshots(makeSnapshots(121, 500));
QVERIFY(model.rowCount() <= 120);
model.setDisplayPaused(true);
const int notifications = resetSpy.size();
model.applySnapshots(makeSnapshotAt(61'000));
QCOMPARE(resetSpy.size(), notifications);
model.setDisplayPaused(false);
QCOMPARE(resetSpy.size(), notifications + 1);
```

- [ ] **Step 4: 实现趋势缓存、选择和暂停语义并跑 GREEN**

裁剪规则为每个测点最多 120 点且相对该测点最新 UTC 时间不超过 60 秒；`status` 不进入趋势。暂停期间更新内部缓存但不发模型变化，恢复时一次 reset。

Run: `cmake --build build --target tst_qml_trend_model -j2 && ./build/bin/tst_qml_trend_model`

- [ ] **Step 5: 为设备、协议和报警模型分别写 RED 测试**

设备测试覆盖配置事实、连接状态与最近 Good 通信时间；协议测试覆盖按 key 去重更新和 `indexOfKey()`；报警测试覆盖稳定 ID upsert、活动过滤、历史保留及未确认状态的 `acknowledgeable=true`。

- [ ] **Step 6: 实现三个模型并逐个跑 GREEN**

Run: `cmake --build build --target tst_qml_device_view_model tst_qml_protocol_model tst_qml_alarm_model -j2`

Expected: 三个测试目标全部 PASS。

### Task 3: 用 Facade 固化唯一 QML/C++ 边界

**Files:**
- Create: `src/monitor_qml/bridge/QmlApplicationFacade.h`
- Create: `src/monitor_qml/bridge/QmlApplicationFacade.cpp`
- Test: `tests/unit/tst_qml_application_facade.cpp`

**Interfaces:**
- Consumes: `ApplicationController` 的 8 个公开状态信号和 5 个公开命令。
- Produces: `realtimeModel`、`trendModel`、`deviceModel`、`protocolModel`、`activeAlarmModel`、`alarmHistoryModel`、`statusMessage`、`statusHealthy`、`initialized`、`commandBusy`、`displayPaused`；以及 7 个 QML 基础类型命令。

- [ ] **Step 1: 写 Facade 状态投影 RED 测试**

通过直接发出 Controller 的公开信号验证协议、初始化、快照、设备状态、报警、写入结果、存储状态和致命错误均更新正确的模型或属性。

- [ ] **Step 2: 写输入校验 RED 测试**

覆盖空主机、端口 `0/65536`、Unit ID `0/248`、轮询 `<50`、超时 `<1`、目标转速 `<0/>65535`；非法输入不得调用 Controller，必须写入明确中文错误并保持 `statusHealthy=false`。

- [ ] **Step 3: 实现 Facade 最小行为并跑 GREEN**

`saveDevice()` 从最近一次 `DeviceConfig` 复制身份字段及未编辑策略字段，只覆盖页面可编辑字段；连接和写入命令只在初始化后发送；写结果、配置回传、设备状态或错误负责收敛忙状态。

Run: `cmake --build build --target tst_qml_application_facade -j2 && ./build/bin/tst_qml_application_facade`

Expected: PASS。

### Task 4: 实现应用壳、公共组件和三个业务页面

**Files:**
- Create: `src/monitor_qml/qml/Theme.qml`
- Create: `src/monitor_qml/qml/components/StatusBadge.qml`
- Create: `src/monitor_qml/qml/components/MetricCard.qml`
- Create: `src/monitor_qml/qml/components/LabeledField.qml`
- Create: `src/monitor_qml/qml/components/SectionPanel.qml`
- Create: `src/monitor_qml/qml/pages/RealtimePage.qml`
- Create: `src/monitor_qml/qml/pages/DevicePage.qml`
- Create: `src/monitor_qml/qml/pages/AlarmPage.qml`
- Modify: `src/monitor_qml/qml/Main.qml`
- Modify: `tests/unit/tst_qml_smoke.cpp`

**Interfaces:**
- Consumes: `appFacade` 的所有属性、模型和 `Q_INVOKABLE` 命令。
- Produces: `mainWindow`、`navigationList`、`globalStatusBar`、`realtimePage`、`devicePage`、`alarmPage` 及主要表单/按钮的稳定 `objectName`。

- [ ] **Step 1: 扩充 QML 冒烟 RED 测试**

递归查找三个页面、导航、全局状态、连接/断开、目标转速、保存设备和确认报警入口；断言中文标题与可见标签存在。

- [ ] **Step 2: 实现主题和公共组件**

使用高对比浅色内容区、深蓝侧栏、蓝色主操作、绿/琥珀/红状态色；正文不小于 14px，按钮/输入高度不小于 44px，焦点边框明显，状态文字与颜色同时出现，不使用 emoji 图标。

- [ ] **Step 3: 实现实时页**

五测点使用 `GridView + MetricCard`；趋势使用 `ChartView + LineSeries + VXYModelMapper`，并提供统计卡片作为非图形替代；暂停只调用 `setDisplayPaused()`；写入成功仅来自 Facade 的异步回传状态。

- [ ] **Step 4: 实现设备页和报警页**

设备表单先在 QML 做就地校验，再调用 Facade 二次校验；协议 key 来自模型。报警以稳定 ID 选择记录，确认按钮只对 `acknowledgeable` 行启用，备注保留至命令提交。

- [ ] **Step 5: 运行 offscreen 冒烟测试**

Run: `QT_QPA_PLATFORM=offscreen ./build/bin/tst_qml_smoke -o -,txt`

Expected: 安装 `qml6-module-qtcharts` 后 PASS；未安装时应只剩 `module "QtCharts" is not installed` 阻塞。

### Task 5: 验证真实后端投影、退出顺序与安装布局

**Files:**
- Test: `tests/integration/tst_qml_backend_closed_loop.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `cmake/InstallLayout.cmake`

**Interfaces:**
- Consumes: 动态 Modbus 插件、`VirtualPlcServer`、`ApplicationController`、`QmlApplicationFacade`。
- Produces: `VirtualPLC -> ModbusTcpWorker -> DeviceSession -> DataPipeline -> Controller -> Facade -> QML model` 真实闭环证据。

- [ ] **Step 1: 写真实闭环 RED 测试**

启动随机环回端口、初始化临时 SQLite、等待 Facade 初始化，执行保存配置、连接、实时模型更新、写 1800 rpm、高温报警与确认、断开和 `shutdown()`；断言 Controller 不再运行。

- [ ] **Step 2: 运行并确认 RED 原因是 Facade/模型链尚未接入测试目标**

Run: `ctest --test-dir build -R '^integration.qml_backend_closed_loop$' --output-on-failure`

- [ ] **Step 3: 接入目标并跑 GREEN**

Run: `ctest --test-dir build -R '^integration.qml_backend_closed_loop$' --output-on-failure`

Expected: 在允许环回监听的环境 PASS；受限沙箱中只允许以 `port != 0` 记录环境失败。

- [ ] **Step 4: 验证安装布局**

Run: `cmake --install build --prefix /tmp/industrial-monitor-qml-install`

Expected: `bin/industrial_monitor`、`bin/industrial_monitor_qml`、`bin/virtual_plc`、`bin/plugins/libmodbus_tcp_plugin.so` 和协议共享库均存在。

### Task 6: 全量回归和文档回写

**Files:**
- Modify: `README.md`
- Modify: `文档/QML并行界面设计.md`
- Modify: `文档/工业监控系统设计.md`

**Interfaces:**
- Consumes: 实际命令输出。
- Produces: 可复制构建/运行命令、真实完成清单、测试数量、已知环境边界和未实现范围。

- [ ] **Step 1: 重新配置和完整构建**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j2`

- [ ] **Step 2: 运行分层验证**

Run: `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`

Run: `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R 'qml' --output-on-failure`

- [ ] **Step 3: 运行静态与仓库卫生检查**

Run: `qmllint src/monitor_qml/qml/Main.qml src/monitor_qml/qml/pages/*.qml src/monitor_qml/qml/components/*.qml`

Run: `git diff --check`

- [x] **Step 4: 回写文档**

只记录本轮真实执行的成功/失败命令；把缺少的 WorkerScript/Templates 运行包、沙箱 socket 限制和未执行 GUI 人工操作分别列出，不能把构建成功写成 QML 运行闭环成功。

**实际验收摘要（2026-08-27）：** 完整构建成功；同版本临时 QML import 环境下 22/22 非监听测试通过；允许环回监听的环境下 5/5 集成测试通过。Qt 6.2.4 Ubuntu 包的 qmllint/qmltypes 不完整，静态检查会把 `QObject.objectName`、`Timer`、`ValueAxis`等真实运行时已成功创建的类型误报为不存在；因此保留为工具链边界，以 QML cache 编译和真实 Engine 无警告加载作为运行证据。
