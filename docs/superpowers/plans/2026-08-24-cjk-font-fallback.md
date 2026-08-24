# VirtualPLC 中文字体回退 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 VirtualPLC 在没有系统 CJK 字体的 Linux 环境中稳定显示中文，并用自动化测试防止方框字回归。

**Architecture:** 将字体加载封装在独立 `UiFont` 模块，通过 Qt Resource 内嵌字体，并在窗口创建前设置 `QApplication` 全局字体。测试创建真实 `VirtualPlcWindow`，使用 `QRawFont` 验证最终继承字体的中文字形，而不是只测试资源文件存在。

**Tech Stack:** C++17、Qt 6.2.4 Widgets/Gui/Test、Qt Resource System、CTest、WenQuanYi Micro Hei

## Global Constraints

- 不依赖系统预装 CJK 字体。
- 不修改 VirtualPLC 现有布局和业务行为。
- 字体资源必须是未修改的 WenQuanYi Micro Hei 0.2.0-beta，SHA-256 为 `2420e8078af796b19a3f6ef13de527a1a91c1e7171eea115926c614ced1009b3`。
- 许可证选择 Apache License 2.0，并随安装产物提供完整文本和上游版权说明。

---

### Task 1: 中文字形回归测试

**Files:**
- Create: `tests/unit/tst_virtual_plc_ui_font.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `VirtualPlcWindow::font()`。
- Produces: CTest `unit.virtual_plc_ui_font`。

- [x] **Step 1: 写失败测试**

```cpp
VirtualPlcWindow window;
const QRawFont rawFont = QRawFont::fromFont(window.font());
QVERIFY(rawFont.isValid());
QVERIFY2(rawFont.supportsCharacter(QChar(u'测')),
         qPrintable(QStringLiteral("字体 %1 不包含中文字形")
                        .arg(window.font().family())));
```

- [x] **Step 2: 验证 RED**

Run: `QT_QPA_PLATFORM=offscreen ./build/bin/tst_virtual_plc_ui_font -o -,txt`

Expected: 测试失败并报告当前 `Sans Serif` 或 `DejaVu Sans` 不包含中文字形。

### Task 2: 字体资源与全局初始化

**Files:**
- Create: `src/virtual_plc/UiFont.h`
- Create: `src/virtual_plc/UiFont.cpp`
- Create: `src/virtual_plc/ChineseFonts.qrc`
- Create: `src/virtual_plc/assets/fonts/wqy-microhei.ttc`
- Modify: `src/virtual_plc/CMakeLists.txt`
- Modify: `src/virtual_plc/main.cpp`

**Interfaces:**
- Produces: `bool configureChineseUiFont()`，幂等加载资源字体并设置 `QApplication` 全局字体。

- [x] **Step 1: 实现最小字体初始化**

```cpp
bool configureChineseUiFont()
{
    initializeChineseFontResources();
    const int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/wqy-microhei.ttc"));
    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (fontId < 0 || families.isEmpty()) {
        return false;
    }
    QFont font = QApplication::font();
    font.setFamily(families.constFirst());
    QApplication::setFont(font);
    return true;
}
```

- [x] **Step 2: 在窗口创建前初始化**

```cpp
QApplication application(argc, argv);
configureChineseUiFont();
VirtualPlcWindow window;
```

- [x] **Step 3: 验证 GREEN**

Run: `QT_QPA_PLATFORM=offscreen ./build/bin/tst_virtual_plc_ui_font -o -,txt`

Expected: 字体有效且支持“测”字。

### Task 3: 许可证、文档与完整验证

**Files:**
- Create: `THIRD_PARTY_NOTICES.md`
- Create: `src/virtual_plc/assets/fonts/WENQUANYI-MICRO-HEI-COPYRIGHT`
- Create: `src/virtual_plc/assets/fonts/APACHE-2.0.txt`
- Modify: `cmake/InstallLayout.cmake`
- Modify: `README.md`
- Modify: `文档/工业监控系统设计.md`

**Interfaces:**
- Produces: 可追踪的第三方资产来源与安装许可证文件。

- [x] **Step 1: 复制未经修改的字体及完整许可证文件**

Run: `sha256sum src/virtual_plc/assets/fonts/wqy-microhei.ttc`

Expected: `2420e8078af796b19a3f6ef13de527a1a91c1e7171eea115926c614ced1009b3`。

- [x] **Step 2: 更新 README 和原设计文档的修复记录**

- [x] **Step 3: 完整验证**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j2`

Expected: 配置和构建退出码均为 0。

Run: `ctest --test-dir build --output-on-failure`

Expected: 8/8 测试通过。

Run: `git diff --check`

Expected: 无空白错误。
