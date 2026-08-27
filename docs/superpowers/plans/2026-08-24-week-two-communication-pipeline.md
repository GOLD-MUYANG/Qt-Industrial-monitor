# 工业监控系统第二周实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现通信线程、周期轮询、单请求在途、结构化错误、自动重连、数据质量与最近 60 秒滚动统计，并用真实 VirtualPLC 验证断线恢复和无线程遗留。

**Architecture:** `ModbusTcpWorker` 只管理通信线程内的 Qt Serial Bus 对象和状态机；主线程 `DeviceSession` 管理 Worker/QThread 生命周期；独立 `DataPipeline` 串行生成实时统计快照。跨线程只传递 protocol SDK 中已注册的值类型。

**Tech Stack:** C++17、CMake 3.22、Qt 6.2.4 Core/Network/SerialBus/Test、CTest

## Global Constraints

- 默认采集周期为 `500 ms`，单次请求超时 `800 ms`，Qt 协议重试 `1` 次，连续失败阈值 `3` 次。
- 应用级重连退避固定为 `1 s -> 2 s -> 4 s -> 8 s -> 10 s`。
- 每个设备最多一个读请求在途，跳过的轮询必须可观察。
- Worker 移入通信线程后才能创建 `QModbusTcpClient` 和 `QTimer`。
- Good 才进入最近 60 秒且最多 120 点的统计窗口；Stale/Bad 不更新统计。
- 本周不实现报警、SQLite、历史查询或完整 UI。
- 当前第一周源码尚未提交，本计划不创建提交，也不推送远程；每个任务用独立测试和文档勾选留证。

---

### Task 1: 扩展跨线程协议 DTO

**Files:**
- Modify: `src/protocol_sdk/include/industrial/protocol/ProtocolTypes.h`
- Modify: `src/protocol_sdk/ProtocolTypes.cpp`
- Modify: `tests/unit/tst_protocol_types.cpp`

**Interfaces:**
- Produces: `ConnectionState::Reconnecting`、`DeviceErrorCategory`、扩展后的 `DeviceConfig/DeviceError/TransactionLog`、`RealtimeSnapshot`、`RealtimeSnapshotBatch`。

- [x] **Step 1: 写失败测试**

```cpp
QCOMPARE(config.pollIntervalMs, 500);
QCOMPARE(config.consecutiveFailureLimit, 3);
QCOMPARE(config.reconnectDelaysMs, QList<int>({1000, 2000, 4000, 8000, 10000}));
registerProtocolMetaTypes();
QVERIFY(QMetaType::fromName("industrial::protocol::RealtimeSnapshotBatch").isValid());
```

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_protocol_types -j2`

Expected: 编译失败，指出新增字段或类型不存在。

- [x] **Step 3: 实现值类型和注册**

```cpp
enum class DeviceErrorCategory : quint8 {
    Configuration, Connection, Timeout, Protocol, Data, Lifecycle
};

struct RealtimeSnapshot {
    QString deviceId;
    QString tagId;
    double current = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    int sampleCount = 0;
    DataQuality quality = DataQuality::Bad;
    QDateTime timestampUtc;
    quint64 sequence = 0;
};
using RealtimeSnapshotBatch = QList<RealtimeSnapshot>;
```

- [x] **Step 4: 运行绿灯并更新设计文档 Task 1 状态**

Run: `ctest --test-dir build -R unit.protocol_types --output-on-failure`

Expected: 该测试通过。

### Task 2: 实现 DataPipeline 与滚动窗口

**Files:**
- Create: `src/monitor/data/RollingStatistics.h`
- Create: `src/monitor/data/RollingStatistics.cpp`
- Create: `src/monitor/data/DataPipeline.h`
- Create: `src/monitor/data/DataPipeline.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Create: `tests/unit/tst_rolling_statistics.cpp`
- Create: `tests/unit/tst_data_pipeline.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `RollingStatistics::add(double, QDateTime)`、`DataPipeline::processSamples(const SampleBatch&)`、`DataPipeline::handleDeviceState(const DeviceState&)`、`snapshotsReady(const RealtimeSnapshotBatch&)`、`pipelineError(const DeviceError&)`。

- [x] **Step 1: 写滚动窗口失败测试**

```cpp
window.add(10.0, base);
window.add(20.0, base.addMSecs(500));
window.add(30.0, base.addSecs(61));
QCOMPARE(window.count(), 1);
QCOMPARE(window.average(), 30.0);
```

- [x] **Step 2: 运行红灯并实现最小 RollingStatistics**

Run: `cmake --build build --target tst_rolling_statistics -j2`

Expected: 首次因类型不存在而失败；实现后测试通过，窗口同时按 60 秒和 120 点裁剪。

- [x] **Step 3: 写 DataPipeline 失败测试**

```cpp
pipeline.processSamples(goodBatch(10.0, 20.0));
QCOMPARE(snapshot.average, 15.0);
pipeline.processSamples(staleBatch(999.0));
QCOMPARE(snapshot.average, 15.0);
QCOMPARE(snapshot.quality, DataQuality::Stale);
```

- [x] **Step 4: 实现验证、质量规则和快照生成**

每个 `deviceId/tagId` 保存独立窗口和最后快照；非法设备、序号、时间、测点或值域发出 `DeviceErrorCategory::Data`。Good 更新统计，Stale/Bad 只覆盖输出质量。

- [x] **Step 5: 运行绿灯并更新设计文档 Task 2 状态**

Run: `ctest --test-dir build -R "unit.rolling_statistics|unit.data_pipeline" --output-on-failure`

Expected: 两个测试通过。

### Task 3: 扩展 Modbus Worker 状态机

**Files:**
- Modify: `src/plugins/modbus_tcp/ModbusTcpWorker.h`
- Modify: `src/plugins/modbus_tcp/ModbusTcpWorker.cpp`
- Modify: `src/plugins/modbus_tcp/CMakeLists.txt`
- Create: `tests/integration/tst_modbus_tcp_worker.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的结构化 DTO。
- Produces: 周期轮询、读请求在途门闩、结构化错误、`Online/Reconnecting/Faulted` 状态和可取消退避重连。

- [x] **Step 1: 写不响应端点下的失败测试**

```cpp
config.pollIntervalMs = 20;
config.timeoutMs = 100;
config.protocolRetries = 0;
worker.start();
QTRY_VERIFY_WITH_TIMEOUT(hasSkippedTransaction(logSpy), 1000);
```

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_modbus_tcp_worker -j2 && ctest --test-dir build -R integration.modbus_tcp_worker --output-on-failure`

Expected: 旧 Worker 没有周期轮询和 skipped 事务，断言失败。

- [x] **Step 3: 实现两个定时器、单请求在途和失败计数**

`onPollTimeout()` 在 `m_readInFlight` 为 true 时只发 skipped 日志；reply 完成后清除门闩。有效采集清零失败数和退避索引；达到阈值调用 `scheduleReconnect()`。

- [x] **Step 4: 写短退避的恢复失败测试并实现重连**

```cpp
config.reconnectDelaysMs = {20, 40, 80};
server.stop();
QTRY_VERIFY_WITH_TIMEOUT(sawState(states, ConnectionState::Reconnecting), 1500);
QVERIFY(server.start(QHostAddress::LocalHost, port, 1));
QTRY_VERIFY_WITH_TIMEOUT(onlineCount(states) >= 2, 3000);
```

- [x] **Step 5: 运行绿灯并更新设计文档 Task 3 状态**

Run: `ctest --test-dir build -R integration.modbus_tcp_worker --output-on-failure`

Expected: skipped 和断线恢复场景通过。

### Task 4: DeviceSession、完整线程链与第二周验收

**Files:**
- Create: `src/monitor/application/DeviceSession.h`
- Create: `src/monitor/application/DeviceSession.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `src/monitor/main.cpp`
- Create: `tests/unit/tst_device_session.cpp`
- Modify: `tests/integration/tst_modbus_plugin_integration.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Modify: `文档/工业监控系统设计.md`

**Interfaces:**
- Consumes: `IProtocolPlugin::createWorker()`、Task 2 的 `DataPipeline`、Task 3 的 Worker。
- Produces: `DeviceSession::start()`、`requestStop()`、`stopAndWait(int)` 以及代理的状态、样本、写结果、错误和日志信号。

- [ ] **Step 1: 写线程归属和有限停止失败测试**

```cpp
session.start();
QTRY_COMPARE_WITH_TIMEOUT(startThreadSpy.count(), 1, 1000);
QVERIFY(startThread != QThread::currentThread());
QVERIFY(session.stopAndWait(1000));
QVERIFY(!session.isRunning());
```

- [ ] **Step 2: 运行红灯并实现 DeviceSession**

Run: `cmake --build build --target tst_device_session -j2`

Expected: 首次因 `DeviceSession` 不存在而失败；实现后启动和停止均发生在通信线程。

- [ ] **Step 3: 扩展动态插件集成测试**

测试通过 `PluginManager -> DeviceSession -> communication QThread -> ModbusTcpWorker -> VirtualPLC` 连续采集；停止/重启服务器后再次 Online 和收到新序号；`stopAndWait()` 后线程不运行。

- [ ] **Step 4: 更新控制台装配和文档**

控制台使用 `DeviceSession`，把其样本以 queued connection 投递给数据线程中的 `DataPipeline`，输出实时快照；README 和主设计文档只声明实际完成并给出第二周演示命令。

- [ ] **Step 5: 完整验证**

Run: `cmake -S . -B build -DBUILD_TESTING=ON`

Run: `cmake --build build -j2`

Run: `ctest --test-dir build --output-on-failure`

Run: `git diff --check`

Expected: 配置与构建退出码为 0；全部 CTest 通过；差异无空白错误；设计文档第二周四项全部标记为完成并记录实际测试数量。
