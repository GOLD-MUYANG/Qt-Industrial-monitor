# OpenCV 视频目标检测模块实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 Qt 6 Widgets 工业监控程序中增加独立“视觉实验”页面，以 OpenCV 4 解码本地视频并逐帧完成高饱和红色目标检测、画框、计数和播放控制。

**Architecture:** `VisionPage` 只收发 Qt 值对象；`VisionSession` 在主线程管理专用 `QThread`；`VideoFileWorker` 在视觉线程独占 `QTimer` 与 `cv::VideoCapture`；`ColorObjectDetector` 和 `VisionFrameConverter` 分别负责纯算法与像素所有权转换。视觉链经 `ApplicationController` 与 `MainWindow` 装配，但不接入 Modbus、报警、SQLite 或 QML 前端。

**Tech Stack:** C++17、Qt 6.2+ Core/Gui/Widgets/Test、OpenCV 4 `core/imgproc/videoio`、CMake 3.22+、CTest。

## Global Constraints

- 只读取用户选择的本地视频，不访问摄像头或 `/dev/video*`。
- 固定检测 HSV 两段红色：`H 0..10 / 170..179`、`S 100..255`、`V 80..255`。
- 形态学开闭操作均使用 `3x3` 椭圆核，轮廓面积阈值为 `500 px`。
- 输入宽度超过 `1280` 时等比缩小，小图不放大；原始 `cv::Mat` 不得被修改。
- FPS 小于 `1` 或非有限值时回退 `30`，高于 `60` 时按 `60` 驱动。
- 跨线程只传 `VisionSourceInfo`、`VisionFrameResult` 等 Qt 值对象；不传 `cv::Mat`、`QPixmap`、Worker 或 Capture 指针。
- `QImage` 发出前必须深拷贝；`QPixmap` 只在 UI 主线程创建。
- 视觉失败不得改变协议、数据质量、报警或 SQLite 线程的状态。
- 不扩展到 QML 页面、摄像头、AI 模型、跟踪、截图保存或数据库持久化。

---

### Task 1: 建立 OpenCV 构建目标和视觉值类型

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/monitor/CMakeLists.txt`
- Create: `src/monitor/vision/VisionTypes.h`
- Create: `tests/unit/tst_vision_types.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `VisionPlaybackState`、`VisionSourceInfo`、`VisionFrameResult`、`registerVisionMetaTypes()`。
- Consumes: Qt `QString/QImage/QMetaType` 与 OpenCV 4 CMake 组件。

- [ ] **Step 1: 写失败的值类型测试**

  测试调用 `registerVisionMetaTypes()`，断言三个类型均可由 `QMetaType::fromName()` 找到，并验证默认时长与帧号使用 `-1/0` 表达未知或未处理状态。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target tst_vision_types -j2`

  Expected: 配置或编译失败，因为 OpenCV 查找、`monitor_vision` 和值类型尚未定义。

- [ ] **Step 3: 添加最小值类型与目标**

  `VisionTypes.h` 在 `industrial::monitor::vision` 命名空间定义六态枚举、源信息与帧结果结构；在命名空间外使用 `Q_DECLARE_METATYPE`，并由内联 `registerVisionMetaTypes()` 调用 `qRegisterMetaType<T>()`。顶层 CMake 使用：

  ```cmake
  find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc videoio)
  ```

  `monitor_vision` 公开 Qt Core/Gui、视觉头文件路径和 OpenCV 头文件，链接 `opencv_core`、`opencv_imgproc`、`opencv_videoio`。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_vision_types -j2 && build/bin/tst_vision_types`

  Expected: 全部值类型注册断言通过。

### Task 2: 测试驱动红色检测纯算法

**Files:**
- Create: `src/monitor/vision/ColorObjectDetector.h`
- Create: `src/monitor/vision/ColorObjectDetector.cpp`
- Create: `tests/unit/tst_color_object_detector.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `ColorDetectionResult ColorObjectDetector::process(const cv::Mat &input) const`。
- `ColorDetectionResult` 包含独立的 `annotatedFrame`、按左上位置稳定排序的 `std::vector<cv::Rect> boxes` 和 `targetCount()`。

- [ ] **Step 1: 写失败算法测试**

  分别构造黑帧、红矩形、蓝矩形、小红点、两个分离红区、Hue 0 与 Hue 179 的 HSV 转 BGR 图像，以及宽度 1600 的大图；断言计数、框位置、缩放尺寸和输入像素不变。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_color_object_detector -j2`

  Expected: 编译失败，因为检测器接口尚不存在。

- [ ] **Step 3: 实现最小检测流水线**

  `process()` 先复制或等比缩放输入，再依次执行 `cvtColor`、两次 `inRange`、`bitwise_or`、OPEN、CLOSE、`findContours(RETR_EXTERNAL, CHAIN_APPROX_SIMPLE)`、面积过滤、稳定排序、`boundingRect`、绿色矩形和 ASCII `Red #N` 标签；空帧或非 `CV_8UC3` 输入抛出 `std::invalid_argument`。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_color_object_detector -j2 && build/bin/tst_color_object_detector`

  Expected: 算法场景全部通过。

### Task 3: 测试驱动拥有独立像素的 QImage 转换

**Files:**
- Create: `src/monitor/vision/VisionFrameConverter.h`
- Create: `src/monitor/vision/VisionFrameConverter.cpp`
- Create: `tests/unit/tst_vision_frame_converter.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `QImage VisionFrameConverter::toOwnedQImage(const cv::Mat &bgrFrame)`。

- [ ] **Step 1: 写失败转换测试**

  验证 BGR `(0,0,255)` 转换后 `QColor` 为红色；空图和灰度图抛出 `std::invalid_argument`；返回后改写或销毁原 `cv::Mat`，`QImage` 像素保持不变。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_vision_frame_converter -j2`

  Expected: 编译失败，因为转换器尚未定义。

- [ ] **Step 3: 实现深拷贝转换**

  先用 `cv::cvtColor(..., COLOR_BGR2RGB)` 得到 RGB Mat，再用包含 `bytesPerLine` 的 `QImage` 构造函数包装，最后立即调用 `copy()` 返回独立像素。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_vision_frame_converter -j2 && build/bin/tst_vision_frame_converter`

  Expected: 颜色、错误和所有权测试全部通过。

### Task 4: 测试驱动视频 Worker 的状态机与线程内资源

**Files:**
- Create: `src/monitor/vision/VideoFileWorker.h`
- Create: `src/monitor/vision/VideoFileWorker.cpp`
- Create: `tests/integration/tst_video_file_worker.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Slots: `initialize()`、`openVideo(QString)`、`play()`、`pause()`、`stop()`、`shutdown()`。
- Signals: `initialized(quintptr)`、`sourceOpened(VisionSourceInfo)`、`frameReady(VisionFrameResult)`、`stateChanged(VisionPlaybackState, QString)`、`videoError(QString)`、`shutdownFinished()`。

- [ ] **Step 1: 写失败 Worker 集成测试**

  在 `QTemporaryDir` 中用 `VideoWriter::fourcc('M','J','P','G')` 生成多帧 AVI；若编码后端不可用则 `QSKIP` 并打印原因。覆盖无效路径、不可解码文件、首帧预览、播放到 EOF、暂停不增帧、继续恢复、停止后重开、直接连接记录处理线程而 queued 接收发生在测试主线程、shutdown 后线程退出。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_video_file_worker -j2`

  Expected: 编译失败，因为 Worker 尚未定义。

- [ ] **Step 3: 实现 Worker 最小状态机**

  `initialize()` 在 Worker 所在线程创建带 parent 的 `QTimer` 与 `VideoCapture`；`openVideo()` 先释放旧源，验证路径并读首帧；Timer timeout 逐帧检测、深拷贝、发结果；EOF 进入 `Finished`，明显提前失败和 OpenCV 异常进入 `Error`；`Finished::play()` 重新打开上次路径并自动播放；`stop/shutdown` 均在视觉线程停止 Timer 并 release。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_video_file_worker -j2 && build/bin/tst_video_file_worker`

  Expected: Worker 状态、线程和资源测试通过，或仅编码器能力检查明确跳过。

### Task 5: 测试驱动 VisionSession 生命周期代理

**Files:**
- Create: `src/monitor/vision/VisionSession.h`
- Create: `src/monitor/vision/VisionSession.cpp`
- Create: `tests/unit/tst_vision_session.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Public: `bool start()`、`bool shutdown(int timeoutMs)`、`bool isRunning() const`。
- Slots: `openVideo(QString)`、`play()`、`pause()`、`stop()`。
- Signals: 转发 Worker 的源、帧、状态、错误，以及内部六个 queued 命令和 `workerInitialized(quintptr)`。

- [ ] **Step 1: 写失败 Session 测试**

  启动 Session，断言 Worker 初始化线程 ID 非测试主线程；向无效路径投递打开请求并从主线程收到错误；调用有限等待 shutdown 后断言线程停止，重复 shutdown 幂等。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_vision_session -j2`

  Expected: 编译失败，因为 Session 尚未定义。

- [ ] **Step 3: 实现 Session 生命周期**

  创建无 parent Worker 和堆上 QThread，先连接 started/commands/results/shutdown/finished，再 `moveToThread()` 与 `start()`；shutdown 使用局部 `QEventLoop + single-shot QTimer` 等待 thread finished，超时只报告并保留 Worker 自主退出链，不调用 `terminate()` 或跨线程 delete。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_vision_session -j2 && build/bin/tst_vision_session`

  Expected: 线程归属、queued 命令和关闭测试通过。

### Task 6: 测试驱动 VisionPage 与第四个 Widgets 页面

**Files:**
- Create: `src/monitor/presentation/pages/VisionPage.h`
- Create: `src/monitor/presentation/pages/VisionPage.cpp`
- Create: `tests/unit/tst_vision_page.cpp`
- Modify: `src/monitor/presentation/MainWindow.h`
- Modify: `src/monitor/presentation/MainWindow.cpp`
- Modify: `tests/unit/tst_monitor_window.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Page signals: `openVideoRequested(QString)`、`playRequested()`、`pauseRequested()`、`stopRequested()`。
- Page slots: `setSourceInfo(...)`、`setFrame(...)`、`setPlaybackState(...)`、`showError(...)`。
- MainWindow 增加同名视觉命令信号和 `showVision*` 转发槽。

- [ ] **Step 1: 写失败页面测试**

  offscreen 创建页面，验证 `Idle/Ready/Playing/Paused/Finished/Error` 的按钮矩阵与播放按钮文案；点击播放/暂停/停止检查信号；输入帧结果检查目标数、检测耗时和无目标文案；输入错误检查页内红色错误文字。更新窗口测试断言第四导航项和 `visionPage` 存在。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_vision_page tst_monitor_window -j2`

  Expected: 编译或断言失败，因为第四页尚不存在。

- [ ] **Step 3: 实现页面和主窗口接线**

  页面分顶部控制、中央画面、底部结果三块；文件对话框仅由“选择视频”按钮触发，空路径不发命令；保存最新 `QImage`，在 `resizeEvent` 中按比例更新 `QPixmap`；状态函数是按钮 enabled/text 的唯一来源。MainWindow 新增第四导航项并只转发 Qt 值对象。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_vision_page tst_monitor_window -j2 && QT_QPA_PLATFORM=offscreen build/bin/tst_vision_page && QT_QPA_PLATFORM=offscreen build/bin/tst_monitor_window`

  Expected: 页面状态和第四导航测试通过。

### Task 7: 控制器装配、退出顺序与真实主程序闭环

**Files:**
- Modify: `src/monitor/application/ApplicationController.h`
- Modify: `src/monitor/application/ApplicationController.cpp`
- Modify: `src/monitor/main.cpp`
- Modify: `src/monitor/CMakeLists.txt`
- Create: `tests/unit/tst_application_controller_vision.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Controller slots: `openVisionVideo(QString)`、`playVisionVideo()`、`pauseVisionVideo()`、`stopVisionVideo()`。
- Controller signals: `visionSourceOpened(...)`、`visionFrameReady(...)`、`visionStateChanged(...)`、`visionError(QString)`。

- [ ] **Step 1: 写失败控制器视觉测试**

  用临时空插件目录验证控制器未启动时视觉命令无副作用；对可独立构造的 Session 装配路径验证视觉错误只走 `visionError`，不产生协议、报警或存储信号。主窗口测试通过信号 spy 验证命令从页面经窗口暴露。

- [ ] **Step 2: 运行测试确认 RED**

  Run: `cmake --build build --target tst_application_controller_vision -j2`

  Expected: 编译失败，因为控制器尚无视觉接口。

- [ ] **Step 3: 实现控制器与 main 装配**

  `ApplicationController::start()` 注册视觉元类型并启动 `VisionSession`；视觉命令只代理到 Session；shutdown 在设备、数据、存储、历史关闭前优先有限等待视觉线程；`isRunning()` 包含视觉线程。`main.cpp` 双向连接 Window 与 Controller 的全部视觉信号槽。

- [ ] **Step 4: 运行测试确认 GREEN**

  Run: `cmake --build build --target tst_application_controller_vision industrial_monitor -j2 && build/bin/tst_application_controller_vision`

  Expected: 控制器隔离、主程序链接与退出链测试通过。

### Task 8: 回写文档并完成全量验证

**Files:**
- Modify: `README.md`
- Modify: `文档/OpenCV视频目标检测设计.md`

**Interfaces:**
- Consumes: 全部实现与测试结果。
- Produces: 已实现范围、运行方法、真实测试证据、编码器跳过情况和未实现边界。

- [ ] **Step 1: 回写实际状态**

  README 增加 OpenCV 4 依赖、视觉实验启动/演示说明，并明确“固定 HSV 颜色规则，不是 AI 或生产质检”。设计文档追加实施状态，列出实际文件、测试名、命令与仍不支持的范围。

- [ ] **Step 2: 运行针对性验证**

  Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j2`

  Run: `ctest --test-dir build --output-on-failure -R 'vision|monitor_window|application_controller'`

  Expected: 新增视觉测试和相关窗口/控制器测试全部通过；编码器缺失时只有文档允许的明确 `QSKIP`。

- [ ] **Step 3: 运行全量回归**

  Run: `ctest --test-dir build --output-on-failure`

  Expected: 全部测试通过；若受沙箱 socket 限制，记录具体失败并在允许环回监听的环境重跑。

- [ ] **Step 4: 静态收尾检查**

  Run: `git diff --check`

  Run: `rg -n 'OpenCV|颜色规则|AI|视觉实验' README.md '文档/OpenCV视频目标检测设计.md'`

  Expected: 无格式错误，能力边界文字明确，工作区没有无关改动。
