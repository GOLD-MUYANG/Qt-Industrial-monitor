# OpenCV 视频目标检测模块设计

## 1. 文档信息

| 项目 | 内容 |
| --- | --- |
| 所属项目 | Industrial Monitoring System |
| 设计日期 | 2026-08-27 |
| 功能定位 | 面向学习和演示的独立视觉实验模块 |
| 输入来源 | 用户选择的本地视频文件 |
| 识别目标 | 高饱和红色物体 |
| 核心技术 | Qt 6 Widgets、QObject Worker + QThread、OpenCV 4 |

本文档定义一个最小、可运行、可测试的 OpenCV 视觉闭环。它不模拟生产级
机器视觉系统，也不把简单颜色规则包装成 AI 模型。用户选择包含红色物体的
本地视频后，系统逐帧识别红色区域、绘制检测框并显示目标数量。

## 2. 背景与目标

当前项目已经具备动态 Modbus TCP 插件、通信线程、数据处理线程、SQLite
读写线程，以及实时监控、设备管理和报警中心三个 Widgets 页面。视觉模块
作为第四个独立页面接入，但不修改现有协议、数据质量、报警或存储语义。

本模块解决三个练习目标：

1. 使用 `cv::VideoCapture` 读取真实视频帧。
2. 使用 OpenCV 完成颜色空间转换、阈值分割、形态学去噪和轮廓检测。
3. 练习 OpenCV 图像与 Qt 界面之间的线程安全传递和实时显示。

演示闭环为：

```text
选择本地视频
-> OpenCV 打开并解码
-> 逐帧缩放和 HSV 红色分割
-> 轮廓面积过滤
-> 绘制目标框和编号
-> Qt 页面显示画面、进度和目标数量
```

## 3. 范围

### 3.1 必须实现

- 在主窗口侧边栏增加“视觉实验”页面。
- 使用文件选择器选择一个本地视频文件。
- 支持打开、播放、暂停、继续、停止和播放结束状态。
- 使用 OpenCV `videoio` 解码视频，不使用 Qt Multimedia。
- 对每帧执行红色目标检测，并在画面上绘制外接矩形和编号。
- 显示当前文件名、播放状态、视频位置和当前帧目标数量。
- 视频解码和图像处理不阻塞 UI 主线程。
- 不检测到目标属于正常结果，页面显示“未检测到红色目标”。
- 视频无法打开、无法读取或格式不受后端支持时给出明确错误。
- 用独立单元测试和集成测试验证算法、内存所有权、线程归属和播放状态。

### 3.2 明确不做

- 不访问摄像头，不依赖 `/dev/video*`。
- 不做人脸识别、通用物体识别、YOLO 或任何模型训练和推理。
- 不做二维码识别、运动检测、跨帧跟踪或目标身份保持。
- 不提供多颜色切换、HSV 参数调节面板或区域标定工具。
- 不保存截图、检测视频或检测结果到 SQLite。
- 不把视觉结果接入现有报警中心、Modbus 数据流水线或 VirtualPLC。
- 不处理音频，也不承诺音画同步和逐帧精确的视频播放器能力。
- 不承诺所有封装格式都可解码；实际能力取决于 OpenCV 的视频后端和编解码器。

这些限制保证本轮重点始终是一个可以解释的 OpenCV 最小纵向闭环。

## 4. 方案选择

### 4.1 采用方案：HSV 红色分割与轮廓计数

每帧先从 BGR 转换到 HSV。红色在 OpenCV 的 Hue 范围两端，因此分别生成
`0..10` 和 `170..179` 两个掩膜，再合并、去噪、提取外部轮廓。过滤面积过小
的轮廓后，剩余轮廓就是当前帧的检测目标。

选择该方案的原因：

- 不需要模型文件、样本标注和网络下载。
- 输入、处理过程和失败条件都能从源码直接解释。
- 可以用红色水杯、瓶盖、卡片等日常物品录制演示视频。
- 可以用程序生成的图像稳定测试，不依赖人工观察判断结果。

### 4.2 未采用方案

- **帧差运动检测**：实现量小，但光照和相机抖动容易产生误报，结果不如颜色目标稳定。
- **二维码检测**：演示稳定，但主要调用 `QRCodeDetector`，能展示的基础图像处理过程较少。
- **深度学习目标检测**：需要模型、预处理和推理运行时，明显超出本轮练习范围。

## 5. 总体架构

```text
UI 主线程
|
+-- MainWindow
|   +-- VisionPage
|       +-- 文件选择与播放控制
|       +-- QImage 画面显示
|       +-- 状态、进度和目标数量
|
+-- ApplicationController
    +-- 创建并持有 VisionSession        [主线程对象，管理生命周期]
        +-- QThread                     [视觉线程载体]
    +-- 转发页面命令和视觉结果

视觉线程
|
+-- VideoFileWorker               [视觉线程 QObject]
    +-- cv::VideoCapture          [在视觉线程创建、打开和释放]
    +-- QTimer                    [按视频帧率驱动读取]
    +-- ColorObjectDetector       [无 QObject 的纯算法类]
    +-- VisionFrameConverter      [cv::Mat -> 独立 QImage]
```

### 5.1 架构原则

1. `VisionPage` 只展示结果和发送命令，不直接包含 OpenCV 头文件或资源对象。
2. `VideoFileWorker` 独占 `cv::VideoCapture` 和播放定时器；打开、读取、停止和
   释放都发生在视觉线程。
3. `ColorObjectDetector` 是同步、无状态的纯算法单元，只处理一帧图像，不知道
   Qt 控件、线程、视频文件或项目业务。
4. 跨线程只传递 Qt 值对象，不传递 `cv::Mat`、Worker 或 Capture 指针。
5. Worker 发出图像前必须复制像素，使 `QImage` 不再引用下一帧会覆盖的
   `cv::Mat` 内存。
6. 视觉线程和通信、数据、数据库线程互相独立；任一视觉错误不影响设备监控。

## 6. 目录与文件边界

```text
src/monitor/
├── vision/
│   ├── VisionTypes.h
│   ├── ColorObjectDetector.h/.cpp
│   ├── VisionFrameConverter.h/.cpp
│   ├── VideoFileWorker.h/.cpp
│   └── VisionSession.h/.cpp
├── application/
│   └── ApplicationController.h/.cpp
└── presentation/
    ├── MainWindow.h/.cpp
    └── pages/
        └── VisionPage.h/.cpp

tests/
├── unit/
│   ├── tst_color_object_detector.cpp
│   ├── tst_vision_frame_converter.cpp
│   ├── tst_vision_session.cpp
│   └── tst_vision_page.cpp
└── integration/
    └── tst_video_file_worker.cpp
```

| 文件或类 | 负责 | 不负责 |
| --- | --- | --- |
| `VisionTypes` | 播放状态、视频信息和跨线程帧结果值对象 | OpenCV 算法和资源管理 |
| `ColorObjectDetector` | 单帧缩放、红色分割、去噪、轮廓过滤和画框 | 视频读取、计时和 Qt 显示 |
| `VisionFrameConverter` | 将 BGR `cv::Mat` 转成拥有自身像素的 RGB `QImage` | 检测算法和控件缩放 |
| `VideoFileWorker` | 打开视频、按帧率读取、调用算法和发出结果 | 创建线程和更新控件 |
| `VisionSession` | 创建视觉线程、投递命令、有限等待关闭 | 图像处理和页面状态文案 |
| `ApplicationController` | 装配 Session，转发 UI 命令和结果 | 持有 Capture 或执行算法 |
| `VisionPage` | 文件选择、按钮状态、画面、进度和数量显示 | 视频解码、OpenCV 和线程管理 |

上述测试继续按职责拆分，避免把不同故障原因堆入同一个测试文件。

## 7. OpenCV 处理规则

### 7.1 输入和缩放

- 文件对话框展示 `mp4`、`avi`、`mov` 和 `mkv` 等常见扩展名，同时允许选择
  所有文件。
- 扩展名只用于选择提示；是否可读取以 `VideoCapture::open()`、`isOpened()`
  和首帧读取结果为准。
- 帧宽大于 1280 时按比例缩小到 1280；小图不放大。检测和最终显示都使用
  缩放后的帧，控制 CPU 占用并保持面积阈值语义相对稳定。
- 播放帧率取 `CAP_PROP_FPS`。有效范围限定为 `1..60 FPS`；无效值使用
  `30 FPS`，高于 60 的输入按 60 FPS 驱动。本模块不做积压帧追赶。
- 当前位置优先取 `CAP_PROP_POS_MSEC`；总时长只在 FPS 和
  `CAP_PROP_FRAME_COUNT` 都有效时由两者估算，否则页面不显示伪造的总时长。

### 7.2 默认红色范围

OpenCV 8 位 HSV 的 Hue 范围是 `0..179`。首版固定使用：

```text
低红区：H 0..10，S 100..255，V 80..255
高红区：H 170..179，S 100..255，V 80..255
```

两个 `inRange()` 掩膜使用按位或合并。固定参数的目标是让学习和测试结果
可重复，不追求覆盖暗红、反光或复杂光照。

### 7.3 去噪和目标判定

```text
BGR 帧
-> cvtColor(BGR -> HSV)
-> 两次 inRange
-> bitwise_or
-> 3x3 椭圆核 morphologyEx(OPEN)
-> 3x3 椭圆核 morphologyEx(CLOSE)
-> findContours(RETR_EXTERNAL, CHAIN_APPROX_SIMPLE)
-> contourArea >= 500 px
-> boundingRect + rectangle
```

- 小于 `500 px` 的区域视为噪点。
- 每个保留轮廓算一个目标；首版不合并相邻框，也不保持跨帧 ID。
- 检测框使用绿色，标签使用 ASCII 文本 `Red #1`、`Red #2`，避免
  `cv::putText` 的中文字形限制。
- 页面中文状态根据目标数显示“未检测到红色目标”或“检测到 N 个红色目标”。
- 同一个真实物体如果被遮挡或分裂成多个轮廓，计数可能波动；这是该规则方案
  的已知边界，不包装成稳定跟踪能力。

## 8. 值对象与接口

### 8.1 值对象

`VisionTypes.h` 定义以下协议无关的 Qt 类型：

- `VisionPlaybackState`：`Idle`、`Ready`、`Playing`、`Paused`、`Finished`、
  `Error`。
- `VisionSourceInfo`：规范化路径、显示文件名、帧率、总帧数和估算时长。
- `VisionFrameResult`：独立 `QImage`、当前位置、已处理帧号、红色目标数和
  检测耗时。

自定义类型使用 `Q_DECLARE_METATYPE`，并在建立 queued connection 前完成运行时
注册。`QImage` 可以跨线程按值传递；结果中的图像必须已经 `copy()` 或等价深拷贝，
页面再把它转换成 `QPixmap`。`QPixmap` 只在 UI 主线程创建。

### 8.2 Worker 命令和信号

`VideoFileWorker` 的命令槽：

```text
initialize()
openVideo(QString path)
play()
pause()
stop()
shutdown()
```

主要信号：

```text
sourceOpened(VisionSourceInfo)
frameReady(VisionFrameResult)
stateChanged(VisionPlaybackState, QString message)
videoError(QString message)
shutdownFinished()
```

命令只表示请求已经投递，不用同步 `bool` 假装视频异步播放已经成功。文件打开
结果、状态变化和错误均通过 Worker 信号返回。

## 9. 运行时调用链

### 9.1 打开视频

```text
VisionPage::选择视频
-> QFileDialog 返回路径                         [UI 主线程]
-> MainWindow::openVisionVideoRequested
-> ApplicationController::openVisionVideo
-> VisionSession::openVideo
-> VideoFileWorker::openVideo                    [queued，视觉线程]
   -> 停止并释放旧视频
   -> VideoCapture::open(path)
   -> 检查 isOpened、FPS、帧数和首帧
   -> 处理首帧作为预览
   -> sourceOpened + frameReady + Ready
-> ApplicationController 转发
-> MainWindow / VisionPage 更新预览和按钮        [UI 主线程]
```

### 9.2 播放和逐帧处理

```text
VisionPage::播放
-> ApplicationController
-> VisionSession
-> VideoFileWorker::play                         [视觉线程]
   -> QTimer 按计算后的间隔启动

每次 timeout
-> VideoCapture::read
-> ColorObjectDetector::process
-> VisionFrameConverter::toOwnedQImage
-> frameReady(VisionFrameResult)
-> VisionPage::setFrame                           [UI 主线程]
```

页面使用 `QLabel` 保持宽高比缩放最新帧。窗口尺寸变化只重新缩放已有 `QImage`，
不触发重复 OpenCV 检测。

### 9.3 暂停、结束和停止

- 暂停只停止视觉线程中的播放 `QTimer`，保留 Capture 位置和最后一帧。
- 继续从当前位置读取下一帧。
- `read()` 返回失败时，若有效总帧数表明当前位置已经到达末尾，则按正常 EOF
  处理；若当前位置明显早于末尾，则进入 `Error`。后端无法提供有效总帧数时，
  首帧之后的读取失败按 `Finished` 处理，并提示“视频结束或后端提前停止”。
- 停止会停止 Timer、释放 Capture、清除源信息并进入 `Idle`；页面可以保留
  最后一帧，但状态和计数归零。
- `Finished` 后点击播放时，Worker 重新打开上一次路径并从头播放；不依赖所有
  视频后端都支持精确 seek。

## 10. 页面设计

“视觉实验”页面保持现有 Widgets 风格，包含三个区域：

1. **顶部控制区**：选择视频、播放/继续、暂停、停止按钮和当前文件名。
2. **中央画面区**：显示带目标框的视频帧；无视频时显示操作提示。
3. **底部结果区**：播放状态、当前位置/估算总时长、当前目标数量和单帧检测耗时。

按钮状态由播放状态唯一决定：

| 状态 | 选择视频 | 播放 | 暂停 | 停止 |
| --- | --- | --- | --- | --- |
| `Idle` | 可用 | 禁用 | 禁用 | 禁用 |
| `Ready` | 可用 | 可用 | 禁用 | 可用 |
| `Playing` | 可用 | 禁用 | 可用 | 可用 |
| `Paused` | 可用 | 可用 | 禁用 | 可用 |
| `Finished` | 可用 | 可用，表示重播 | 禁用 | 可用 |
| `Error` | 可用 | 禁用 | 禁用 | 可用 |

画面不是生产质检结论，因此页面不显示“合格/不合格”，只陈述可验证事实：
是否检测到红色目标以及当前数量。

## 11. 错误处理

- **未选择文件**：不投递打开命令，不改变当前播放状态。
- **文件不存在或不可读**：进入 `Error`，显示路径不可访问，不影响其他页面。
- **容器或编解码器不支持**：`open()` 或首帧读取失败时释放 Capture，并提示
  尝试使用常见 H.264 MP4 或 MJPEG AVI；不以扩展名推断成功。
- **中途读取失败**：若尚未输出首帧则视为错误；有效总帧数表明已到末尾时视为
  正常结束；位置明显早于末尾时报告“视频提前结束”。后端不给出有效总帧数时，
  首帧后的读取失败进入 `Finished`，状态文字同时说明可能是后端提前停止。
- **异常 FPS/总帧数**：FPS 回退为 30；无法得到总帧数时进度只显示当前位置，
  不伪造总时长。
- **处理异常**：捕获 OpenCV 异常，停止 Timer、释放视频并发送错误；异常不得
  穿过 Qt 事件循环边界。
- **重复打开**：先停止并释放旧视频，再打开新文件。旧任务不会继续发帧。
- **关闭超时**：`VisionSession::shutdown()` 有限等待；超时返回失败并报告，
  不直接销毁仍在运行的 QThread。

## 12. 线程生命周期与关闭顺序

`ApplicationController::start()` 创建 `VisionSession`。Session 创建无 parent 的
Worker，移动到视觉线程后启动线程；Worker 收到 `initialize()` 后才创建 QTimer
和 `cv::VideoCapture`。

应用退出顺序在现有通信、数据和数据库关闭链中增加视觉模块，但视觉模块没有
业务队列依赖，可以优先停止：

```text
停止视觉 Timer
-> 在视觉线程 release VideoCapture
-> 确认 shutdownFinished
-> quit/wait 视觉线程
-> 继续现有设备、数据、存储和历史线程关闭流程
```

Worker 通过 `deleteLater` 在视觉线程结束前销毁。不得在 UI 线程直接调用
Capture，也不得使用 `QThread::terminate()`。

## 13. 构建与依赖

顶层 CMake 增加：

```cmake
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc videoio)
```

新增 `monitor_vision` 静态目标，链接 Qt Core/Gui 和 OpenCV 的 `core`、`imgproc`、
`videoio`。不链接 `highgui`，因为窗口显示全部由 Qt Widgets 完成。
`monitor_runtime` 和 `monitor_presentation` 只通过 `monitor_vision` 暴露的 Qt 值类型
和会话接口接入，不在页面源文件中包含 OpenCV 头文件。

当前环境尚未安装 OpenCV。Ubuntu 22.04 当前软件源提供
`libopencv-dev 4.5.4+dfsg-9ubuntu4`；实施前需要得到用户授权后安装，并重新执行
CMake 配置验证 `OpenCVConfig.cmake` 和三个组件。设计采用 OpenCV 4.x 稳定 API，
不要求 OpenCV 5。

## 14. 测试设计

### 14.1 算法单元测试

`tst_color_object_detector.cpp` 使用内存中的合成 `cv::Mat`，覆盖：

- 纯黑帧返回 0。
- 一个足够大的红色矩形返回 1，并生成正确外接框。
- 蓝色矩形不被识别。
- 面积小于阈值的红点被过滤。
- 两个分离红色区域返回 2。
- Hue 两端的两种红色都能进入合并掩膜。
- 大于 1280 像素的帧按比例缩放，原图不被修改。

### 14.2 转换单元测试

`tst_vision_frame_converter.cpp` 覆盖：

- BGR 红色像素转换后仍按 RGB 红色显示，防止红蓝通道互换。
- 灰度或空输入得到明确错误，不发生越界访问。
- 原 `cv::Mat` 销毁或改写后，发出的 `QImage` 像素保持不变，证明已获得独立内存。

### 14.3 Worker 和线程测试

`tst_video_file_worker.cpp` 在临时目录用 OpenCV `VideoWriter` 生成短 MJPEG AVI，
验证：

- 无效路径和不可解码文件发出错误。
- 有效视频能打开、产出首帧、播放并进入 EOF。
- 帧结果来自视觉线程，接收结果发生在测试主线程。
- 暂停期间不继续发帧，继续后恢复。
- 停止释放资源，同一路径可以再次打开。
- 关闭时不遗留运行中的 Timer 或线程。

若当前 OpenCV 构建没有可用的 MJPEG 编码后端，该项必须明确 `QSKIP` 并记录
环境原因；真实解码验收仍需使用用户提供的视频完成，不能把跳过写成通过。

### 14.4 Session 和页面测试

- `tst_vision_session.cpp` 验证 Worker 的线程归属、queued 命令和有限等待关闭。
- `tst_vision_page.cpp` 使用 `QT_QPA_PLATFORM=offscreen` 验证第四个导航项、按钮
  状态转换、页面信号、目标数量和错误文字；测试不弹出真实文件对话框。
- 现有 `unit.monitor_window` 增加第四页存在性检查，但视觉细节留在独立文件中。

## 15. 验收标准

实现完成必须同时满足：

1. CMake 明确找到 OpenCV 4 的 `core`、`imgproc` 和 `videoio`。
2. 原有全部 CTest 不回归，新增视觉测试通过或按第 14.3 节明确记录编码后端跳过。
3. 在当前 Qt 界面选择一个含红色物体的视频后，首帧预览正常，UI 可播放、暂停、
   继续、停止和重播。
4. 红色目标被逐帧画框并计数；无红色目标时稳定显示 0，不产生错误弹窗。
5. 播放期间可以切换到实时监控、设备管理和报警中心，UI 不因视频读取明显冻结。
6. 视频错误不会中断 Modbus 通信、数据统计、报警或 SQLite 线程。
7. 退出应用后视觉线程正常结束，不出现 `QThread: Destroyed while thread is still running`。
8. 运行 `git diff --check` 无新增格式错误，并在主设计文档或 README 中明确该模块
   是练习性质的颜色规则检测，而非生产级视觉识别。

## 16. 演示流程

1. 启动 `IndustrialMonitor`，确认原有三个页面和设备监控仍可用。
2. 进入“视觉实验”，选择一段包含红色杯子、瓶盖或卡片的视频。
3. 首帧显示后点击播放，观察红色目标的绿色检测框和实时计数。
4. 点击暂停，确认画面和位置停止；点击继续，确认从原位置恢复。
5. 播放到结尾后点击重播，确认从头开始。
6. 选择一个无红色物体的视频，确认结果为 0，而不是错误。
7. 选择无效文件，确认只在视觉页报告错误，原有监控链继续运行。

该流程可以在几分钟内完整演示视频输入、OpenCV 算法、跨线程值对象、Qt 页面
和错误隔离，符合练手项目的真实能力边界。
