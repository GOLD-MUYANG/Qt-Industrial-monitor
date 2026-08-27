# 第三周报警、SQLite 与主要界面设计

## 1. 目标与范围

本周把已经通过验证的第二周链路
`VirtualPLC -> ModbusTcpWorker -> DeviceSession -> DataPipeline`
继续闭合为：

```text
VirtualPLC
-> 通信线程采集/重连
-> 数据线程统计/报警
-> UI 主线程模型与页面
-> 数据库写线程持久化
-> 历史查询线程只读验证
```

交付范围严格对应主设计文档第三周：

1. 阈值报警的连续样本激活、回差恢复、确认和通信报警生命周期。
2. SQLite schema、WAL、预编译事务写入、测量降采样和独立历史查询连接。
3. 实时监控、设备管理、报警中心三个主要页面。
4. VirtualPLC 高温注入和停服通信故障两条可重复演示链。

CSV、完整历史查询页面、通信日志页面和文件滚动日志仍属于第四周。

## 2. 采用方案

采用按职责拆分的 Qt Worker + Model/View 方案。报警规则和状态机位于
`domain/`；SQL 迁移、写入和查询位于 `data/`；只持有显示状态和用户命令的
模型/页面位于 `presentation/`；`ApplicationController` 在主线程完成装配。

拒绝两个候选方案：

- 不把 `QSqlDatabase`、报警状态和协议 Worker 放入 `MainWindow`，否则 UI 会成为业务与资源所有者。
- 不提前实现第四周历史页面和 CSV；历史查询 Worker 只交付明确接口和线程测试。

## 3. 文件与职责

### 3.1 报警领域

- `src/monitor/domain/AlarmTypes.h`：规则、报警记录、等级、类别和生命周期值对象。
- `src/monitor/domain/AlarmEngine.h/.cpp`：连续样本计数、越界判定、回差恢复、确认和通信状态转换。
- `DataPipeline`：在完成样本合法性校验后把 Good 样本交给 `AlarmEngine`；Bad/Stale 不触发阈值报警。

`AlarmEngine` 不依赖 SQL、Widgets 或 Modbus。每次激活创建新报警 ID；同一规则活动期间只更新同一记录。

### 3.2 SQLite

- `src/monitor/data/DatabaseMigrator.h/.cpp`：启用 WAL、foreign_keys、busy_timeout，创建并版本化表/索引，从内嵌 `default_config.json` 幂等导入默认单设备、测点和规则。
- `src/monitor/data/StorageWorker.h/.cpp`：在写线程内创建连接；Good 测量按设备/测点/秒保留最新值，1 秒或 50 行事务提交；报警激活、确认、恢复立即进入有界重试缓冲并事务 upsert；连接失败每 2 秒在原线程重试。
- `src/monitor/data/HistoryWorker.h/.cpp`：在查询线程内创建只读连接；校验时间范围不超过 24 小时，按时间分页返回。
- `src/monitor/data/StorageTypes.h`：历史查询请求、结果和跨线程元类型。

Qt 6.2 没有 `QSqlDatabase::moveToThread()`，因此连接不会在主线程预创建后移动；两个 Worker 都在其目标线程收到 `start()` 后才调用 `addDatabase()`，并在同一线程关闭和移除连接。

### 3.3 主界面

- `presentation/models/RealtimeTableModel`：五个测点的当前值、60 秒 min/max/average、质量和时间。
- `presentation/models/DeviceTableModel`：当前设备名称、插件协议、端点、Unit ID、轮询、最近通信时间和状态。
- `presentation/models/AlarmTableModel`：活动/历史过滤、确认与恢复状态显示。
- `presentation/pages/RealtimePage`：连接状态、连接/断开、暂停显示、目标转速异步写入、数据表和最近 60 秒 Qt Charts 曲线。
- `presentation/pages/DevicePage`：协议选项来自已加载插件描述，提供启用状态和有标签的设备配置表单；保存时请求控制器停止旧会话并应用新配置。
- `presentation/pages/AlarmPage`：活动/历史两个标签页和带备注确认命令。
- `presentation/MainWindow`：侧边导航、页面装配和状态栏，不直接持有协议或 SQL 对象。
- `application/ApplicationController`：持有插件管理器和设备会话，管理数据/写入/查询线程，转发值对象到 UI 模型。

质量、严重级别和连接状态始终用文字与颜色共同表达。实时表格是曲线的数据替代视图；表单都有可见标签，按钮保留键盘焦点。

### 3.4 VirtualPLC 演示

`SimulationEngine` 增加可清除的温度覆盖值；`VirtualPlcServer` 暴露高温演示命令；`VirtualPlcWindow` 提供“高温演示 86.3 ℃”切换。关闭服务器继续复用现有停止按钮演示通信报警。

## 4. 数据流与线程关闭顺序

```text
DeviceSession::samplesReady
  -> DataPipeline::processSamples          [data thread]
     -> snapshotsReady                     [UI thread]
     -> alarmChanged                       [UI + storage thread]
     -> validatedSamplesReady
        -> StorageWorker::enqueueSamples   [storage thread]

RealtimePage::writeTargetSpeedRequested
  -> ApplicationController::writeTargetSpeed
  -> DeviceSession::writeValue
  -> ModbusTcpWorker::writeValue            [device thread]
  -> writeFinished                          [UI thread]

DeviceSession::stateChanged
  -> DataPipeline::handleDeviceState       [data thread]
     -> communication alarmChanged
  -> MainWindow::setDeviceState            [UI thread]
```

退出顺序固定为：在可处理 queued 信号的局部事件循环中停止 `DeviceSession`，用阻塞队列栅栏排空数据线程，要求 `StorageWorker` 消费校验后批次并 flush/close 后停止写线程，要求 `HistoryWorker` close 后停止查询线程，最后允许插件加载器销毁。插件显式设置 `PreventUnloadHint`，异常停止超时返回失败且不卸载仍可能执行的代码。

## 5. 错误处理

- 非法报警规则在配置时拒绝并发出领域错误，不进入运行状态。
- 数据库打开、迁移、准备、执行、提交失败均发出结构化存储错误；不阻塞通信或 UI。
- 历史查询明确区分非法范围、SQL 失败和空结果。
- 配置保存只在旧会话停止成功后投递 SQL，并只在 SQL 成功后更新 UI；任一步失败都不重建会话。
- 存储连接恢复和 7 天清理保留在本周 Worker 边界内；不会把失败的测量伪装成已持久化。

## 6. 验证边界

测试按职责放入独立文件：

1. `tst_alarm_engine.cpp`：三次激活、毛刺过滤、回差三次恢复、确认先后顺序、同一活动实例、通信恢复。
2. `tst_database_migrator.cpp`：空库迁移、幂等迁移、表/索引/默认配置和 WAL。
3. `tst_storage_worker.cpp`：写线程连接归属、设备配置、测量降采样、报警 upsert/瞬时写锁重试、数据库路径恢复、停止 flush。
4. `tst_history_worker.cpp`：只读线程、时间校验、排序和分页。
5. `tst_presentation_models.cpp`：模型角色、状态文本和活动/历史过滤。
6. `tst_monitor_window.cpp`：offscreen 创建三个页面、可访问标签、插件协议/启用编辑、目标转速命令和曲线暂停恢复。
7. `tst_device_session.cpp`：最终 queued 样本派发、线程归属、正常停止与超时返回。
8. `tst_week_three_closed_loop.cpp`：真实动态插件、本地 TCP、功能码 06 写入与渐变转速、在线配置重建、高温连续采集、SQLite 测量/报警记录、关闭前数据栅栏与通信恢复。

最终执行配置、完整构建、全量 CTest、关键报警/存储重复测试、offscreen UI 冒烟、`git diff --check`，并回写主设计文档的真实数量与结果。
