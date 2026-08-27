# 第三周报警、SQLite 与主要界面 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在第二周通信与数据流水线之上完成报警、SQLite 读写线程、三个主要 Widgets 页面以及高温/通信故障演示闭环。

**Architecture:** 报警状态机独立于 SQL 和 UI；写入与查询各自在所属线程创建数据库连接；Qt Model/View 只消费跨线程值对象；主线程控制器统一装配并按固定顺序关闭资源。

**Tech Stack:** C++17、Qt 6.2 Core/Widgets/Charts/SQL/Test、SQLite、CMake 3.22、CTest。

## Global Constraints

- 严格停在主设计文档第三周，不实现 CSV、完整历史页和文件日志。
- `QSqlDatabase` 连接必须在使用它的 Worker 线程中创建、使用、关闭和移除。
- Bad/Stale 不参与阈值报警；同一设备同一规则活动期间只存在一个报警实例。
- UI 不持有协议 Worker 或数据库连接，状态不能只依赖颜色表达。
- 保留当前工作区已有未提交改动，不覆盖或丢弃用户内容，不向远程推送。

---

### Task 1: 报警领域状态机

**Files:**
- Create: `src/monitor/domain/AlarmTypes.h`
- Create: `src/monitor/domain/AlarmEngine.h`
- Create: `src/monitor/domain/AlarmEngine.cpp`
- Test: `tests/unit/tst_alarm_engine.cpp`
- Modify: `src/monitor/data/DataPipeline.h`
- Modify: `src/monitor/data/DataPipeline.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SampleBatch`、`DeviceState`。
- Produces: `AlarmEngine::setRules(AlarmRuleList)`、`processSamples(SampleBatch)`、`handleDeviceState(DeviceState)`、`acknowledge(QString, QString, QDateTime)` 和 `alarmChanged(AlarmRecord)`。

- [x] **Step 1: 写报警红灯测试**

覆盖连续三点、单点毛刺、上下限、回差恢复、确认前后恢复、重复活动值不新建 ID、通信 Reconnecting/Online。

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_alarm_engine -j2`

Expected: 因 `AlarmEngine`/`AlarmTypes` 尚不存在而编译失败。

- [x] **Step 3: 实现最小状态机并接入 DataPipeline**

实现规则运行时计数；只有 Good 样本进入阈值判断；通信状态由同一引擎生成独立报警。

- [x] **Step 4: 运行绿灯**

Run: `ctest --test-dir build -R 'unit.(alarm_engine|data_pipeline)' --output-on-failure`

Expected: 两项测试通过。

- [x] **Step 5: 回写进度**

把主设计文档第三周“报警生命周期”标为已完成，记录具体测试名，不提前勾选 SQLite/UI。

### Task 2: SQLite schema 与独立读写 Worker

**Files:**
- Create: `src/monitor/data/StorageTypes.h`
- Create: `src/monitor/data/default_config.json`
- Create: `src/monitor/data/DefaultConfig.qrc`
- Create: `src/monitor/data/DatabaseMigrator.h`
- Create: `src/monitor/data/DatabaseMigrator.cpp`
- Create: `src/monitor/data/StorageWorker.h`
- Create: `src/monitor/data/StorageWorker.cpp`
- Create: `src/monitor/data/HistoryWorker.h`
- Create: `src/monitor/data/HistoryWorker.cpp`
- Test: `tests/unit/tst_database_migrator.cpp`
- Test: `tests/unit/tst_storage_worker.cpp`
- Test: `tests/unit/tst_history_worker.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SampleBatch`、`AlarmRecord`、`DeviceConfig`。
- Produces: `StorageWorker::start/stop/enqueueSamples/enqueueAlarm/saveDevice/flushNow`、`HistoryWorker::start/stop/query`、`HistoryQueryResult`。

- [x] **Step 1: 写迁移与线程红灯测试**

使用 `QTemporaryDir`，验证 WAL、外键、表/索引、内嵌 JSON 默认配置、单设备边界、幂等性、测量降采样、报警 upsert/重试、只读分页和 24 小时限制。

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_database_migrator tst_storage_worker tst_history_worker -j2`

Expected: 因三个组件尚不存在而编译失败。

- [x] **Step 3: 实现迁移和 Worker**

所有 SQL 使用准备语句；写入事务按 1 秒或 50 行 flush；连接失败每 2 秒重试；历史结果按 `timestamp_utc, id` 排序；stop 在所属线程取消重连、flush 并移除连接。

- [x] **Step 4: 运行绿灯与重复测试**

Run: `ctest --test-dir build -R 'unit.(database_migrator|storage_worker|history_worker)' --repeat until-fail:5 --output-on-failure`

Expected: 三项测试各连续五轮通过且无 `removeDatabase` 警告。

- [x] **Step 5: 回写进度**

勾选 SQLite 里程碑，记录 schema、事务和线程测试证据。

### Task 3: Qt Model/View 与三个主要页面

**Files:**
- Create: `src/monitor/presentation/models/RealtimeTableModel.h/.cpp`
- Create: `src/monitor/presentation/models/DeviceTableModel.h/.cpp`
- Create: `src/monitor/presentation/models/AlarmTableModel.h/.cpp`
- Create: `src/monitor/presentation/pages/RealtimePage.h/.cpp`
- Create: `src/monitor/presentation/pages/DevicePage.h/.cpp`
- Create: `src/monitor/presentation/pages/AlarmPage.h/.cpp`
- Create: `src/monitor/presentation/MainWindow.h/.cpp`
- Test: `tests/unit/tst_presentation_models.cpp`
- Test: `tests/unit/tst_monitor_window.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `RealtimeSnapshotBatch`、`DeviceState`、`DeviceConfig`、`ProtocolDescriptor`、`AlarmRecord`、`WriteResult`。
- Produces: 用户的 connect/disconnect/write/save/acknowledge/pause 命令信号。

- [x] **Step 1: 写模型和 offscreen 窗口红灯测试**

验证表头/显示文本、质量与等级不只靠颜色、活动/历史过滤、三个页面存在、有标签表单、插件协议与启用状态、目标转速异步反馈、曲线接收最近 120 点。

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_presentation_models tst_monitor_window -j2`

Expected: 因模型和窗口尚不存在而编译失败。

- [x] **Step 3: 实现最小专业桌面界面**

使用左侧文字导航、稳定状态栏、实时表格 + QLineSeries、目标转速写入、设备表单、活动/历史报警表格；暂停只影响模型重绘，数据输入继续保留。

- [x] **Step 4: 运行绿灯**

Run: `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R 'unit.(presentation_models|monitor_window)' --output-on-failure`

Expected: 两项测试通过。

- [x] **Step 5: 回写进度**

勾选三个主要页面，记录 UI 冒烟测试；不把未实现历史/日志页写成完成。

### Task 4: 应用控制器、共享中文字体与第三周闭环

**Files:**
- Create: `src/monitor/application/ApplicationController.h/.cpp`
- Create: `src/ui_support/CMakeLists.txt`
- Create: `src/ui_support/UiFont.h/.cpp`
- Create: `src/ui_support/ChineseFonts.qrc`
- Modify: `src/virtual_plc/CMakeLists.txt`
- Modify: `src/virtual_plc/main.cpp`
- Modify: `src/monitor/main.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/integration/tst_week_three_closed_loop.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: 前三项组件和插件目录。
- Produces: GUI 启动、固定关闭顺序、测量/报警持久化与历史查询装配。

- [x] **Step 1: 写控制器/闭环红灯测试**

真实链路验证插件扫描、持续采集、功能码 06 写入、在线配置重建、DataPipeline 校验后存储/报警、SQLite 写入和停止后线程退出。

- [x] **Step 2: 运行红灯**

Run: `cmake --build build --target tst_week_three_closed_loop -j2`

Expected: 因控制器未装配而编译失败。

- [x] **Step 3: 实现控制器与 GUI 入口**

`QApplication` 创建后加载共享 CJK 字体；控制器在主线程建立 queued connections；数据库路径使用 `QStandardPaths::AppDataLocation`；退出时先在局部事件循环停止 Session，再以数据线程栅栏、Storage flush、History close 的顺序执行。

- [x] **Step 4: 运行闭环绿灯**

Run: `ctest --test-dir build -R integration.week_three_closed_loop --output-on-failure`

Expected: 高温报警和 SQLite 闭环通过。

- [x] **Step 5: 回写进度**

记录应用真实调用链和线程关闭证据。

### Task 5: VirtualPLC 高温/通信故障演示与最终验收

**Files:**
- Modify: `src/virtual_plc/SimulationEngine.h/.cpp`
- Modify: `src/virtual_plc/VirtualPlcServer.h/.cpp`
- Modify: `src/virtual_plc/VirtualPlcWindow.h/.cpp`
- Modify: `tests/unit/tst_simulation_engine.cpp`
- Modify: `tests/integration/tst_virtual_plc_server.cpp`
- Modify: `README.md`
- Modify: `文档/工业监控系统设计.md`

**Interfaces:**
- Produces: `SimulationEngine::setTemperatureOverride/clearTemperatureOverride`、`VirtualPlcServer::setHighTemperatureEnabled(bool)`。

- [x] **Step 1: 写高温注入红灯测试**

验证覆盖启用后温度稳定为 863，清除后恢复确定性波形；Server 运行时同步寄存器。

- [x] **Step 2: 运行红灯并实现最小演示控制**

Run: `cmake --build build --target tst_simulation_engine tst_virtual_plc_server -j2`

Expected: 新 API 首次编译失败；实现后两项通过。

- [x] **Step 3: 更新 UI 与文档**

VirtualPLC 增加带文字状态的高温切换；README 写第三周启动、报警确认、停服/恢复和数据库位置，不包含第四周功能。

- [x] **Step 4: 完整验证**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j2`

Run: `ctest --test-dir build --output-on-failure`

Run: `ctest --test-dir build -R 'unit\.(data_pipeline|alarm_engine|database_migrator|storage_worker|history_worker)$' --repeat until-fail:10 --output-on-failure`

Run: `ctest --test-dir build -R '^unit.device_session$' --repeat until-fail:100 --output-on-failure`

Run: `ctest --test-dir build -R '^integration.week_three_closed_loop$' --repeat until-fail:10 --output-on-failure`

Run: `git diff --check`

Expected: 配置/构建成功、全部测试通过，数据/报警/SQLite 与真实闭环各十轮通过，会话生命周期一百轮通过，差异检查无输出。

- [x] **Step 5: 最终文档对账**

逐条核对主设计文档第三周四项和结束标准；写入实际文件、测试数量、命令结果、socket/offscreen 环境边界和仍留给第四周的功能。
