# QML 并行界面设计

## 1. 文档信息

| 项目 | 内容 |
| --- | --- |
| 项目名称 | Industrial Monitoring System |
| 设计日期 | 2026-08-27 |
| 设计目标 | 在保留现有 Qt Widgets 界面的同时，新增一套 QML 界面 |
| 新可执行程序 | `industrial_monitor_qml` |
| Qt 基线 | Qt 6.2.4 |
| 设计状态 | 已实施并验证（2026-08-27） |

本文档定义 QML 并行界面的架构边界、文件职责、运行时调用链、错误处理、依赖和验收标准。QML 界面是现有 Widgets 界面的并行前端，不是对通信、数据、报警或存储链路的重写。

## 2. 背景与目标

当前工程已经把主要职责拆为：

- `monitor_application`：设备会话、数据流水线、报警和 SQLite 相关业务能力。
- `monitor_runtime`：`ApplicationController` 运行时装配与 UI 命令入口。
- `monitor_presentation`：现有 Qt Widgets 模型、页面和主窗口。
- `industrial_monitor`：现有 Widgets 程序入口。

现有 `ApplicationController` 在 UI 主线程接收连接、断开、写目标转速、保存配置和确认报警等命令，并通过信号输出协议、设备配置、连接状态、实时快照、报警、写入结果和存储状态。因此，新增 QML 界面不需要复制或改写后端链路。

本次设计目标为：

1. 保留现有 `industrial_monitor` 和 Widgets 界面，确保原有行为与测试不受影响。
2. 新增 `industrial_monitor_qml`，与现有程序共享 `monitor_application` 和 `monitor_runtime`。
3. QML 只负责布局、交互和展示；不直接接触协议 Worker、`QModbusTcpClient`、`QThread` 或 `QSqlDatabase`。
4. 通过 QML 专用 Facade 和命名角色模型完成 C++ 值对象到 QML 基础类型的转换。
5. 保持现有插件目录、数据库参数、线程模型和退出顺序。

## 3. 范围边界

### 3.1 本次包含

- 新增独立的 QML 可执行程序和 QML 模块。
- 新增一个 QML 应用 Facade，集中暴露属性、模型和用户命令。
- 新增实时数据、趋势、设备状态、协议选项和报警等 QML 专用展示模型。
- 实现实时监控、设备管理、报警中心三个页面。
- 保留连接、断开、目标转速写入、设备配置保存和报警确认操作。
- 保留中文字体、状态文字与颜色双重表达、输入校验和错误提示。
- 为 Facade、模型、QML 加载和后端闭环增加独立测试文件。

### 3.2 本次不包含

- 不删除、替换或重写现有 Widgets 界面。
- 不修改 Modbus TCP 插件、通信线程、数据线程、报警状态机和 SQLite Worker 的业务行为。
- 不为了 QML 改造协议 SDK 中的值对象，也不把协议类型直接暴露给 JavaScript。
- 不引入新的设备、协议、数据库表或业务功能。
- 不同时运行 Widgets 和 QML 两个程序去控制同一设备会话；两者是互斥的可选前端。
- 不在第一版中引入复杂动画、3D、主题编辑器或额外 UI 框架。

### 3.3 “不改旧代码”的精确定义

后续实现应遵守以下边界：

- 不修改现有通信、数据、领域、存储和 Widgets 展示层的 `.cpp/.h` 文件。
- 不修改现有 `industrial_monitor` 的入口和链接关系。
- 允许对顶层 CMake、安装清单和测试清单做最小增量修改，使新目标可以被构建、安装和测试。
- 所有 QML 运行时代码放入新目录；如果实现过程中发现必须修改旧业务接口，应暂停并重新评审设计，而不是直接扩展旧类职责。

完全不修改任何既有 CMake 文件也能通过独立子工程实现，但会重复工程装配和依赖发现，容易造成两套构建配置漂移，本设计不采用该方式。

## 4. 方案比较与选择

### 4.1 方案 A：并行目标 + QML Facade + QML 专用模型

新增 QML 可执行目标，只链接现有业务运行库，不链接 `monitor_presentation`。Facade 在 C++ 层接收 `ApplicationController` 的值对象信号，将其转换为 QML 可直接消费的属性和命名角色模型。

优点：

- 旧业务和 Widgets 源码零侵入。
- QML 不需要理解自定义 C++ 结构体和跨线程细节。
- 两套界面可以独立测试和演进。
- 文件职责清晰，符合现有分层。

代价：

- 需要维护少量 QML 专用展示状态和格式化代码。
- Widgets 与 QML 的展示模型不会共用同一个类。

### 4.2 方案 B：QML 直接使用现有 Controller 和 Widgets 表格模型

把 `ApplicationController` 和现有 `QAbstractTableModel` 直接注册给 QML。

优点是新增 C++ 文件较少，但现有模型主要面向 Widgets 的列和 `Qt::DisplayRole`，缺少适合 QML delegate 的命名角色；`DeviceConfig`、`AlarmRecord` 等自定义值对象也不适合由 QML JavaScript 直接构造。该方案会让 QML 页面充满列号、类型转换和展示格式判断，因此不采用。

### 4.3 方案 C：重构出 Widgets/QML 共用的通用展示模型

修改现有 presentation 模型，使其同时服务 Widgets 和 QML。

该方案长期看可减少展示逻辑重复，但会扩大修改面并增加现有 Widgets 回归风险，不符合“不改旧代码”的当前约束，因此暂不采用。

### 4.4 最终选择

采用方案 A。允许重复少量纯展示映射，换取旧界面稳定、类型边界明确和两套前端独立演进。

## 5. 总体架构

```text
                     +---------------------------+
                     | monitor_application       |
                     | 通信/数据/报警/SQLite      |
                     +-------------+-------------+
                                   |
                     +-------------v-------------+
                     | monitor_runtime           |
                     | ApplicationController     |
                     +-------------+-------------+
                                   |
                  +----------------+----------------+
                  |                                 |
       +----------v-----------+          +----------v-----------+
       | monitor_presentation |          | monitor_qml_ui        |
       | Widgets 模型与页面    |          | Facade/模型/QML 页面   |
       +----------+-----------+          +----------+-----------+
                  |                                 |
       +----------v-----------+          +----------v-----------+
       | industrial_monitor   |          | industrial_monitor_qml|
       +----------------------+          +----------------------+
```

两个可执行程序共用以下后端事实：

- 动态协议插件和 `PluginManager`。
- `DeviceSession` 与通信线程。
- `DataPipeline` 与数据线程。
- `AlarmEngine`。
- `StorageWorker` 和 `HistoryWorker`。
- `ApplicationController` 对外命令与状态信号。

QML 分支不会链接 `monitor_presentation`，也不会创建 `MainWindow` 或 QWidget 页面。

## 6. 新增目录与文件职责

```text
src/monitor_qml/
├── CMakeLists.txt
├── main.cpp
├── bridge/
│   ├── QmlApplicationFacade.h
│   └── QmlApplicationFacade.cpp
├── models/
│   ├── QmlRealtimeModel.h
│   ├── QmlRealtimeModel.cpp
│   ├── QmlTrendModel.h
│   ├── QmlTrendModel.cpp
│   ├── QmlDeviceViewModel.h
│   ├── QmlDeviceViewModel.cpp
│   ├── QmlProtocolModel.h
│   ├── QmlProtocolModel.cpp
│   ├── QmlAlarmModel.h
│   └── QmlAlarmModel.cpp
└── qml/
    ├── Main.qml
    ├── Theme.qml
    ├── pages/
    │   ├── RealtimePage.qml
    │   ├── DevicePage.qml
    │   └── AlarmPage.qml
    └── components/
        ├── StatusBadge.qml
        ├── MetricCard.qml
        ├── LabeledField.qml
        └── SectionPanel.qml
```

### 6.1 `main.cpp`

只负责进程级装配：

1. 创建 `QApplication`。
2. 设置与现有程序兼容的组织名、应用名和版本。
3. 复用 `industrial_ui_support` 加载内嵌中文字体。
4. 解析与现有程序一致的 `--plugin-dir` 和 `--database` 参数。
5. 创建 `ApplicationController`、`QmlApplicationFacade` 和 `QQmlApplicationEngine`。
6. 在加载 QML 前，把 Facade 作为唯一顶层 C++ 对象提供给 QML。
7. QML 根对象创建成功后启动 Controller。
8. 在 `aboutToQuit` 中调用 `ApplicationController::shutdown()`。

第一版继续使用 `QApplication` 而不是 `QGuiApplication`，目的是直接复用现有中文字体支持。虽然可执行程序会因此保留 Qt Widgets 链接依赖，但全部可见界面仍由 QML/Qt Quick 实现。后续只有在允许调整共享字体模块时，才考虑移除该依赖。

### 6.2 `QmlApplicationFacade`

Facade 是 QML 与现有 Controller 之间唯一的 C++ 边界，位于 UI 主线程，不拥有任何通信或数据库 Worker。

职责：

- 连接 `ApplicationController` 的公开信号。
- 持有并暴露实时数据、趋势、设备、协议和报警展示模型。
- 把存储状态、致命错误和写入结果转换为 QML 属性与通知信号。
- 提供 QML 可调用的简单命令，例如连接、断开、写目标转速、保存设备和确认报警。
- 对 QML 输入做基础格式校验，再组装现有 `DeviceConfig` 值对象交给 Controller；Controller 仍保留最终业务校验权。

Facade 不负责：

- 插件扫描和协议选择逻辑。
- 设备线程或数据库线程生命周期。
- 报警判断、统计计算和 SQL 操作。
- 在 JavaScript 中保存第二份业务状态。

建议暴露的只读属性包括：

- `realtimeModel`
- `trendModel`
- `deviceModel`
- `protocolModel`
- `activeAlarmModel`
- `alarmHistoryModel`
- `statusMessage`
- `statusHealthy`
- `initialized`
- `commandBusy`
- `displayPaused`

建议提供的 QML 命令包括：

- `connectDevice()`
- `disconnectDevice()`
- `writeTargetSpeed(int targetSpeed)`
- `saveDevice(string protocolKey, string host, int port, int unitId, int pollIntervalMs, int timeoutMs, bool enabled)`
- `acknowledgeAlarm(string alarmId, string note)`
- `selectTrendTag(string tagId)`
- `setDisplayPaused(bool paused)`

保存设备时，Facade 复制当前 `DeviceConfig`，保留设备 ID 等不可编辑身份字段，只用经过校验的页面输入覆盖协议、端点、Unit ID、轮询、超时和启用状态。QML 不直接构造 `DeviceConfig`、`WriteRequest` 或 `AlarmRecord`。

### 6.3 QML 专用模型

#### `QmlRealtimeModel`

使用 `QAbstractListModel` 表达五个测点，每行提供稳定命名角色：

- `tagId`
- `displayName`
- `unit`
- `currentValue`
- `minimumValue`
- `maximumValue`
- `averageValue`
- `quality`
- `qualityText`
- `timestampText`

模型接收完整 `RealtimeSnapshotBatch`，按 `tagId` 更新对应行并发出最小范围的 `dataChanged()`。数值在模型中保持数值类型，QML 只做最终展示格式，不把数值提前变成不可计算的字符串。

#### `QmlTrendModel`

保存温度、压力、转速和电压最近 60 秒、最多 120 点的显示历史，并以时间戳和值两列向 Qt Charts 的 XY Model Mapper 提供数据。模型只保存 UI 趋势缓存，不参与统计、报警或持久化。

切换测点时重置当前映射视图；暂停显示时继续接收并裁剪后台数据，但不刷新图表，恢复后一次性提交最新 60 秒窗口。这与现有 Widgets 页面“暂停显示不停止采集”的语义一致。

#### `QmlDeviceViewModel`

首版只有一个配置设备，因此使用 `QObject + Q_PROPERTY`，不强行套用列表模型。属性包括设备 ID、协议名称、主机、端口、Unit ID、轮询间隔、超时、启用状态、连接状态、状态说明和最近通信时间。

编辑表单使用独立的 QML 临时输入值；保存成功后再以 Controller 发出的 `deviceConfigChanged` 更新事实状态。保存失败不能提前修改 ViewModel。

#### `QmlProtocolModel`

使用 `QAbstractListModel` 保存 Controller 发出的插件描述，至少提供 `protocolKey` 和 `displayName` 两个角色。设备页面以 `protocolKey` 作为保存值，以 `displayName` 作为可见文案，不在 QML 中硬编码协议名称。

#### `QmlAlarmModel`

使用 `QAbstractListModel` 保存报警记录，并分别通过活动模型和历史模型表达过滤结果。命名角色包括：

- `alarmId`
- `activatedAtText`
- `deviceId`
- `tagText`
- `message`
- `triggerValue`
- `severity`
- `severityText`
- `state`
- `stateText`
- `active`
- `acknowledgeable`

确认操作通过 `alarmId` 发送给 Facade，QML 不依赖行号作为业务标识。

## 7. QML 页面边界

### 7.1 `Main.qml`

负责应用窗口、侧边导航、全局状态条和页面容器。窗口建议默认尺寸为 1280×800，并设置可用的最小尺寸；缩小时允许内容滚动，不通过压缩导致文字或表单控件不可用。

`Main.qml` 不包含具体业务表格和表单实现，避免形成超大 QML 文件。

### 7.2 `RealtimePage.qml`

包含：

- 当前连接状态和连接/断开操作。
- 五个测点的当前值与质量摘要。
- 最近 60 秒趋势图、测点选择和暂停/恢复显示。
- 最小值、最大值、平均值和最近更新时间。
- 目标转速输入和异步写入结果。

暂停只停止图形刷新，不停止采集、报警判断和数据入库。

### 7.3 `DevicePage.qml`

包含：

- 当前设备与协议摘要。
- 主机、端口、Unit ID、轮询间隔、超时和启用状态表单。
- 保存前的本地输入检查和明确错误文案。
- 保存中状态，避免重复提交。

协议选项来自 Controller 已加载的插件描述，不在 QML 中硬编码协议名称。

### 7.4 `AlarmPage.qml`

包含活动报警和报警历史两个视图，展示激活时间、设备、测点/通信类别、消息、触发值、等级和状态。确认对话区域要求输入可选备注，并只对可确认报警启用操作。

### 7.5 通用组件

- `StatusBadge.qml`：连接、质量、报警和存储状态的文字与颜色双重表达。
- `MetricCard.qml`：实时值、单位、统计和更新时间。
- `LabeledField.qml`：保证每个输入控件都有可见标签、错误信息和键盘焦点。
- `SectionPanel.qml`：统一页面分组、间距和标题层级。
- `Theme.qml`：集中保存颜色、字号、间距和圆角等视觉常量，不保存业务状态。

## 8. 运行时调用链

### 8.1 启动链

```text
QApplication
-> configureChineseUiFont
-> 解析插件目录与数据库路径
-> ApplicationController
-> QmlApplicationFacade
-> QQmlApplicationEngine 加载 Main.qml
-> QML 根对象创建成功
-> ApplicationController::start
-> 插件扫描、数据/存储/历史线程启动
-> Facade 和模型接收初始化状态
```

如果 QML 根对象创建失败，不启动 Controller，并以非零状态退出。

### 8.2 实时数据链

```text
VirtualPLC
-> ModbusTcpWorker                         [通信线程]
-> DeviceSession::samplesReady             [主线程转发]
-> DataPipeline::processSamples            [数据线程]
-> ApplicationController::snapshotsReady   [UI 主线程]
-> QmlApplicationFacade
-> QmlRealtimeModel
-> RealtimePage.qml
```

QML 页面不会接收到 Worker、Reply、Timer 或数据库连接指针。

### 8.3 用户命令链

```text
QML Button/TextField
-> QmlApplicationFacade 的简单类型命令
-> 基础输入校验、保留身份字段并组装 C++ 值对象
-> ApplicationController
-> DeviceSession / StorageWorker / DataPipeline
-> 结果信号返回 Facade
-> 属性或模型更新
-> QML 界面刷新
```

### 8.4 线程归属

| 对象 | 所属线程 | 职责 |
| --- | --- | --- |
| `QQmlApplicationEngine`、QML 对象 | UI 主线程 | 页面、交互、绑定 |
| `QmlApplicationFacade`、QML 模型 | UI 主线程 | 类型转换、展示状态、命令入口 |
| `ApplicationController`、`DeviceSession` | UI 主线程 | 运行时编排、线程管理 |
| `ModbusTcpWorker`、Client、Timer | 通信线程 | Modbus 连接、轮询、写入、重连 |
| `DataPipeline`、`AlarmEngine` | 数据线程 | 校验、统计、报警 |
| `StorageWorker` | SQLite 写线程 | 持久化 |
| `HistoryWorker` | SQLite 查询线程 | 只读查询 |

新增 QML 层不改变任何现有跨线程连接。跨线程仍只传递已注册的值对象；Facade 只在信号回到 UI 主线程后更新模型。

## 9. 生命周期与退出顺序

栈对象声明顺序应保证 QML Engine 先于 Facade 和 Controller 析构。退出时固定执行：

```text
QCoreApplication::aboutToQuit
-> ApplicationController::shutdown
-> 停止 DeviceSession
-> 排空数据线程
-> StorageWorker flush/close
-> HistoryWorker close
-> 线程退出
-> QML Engine 销毁
-> Facade 和 Controller 析构
-> 插件加载器最后释放
```

Facade 不自行停止线程，也不在析构中重复管理后端资源。`shutdown()` 失败时记录明确错误，但仍保持插件生命周期长于可能尚未结束的 Worker。

## 10. 错误处理与输入校验

- QML 加载失败：输出 QML 错误并以非零状态退出，不启动后端线程。
- 插件或数据库初始化失败：通过 Facade 显示全局错误条，禁用依赖初始化完成的命令。
- 主机为空、端口越界、Unit ID 不在 1–247、轮询或超时非法：QML 表单先提示；Facade 再校验；Controller 保留最终校验。
- 目标转速超出 `quint16` 范围：Facade 拒绝请求并显示错误，不发生截断转换。
- 连接、断开、保存和写入进行中：设置 `commandBusy`，防止同类命令重复提交。
- 通信错误：继续复用 Controller 状态和重连机制，QML 只展示状态，不实现第二套重连计时器。
- 数据质量：Good、Stale、Bad 同时显示文字和颜色，不能只靠颜色区分。
- 保存设备配置：只有收到 Controller 的成功配置回传后才更新事实状态；失败时保留用户输入以便修改重试。
- 报警确认：使用稳定 `alarmId`，确认失败不能在 QML 中提前改变报警状态。

## 11. 构建与依赖设计

### 11.1 CMake 增量

后续实现需要进行以下最小构建调整：

1. 查找 `Qt6::Qml`、`Qt6::Quick` 和 `Qt6::QuickControls2`。
2. 在顶层增加 `add_subdirectory(src/monitor_qml)`。
3. 使用 `qt_add_qml_module` 将 QML 文件编译进资源，避免运行时依赖源码目录。
4. 新目标链接 `monitor_runtime`、`monitor_application`、`industrial_ui_support`、`Qt6::Qml`、`Qt6::Quick`、`Qt6::QuickControls2` 和已有 `Qt6::Charts`。
5. 不链接 `monitor_presentation`。
6. 在安装清单中把 `industrial_monitor_qml` 放到与现有程序相同的可执行目录，使其复用同一插件安装布局。

### 11.2 当前环境依赖

开发和运行 QML 前端需要 Qt Declarative 开发包及 Quick/Controls/Charts 运行时 import：

```text
qt6-declarative-dev
qml6-module-qtqml-workerscript
qml6-module-qtquick
qml6-module-qtquick-controls
qml6-module-qtquick-layouts
qml6-module-qtquick-templates
qml6-module-qtquick-window
qml6-module-qtcharts
```

2026-08-27 环境核对：`qml6-module-qtcharts` 和 `libqt6chartsqml6` 已安装；系统仍缺 `qml6-module-qtqml-workerscript` 与 `qml6-module-qtquick-templates`。因此直接在当前系统环境运行 QML 冒烟测试会精确失败于 `module "QtQml.WorkerScript" is not installed`；使用已下载并解包的同版本临时运行模块后，真实 QML Engine 加载测试通过。这是系统包安装边界，不是资源 URL、QML 绑定或 Qt Charts 代码错误。

## 12. 测试设计

测试代码按职责拆到不同文件，避免单个测试不断膨胀。

### 12.1 C++ 单元测试

- `tst_qml_application_facade.cpp`：命令参数转换、忙状态、错误状态、Controller 信号转发。
- `tst_qml_realtime_model.cpp`：角色名称、按 `tagId` 更新、数值类型、质量和时间显示。
- `tst_qml_trend_model.cpp`：60 秒/120 点裁剪、测点切换、暂停继续缓存和恢复刷新。
- `tst_qml_device_view_model.cpp`：配置、连接状态和保存成功/失败边界。
- `tst_qml_protocol_model.cpp`：插件描述去重、key/displayName 角色和选择值。
- `tst_qml_alarm_model.cpp`：新增、更新、活动/历史过滤、稳定 ID 和可确认状态。

### 12.2 QML 冒烟测试

- 使用 offscreen 平台加载 QML 模块。
- 验证 `Main.qml` 和三个页面能够创建。
- 验证 Facade 和三个模型绑定存在。
- 验证中文标题、导航、状态条、主要输入和按钮可访问。
- 通过稳定 `objectName` 验证主要交互入口，不依赖像素截图作为功能断言。

### 12.3 集成测试

- 启动 VirtualPLC 和真实动态 Modbus 插件。
- 通过现有 Controller 产生实时快照，验证 QML 实时模型收到更新。
- 执行连接、断开、目标转速写入、设备保存和报警确认，验证命令经过 Facade 到达现有后端。
- 注入高温和通信中断，验证报警模型和状态属性更新。
- 验证退出时通信、数据、写入和查询线程按原顺序停止。

### 12.4 回归验证

最终验证至少包括：

1. 配置并完整构建 `industrial_monitor` 与 `industrial_monitor_qml`。
2. 执行全量 CTest，确保现有测试无回归。
3. 执行 QML 静态检查和 QML offscreen 冒烟测试。
4. 分别启动 Widgets 和 QML 程序完成主要页面演示，但不同时连接同一设备。
5. 运行真实 `VirtualPLC -> Modbus TCP -> Controller -> QML 模型 -> QML 页面` 闭环。
6. 执行 `git diff --check`，确认没有冲突标记、尾随空白或构建产物混入源码。

## 13. 验收标准

满足以下条件才认为 QML 并行界面完成：

- 原 `industrial_monitor` 可继续构建、运行，现有测试全部通过。
- 新 `industrial_monitor_qml` 可从构建目录和安装目录启动。
- 两个前端共享同一个 `ApplicationController` 和后端库，没有复制通信、报警或 SQL 实现。
- QML 能展示五个测点、统计值、数据质量、连接状态、设备配置和报警生命周期。
- 连接、断开、写目标转速、保存配置和确认报警均走现有真实调用链。
- QML 页面不持有协议对象、Worker、Timer 或数据库连接。
- 自定义 C++ DTO 不由 QML JavaScript 直接构造；Facade 和模型边界清晰。
- 中文无方框字，状态不只依赖颜色，表单有可见标签和错误信息。
- QML 加载失败、后端初始化失败、非法输入和异步命令失败都有明确反馈。
- 真实闭环、offscreen QML 冒烟和全量回归验证通过，测试结果如实记录。

## 14. 实际实施与验收记录

实际代码与本文档的架构边界一致：

1. `monitor_qml_ui` 静态边界包含 Facade、五个 QML 专用模型和嵌入式 QML 模块；`industrial_monitor_qml` 只做进程级装配。
2. `Main.qml` 与三个独立页面已实现实时值/趋势、设备配置、报警生命周期和五个真实命令入口。
3. Facade 对异步命令使用明确的待完成类型，无关报警/写入/存储回调不会提前清除 `commandBusy`；停用设备和非 `Stopped` 状态会拒绝重复连接。
4. 趋势暂停仅冻结当前可见点集，后台继续收集和裁剪缓存；恢复时一次 reset 到最新 60 秒窗口。
5. 安装清单已包含 Widgets/QML 两个入口、VirtualPLC、协议共享库、动态插件、文档与字体许可证。

2026-08-27 最终验证：

```text
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
-> 成功（仅保留已知 Qt SerialBus 开发包不完整告警）

cmake --build build -j2
-> 成功，Widgets/QML 两个可执行目标均已构建

使用同版本临时 QML 运行模块执行 22 项非监听测试
-> 22/22 通过，包括 QML 模型、Facade、offscreen 实引擎加载与中文字形

在允许本地环回监听的环境执行 5 项集成测试
-> 5/5 通过，其中 integration.qml_backend_closed_loop 用时 5.47 s

cmake --install build --prefix /tmp/industrial-monitor-qml-install-20260827
-> Widgets/QML/VirtualPLC/插件/共享库/文档安装成功
```

`integration.qml_backend_closed_loop` 已真实覆盖保存设备、连接、实时/趋势投影、写入 1800 rpm、高温报警确认/恢复、通信中断/恢复、断开与线程停止。当前唯一未满足的本机启动前置条件是系统层尚未安装上述两个 QML 运行包。
