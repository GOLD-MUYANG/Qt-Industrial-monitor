# 工业监控系统第一周实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立可由 CTest 验证的 Qt 6 多目标工程，通过动态 Modbus TCP 插件从 VirtualPLC 读取 5 个 Holding Registers，并验证目标转速写入。

**Architecture:** `protocol_sdk` 只定义插件 ABI 和跨模块值类型；`modbus_tcp_plugin` 封装 Qt Serial Bus 客户端与寄存器编解码；`virtual_plc_core` 封装寄存器、确定性模拟和 TCP Server；`PluginManager` 负责发现、验证并持有加载器。单元测试分别验证边界，集成测试通过真实环回 TCP 串起最终验收链路。

**Tech Stack:** C++17、CMake 3.22、Qt 6.2.4 Core/Widgets/Network/SerialBus/Test、CTest

## Global Constraints

- 插件 IID 固定为 `com.muyang.IndustrialMonitor.ProtocolPlugin/1.0`，API 版本固定为 `1`。
- 开发端点默认 `127.0.0.1:1502`，Unit ID 为 `1`。
- Holding Registers 从 Qt 地址 `0` 开始连续读取 `5` 个；目标转速写到 Qt 地址 `10`。
- 第一周不实现第二周的周期轮询、重连状态机、数据流水线和线程编排。
- 每个生产文件只承担一个明确职责，测试按模块拆分。

---

### Task 1: 多目标构建骨架与协议 SDK

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/protocol_sdk/CMakeLists.txt`
- Create: `src/protocol_sdk/include/industrial/protocol/ProtocolSdkExport.h`
- Create: `src/protocol_sdk/include/industrial/protocol/ProtocolVersion.h`
- Create: `src/protocol_sdk/include/industrial/protocol/ProtocolTypes.h`
- Create: `src/protocol_sdk/include/industrial/protocol/AbstractDeviceWorker.h`
- Create: `src/protocol_sdk/include/industrial/protocol/IProtocolPlugin.h`
- Create: `src/protocol_sdk/ProtocolTypes.cpp`
- Test: `tests/unit/tst_protocol_types.cpp`

**Interfaces:**
- Produces: `registerProtocolMetaTypes()`、`DeviceConfig`、`SampleBatch`、`WriteRequest`、`IProtocolPlugin::createWorker()`。

- [ ] **Step 1: 写协议元类型失败测试**

```cpp
void ProtocolTypesTest::registersQueuedConnectionTypes()
{
    registerProtocolMetaTypes();
    QVERIFY(QMetaType::fromName("industrial::protocol::SampleBatch").isValid());
}
```

- [ ] **Step 2: 配置并运行测试，确认因 SDK 尚未实现而失败**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build --target tst_protocol_types`

Expected: 编译失败，指出 `registerProtocolMetaTypes` 或协议头不存在。

- [ ] **Step 3: 实现最小 SDK 和 CMake 目标**

```cpp
class IProtocolPlugin
{
public:
    virtual ~IProtocolPlugin() = default;
    virtual ProtocolDescriptor descriptor() const = 0;
    virtual AbstractDeviceWorker *createWorker(const DeviceConfig &config) = 0;
};
```

- [ ] **Step 4: 运行协议测试**

Run: `ctest --test-dir build -R protocol_types --output-on-failure`

Expected: `100% tests passed`。

### Task 2: VirtualPLC 寄存器与确定性模拟

**Files:**
- Create: `src/virtual_plc/RegisterBank.h`
- Create: `src/virtual_plc/RegisterBank.cpp`
- Create: `src/virtual_plc/SimulationEngine.h`
- Create: `src/virtual_plc/SimulationEngine.cpp`
- Test: `tests/unit/tst_register_bank.cpp`
- Test: `tests/unit/tst_simulation_engine.cpp`

**Interfaces:**
- Produces: `RegisterBank::holdingRegisters()`、`RegisterBank::setTargetSpeed(quint16)`、`SimulationEngine::advance(RegisterBank&)`。

- [ ] **Step 1: 写寄存器映射失败测试**

```cpp
QCOMPARE(bank.value(RegisterBank::Temperature), quint16(420));
QCOMPARE(bank.value(RegisterBank::Pressure), quint16(120));
QCOMPARE(bank.value(RegisterBank::TargetSpeed), quint16(1500));
```

- [ ] **Step 2: 运行并确认缺少 `RegisterBank` 而失败**

Run: `cmake --build build --target tst_register_bank`

Expected: 编译失败，指出 `RegisterBank` 未定义。

- [ ] **Step 3: 实现地址常量、默认值和边界检查**

```cpp
enum Address : quint16 { Temperature = 0, Pressure = 1, Speed = 2,
                         Voltage = 3, Status = 4, TargetSpeed = 10 };
```

- [ ] **Step 4: 写并验证模拟失败测试，再实现确定性推进**

Run: `ctest --test-dir build -R "register_bank|simulation_engine" --output-on-failure`

Expected: 两个测试通过，实际转速单步接近目标且温度、压力、电压保持设计范围。

### Task 3: VirtualPLC TCP Server 与最小窗口

**Files:**
- Create: `src/virtual_plc/VirtualPlcServer.h`
- Create: `src/virtual_plc/VirtualPlcServer.cpp`
- Create: `src/virtual_plc/VirtualPlcWindow.h`
- Create: `src/virtual_plc/VirtualPlcWindow.cpp`
- Create: `src/virtual_plc/main.cpp`
- Test: `tests/integration/tst_virtual_plc_server.cpp`

**Interfaces:**
- Consumes: `RegisterBank`、`SimulationEngine`。
- Produces: `VirtualPlcServer::start(QHostAddress, quint16, int)`、`stop()`、`setTargetSpeed()`。

- [ ] **Step 1: 写 TCP Server 启停和数据映射失败测试**

```cpp
QVERIFY(server.start(QHostAddress::LocalHost, testPort, 1));
QVERIFY(server.isListening());
QCOMPARE(server.registerBank().value(RegisterBank::Temperature), quint16(420));
```

- [ ] **Step 2: 运行并确认缺少 Server 而失败**

Run: `cmake --build build --target tst_virtual_plc_server`

Expected: 编译失败，指出 `VirtualPlcServer` 未定义。

- [ ] **Step 3: 用 `QModbusTcpServer::setMap`、NetworkAddressParameter 和 NetworkPortParameter 实现最小服务器**

- [ ] **Step 4: 增加只负责启动、停止和展示寄存器值的窗口**

- [ ] **Step 5: 运行 Server 测试**

Run: `ctest --test-dir build -R virtual_plc_server --output-on-failure`

Expected: 测试通过且端口关闭后可再次启动。

### Task 4: 插件发现、元数据验证与生命周期

**Files:**
- Create: `src/monitor/application/PluginManager.h`
- Create: `src/monitor/application/PluginManager.cpp`
- Create: `tests/fixtures/invalid_api_plugin/InvalidApiPlugin.h`
- Create: `tests/fixtures/invalid_api_plugin/invalid_api_plugin.json`
- Test: `tests/unit/tst_plugin_manager.cpp`

**Interfaces:**
- Produces: `PluginManager::scan(const QString&)`、`plugins()`、`errors()`、`plugin(QStringView)`。

- [ ] **Step 1: 写空目录和错误 API 版本失败测试**

```cpp
manager.scan(emptyDirectory.path());
QVERIFY(manager.plugins().isEmpty());
QVERIFY(manager.errors().isEmpty());

manager.scan(invalidPluginDirectory);
QVERIFY(manager.plugins().isEmpty());
QVERIFY(manager.errors().constFirst().message.contains("apiVersion"));
```

- [ ] **Step 2: 运行并确认缺少 `PluginManager` 而失败**

Run: `cmake --build build --target tst_plugin_manager`

Expected: 编译失败，指出 `PluginManager` 未定义。

- [ ] **Step 3: 实现目录扫描、IID/API/key/qobject_cast 验证，并让 `QPluginLoader` 由 Manager 持有**

- [ ] **Step 4: 运行加载失败测试**

Run: `ctest --test-dir build -R plugin_manager --output-on-failure`

Expected: 空目录不报错，错误 API 插件产生结构化错误且不进入插件集合。

### Task 5: Modbus 插件的编解码与异步读写

**Files:**
- Create: `src/plugins/modbus_tcp/ModbusRegisterCodec.h`
- Create: `src/plugins/modbus_tcp/ModbusRegisterCodec.cpp`
- Create: `src/plugins/modbus_tcp/ModbusTcpWorker.h`
- Create: `src/plugins/modbus_tcp/ModbusTcpWorker.cpp`
- Create: `src/plugins/modbus_tcp/ModbusTcpPlugin.h`
- Create: `src/plugins/modbus_tcp/ModbusTcpPlugin.cpp`
- Create: `src/plugins/modbus_tcp/modbus_tcp.json`
- Test: `tests/unit/tst_modbus_register_codec.cpp`

**Interfaces:**
- Consumes: `IProtocolPlugin` 和 SDK DTO。
- Produces: key 为 `modbus-tcp` 的动态插件；Worker 连接成功后异步读取地址 `0..4`，`writeValue()` 写地址 `10`。

- [ ] **Step 1: 写 5 寄存器解码和非法长度失败测试**

```cpp
const auto result = ModbusRegisterCodec::decodeSnapshot({420, 120, 1500, 2200, 1}, "plc-1", now, 7);
QCOMPARE(result.samples.size(), 5);
QCOMPARE(result.samples.at(0).engineeringValue, 42.0);
QCOMPARE(result.samples.at(1).engineeringValue, 1.2);
```

- [ ] **Step 2: 运行并确认缺少 Codec 而失败**

Run: `cmake --build build --target tst_modbus_register_codec`

Expected: 编译失败，指出 `ModbusRegisterCodec` 未定义。

- [ ] **Step 3: 实现最小编解码、`QModbusTcpClient` 连接、一次读取和功能码 06 写入**

- [ ] **Step 4: 构建插件并运行 Codec 测试**

Run: `cmake --build build --target modbus_tcp_plugin && ctest --test-dir build -R modbus_register_codec --output-on-failure`

Expected: 插件生成到 `build/bin/plugins/`，Codec 测试通过。

### Task 6: 真实插件—TCP 集成闭环、控制台入口和文档回写

**Files:**
- Create: `src/monitor/main.cpp`
- Test: `tests/integration/tst_modbus_plugin_integration.cpp`
- Modify: `README.md`
- Modify: `文档/工业监控系统设计.md`

**Interfaces:**
- Consumes: `PluginManager`、真实 `modbus_tcp_plugin`、`VirtualPlcServer`。
- Produces: 第一周最终验收证据。

- [ ] **Step 1: 写动态加载、异步读取 5 个寄存器和写目标转速的集成测试**

```cpp
QSignalSpy samplesSpy(worker, &AbstractDeviceWorker::samplesReady);
worker->start();
QTRY_COMPARE_WITH_TIMEOUT(samplesSpy.count(), 1, 3000);
QCOMPARE(qvariant_cast<SampleBatch>(samplesSpy.takeFirst().at(0)).size(), 5);
```

- [ ] **Step 2: 运行并确认闭环尚未满足而失败**

Run: `ctest --test-dir build -R modbus_plugin_integration --output-on-failure`

Expected: 因插件未被复制、无法连接或读链未完成而失败。

- [ ] **Step 3: 补齐运行目录依赖和 `industrial_monitor` 控制台探测入口**

- [ ] **Step 4: 运行完整构建与测试**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure`

Expected: 全部测试通过。

- [ ] **Step 5: 回写设计文档的第一周完成清单、实际文件、测试命令与未进入本周范围的事项**

- [ ] **Step 6: 做差异和格式验证**

Run: `git diff --check && git status --short`

Expected: 无空白错误；仅出现第一周实现、测试、README、计划和原设计文档相关变更。
