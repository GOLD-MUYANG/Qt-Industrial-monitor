# VirtualPLC 中文字体回退设计

## 问题与证据

VirtualPLC 的中文显示为方框。源文件和文档均为 UTF-8，运行 locale 为 `C.UTF-8`；当前系统 `fc-list ':lang=zh-cn'` 无结果，Qt 只能选择不包含中文字形的 DejaVu Sans。因此根因是字体缺字，不是字符串编码损坏。

## 方案

应用内嵌未修改的 WenQuanYi Micro Hei 0.2.0-beta 字体。`UiFont` 模块在 `QApplication` 创建后、任何窗口创建前通过 `QFontDatabase::addApplicationFont()` 加载资源，取得字体族并设置为应用全局字体。VirtualPLC 的窗口和子控件自然继承该字体，不在各控件重复设置。

## 文件边界

- `src/virtual_plc/UiFont.h/.cpp`：字体资源初始化、字体加载、字体族选择和全局设置。
- `src/virtual_plc/ChineseFonts.qrc`：将字体暴露为 `:/fonts/wqy-microhei.ttc`。
- `src/virtual_plc/assets/fonts/`：未修改字体、原版权说明和 Apache 2.0 全文。
- `tests/unit/tst_virtual_plc_ui_font.cpp`：在 offscreen GUI 环境验证窗口字体支持“测”字。
- `THIRD_PARTY_NOTICES.md`：说明字体来源、版本、版权和许可证选择。

## 启动链与失败处理

```text
QApplication 创建
-> configureChineseUiFont()
-> Q_INIT_RESOURCE(ChineseFonts)
-> QFontDatabase::addApplicationFont()
-> QApplication::setFont()
-> VirtualPlcWindow 创建
```

资源初始化函数放在全局作用域，避免 Qt 资源符号进入错误命名空间。字体加载失败时记录明确 warning 并返回 `false`；生产程序继续启动，但自动化字形测试会阻止缺字体的构建被当作修复完成。

## 验收

- `QRawFont::fromFont(window.font()).supportsCharacter(QChar(u'测'))` 返回 `true`。
- 原有 7 项测试继续通过，新增字体测试后为 8 项。
- `virtual_plc` 在无系统 CJK 字体的当前环境中显示中文而不是方框。
- 安装产物携带第三方字体版权和 Apache 2.0 文本。
