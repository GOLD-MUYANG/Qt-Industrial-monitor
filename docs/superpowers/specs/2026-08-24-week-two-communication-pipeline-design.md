# 工业监控系统第二周设计收敛

## 目标与范围

第二周把第一周的“一次读取”扩展为可长期运行的采集链路：每台设备的 Modbus Worker 在独立通信线程中周期轮询；网络中断后按退避策略自动重连；有效样本进入共享的数据处理线程，产生带质量状态的最近 60 秒滚动统计。

本周不实现报警生命周期、SQLite、历史查询或 Widgets 页面。这些仍属于第三周。控制台入口只负责装配和展示第二周链路，不承载协议、统计或线程管理逻辑。

## 采用方案

采用“协议 Worker + DeviceSession + DataPipeline”三段式方案：

1. `ModbusTcpWorker` 拥有 `QModbusTcpClient`、轮询定时器和重连定时器，只处理协议状态、请求和结构化通信错误。
2. `DeviceSession` 在主线程拥有一个 `QThread`，把插件创建的 Worker 移入该线程，并用 queued signal/slot 发送启动、停止和写入命令。
3. `DataPipeline` 在独立数据线程串行消费 `SampleBatch`，维护每个设备/测点的滚动窗口并发出 `RealtimeSnapshotBatch`。

没有采用把线程、协议和统计全部塞入 Worker 的方案，因为这会让插件 ABI 承担业务职责；也没有提前建设完整 `DeviceManager`/UI，因为第二周只有单设备验收，提前引入会扩大范围。

## 协议 DTO

`protocol_sdk` 增加或扩展以下值类型：

- `ConnectionState::Reconnecting`：网络故障后的可恢复状态，不复用永久错误 `Faulted`。
- `DeviceErrorCategory`：`Configuration`、`Connection`、`Timeout`、`Protocol`、`Data`、`Lifecycle`。
- `DeviceError`：包含设备 ID、分类、Qt/本地错误码、请求 ID、是否可恢复和文本。
- `DeviceConfig`：增加轮询周期、连续失败阈值和可覆盖的重连延迟序列；生产默认值为 500 ms、3 次、`1/2/4/8/10` 秒。
- `TransactionLog::skipped`：明确记录因为上一读请求仍在途而跳过的轮询，不把它伪装成一次协议失败。
- `RealtimeSnapshot`：每个设备/测点的当前值、min/max/average、样本数、质量、时间和序号。
- `RealtimeSnapshotBatch`：一次管线输出的一组快照。

所有跨线程 DTO 使用 `Q_DECLARE_METATYPE`，并在建立 queued connection 前由 `registerProtocolMetaTypes()` 注册。

## 通信状态机与轮询

Worker 只能在 `start()` 所在线程创建 `QModbusTcpClient` 和两个 `QTimer`。配置错误进入 `Faulted`，网络错误进入 `Reconnecting`。

连接成功后立即发起一次读取并启动周期轮询。任一时刻最多有一个读请求在途；定时器再次触发时只发出 `skipped=true` 的事务日志。有效读取把连续失败数清零，并将重连退避恢复到第一档。读取超时、协议错误或无效数据累计到 3 次时主动断开并调度重连。

应用级重连使用单次定时器，生产延迟固定为 `1000, 2000, 4000, 8000, 10000` ms，耗尽后保持 10 秒。用户主动停止会停止两个定时器、断开客户端并且只发出一次 `stopped()`。

## 线程生命周期

`DeviceSession` 负责以下顺序：

```text
createWorker -> moveToThread -> QThread::started -> Worker::start
Worker signals -> DeviceSession proxy signals
requestStop -> Worker::stop -> Worker::stopped -> QThread::quit
QThread::finished -> Worker::deleteLater -> session stopped
```

`stopAndWait(timeoutMs)` 使用有限等待；超时只报告生命周期错误，不调用 `terminate()`。`DeviceSession` 不读取 Worker 内部成员，也不从主线程直接调用 Worker slot。

`DataPipeline` 由控制器移动到一个共享数据线程。通信线程到数据线程以及数据线程到主线程全部显式使用 `Qt::QueuedConnection`。

## 数据质量与滚动统计

管线先验证批次设备 ID、同批序号、严格递增序号、UTC 时间、已知测点和有限工程值。演示测点的合理范围为：温度 `-50..200 ℃`、压力 `0..10 MPa`、转速 `0..6000 rpm`、电压 `0..500 V`、状态字 `0..15`。

- `Good`：进入最近 60 秒窗口；每个窗口最多 120 个样本，并更新 current/min/max/average。
- `Stale`：保留最后一份有效统计，只把快照质量改成 Stale，不追加样本。
- `Bad`：不进入统计；有历史值时保留统计并输出 Bad 质量，没有历史值时只报告数据错误。

当设备进入 `Reconnecting` 或意外 `Stopped` 时，管线为该设备已有测点发出 Stale 快照。`Stopping` 是用户主动停止过程，不额外制造通信故障。

## 测试与验收

1. SDK 单测验证新增枚举、默认参数和 queued meta type。
2. DataPipeline 单测验证 60 秒/120 点窗口、Bad/Stale 不参与统计、非法序号和值域拒绝。
3. Worker 测试用不响应的本地 TCP 端点验证请求在途时只记录 skipped poll，用短测试退避验证重连状态。
4. DeviceSession 单测验证 Worker 的 `start/stop` 均在通信线程执行且线程可在有限时间内退出。
5. 动态插件集成测试反复停止并恢复同一个 VirtualPLC，验证 `Online -> Reconnecting -> Online`、恢复采集以及停止后无线程遗留。
6. 完整 CMake build 与 CTest 必须通过；需要监听本地端口的测试在允许环回网络的环境执行。

## 自查结论

本规格没有引入第三周功能；协议、线程和数据职责互不重叠；默认时间参数、状态和结束标准与 `文档/工业监控系统设计.md` 一致。测试可覆盖所有新增边界，不依赖 UI 或数据库。
