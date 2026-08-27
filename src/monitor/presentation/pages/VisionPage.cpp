#include "VisionPage.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

using namespace industrial::monitor::vision;

VisionPage::VisionPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("visionPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // 第一块：标题和播放命令，文件名始终以文字展示。
    auto *title = new QLabel(QStringLiteral("视觉实验"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *controls = new QHBoxLayout;
    m_selectButton = new QPushButton(QStringLiteral("选择视频"), this);
    m_selectButton->setObjectName(QStringLiteral("visionSelectButton"));
    m_selectButton->setAccessibleName(QStringLiteral("选择本地视频文件"));
    m_playButton = new QPushButton(QStringLiteral("播放"), this);
    m_playButton->setObjectName(QStringLiteral("visionPlayButton"));
    m_playButton->setAccessibleName(QStringLiteral("播放或继续视频"));
    m_pauseButton = new QPushButton(QStringLiteral("暂停"), this);
    m_pauseButton->setObjectName(QStringLiteral("visionPauseButton"));
    m_pauseButton->setAccessibleName(QStringLiteral("暂停视频"));
    m_stopButton = new QPushButton(QStringLiteral("停止"), this);
    m_stopButton->setObjectName(QStringLiteral("visionStopButton"));
    m_stopButton->setAccessibleName(QStringLiteral("停止并释放视频"));
    m_fileNameLabel = new QLabel(QStringLiteral("未选择视频"), this);
    m_fileNameLabel->setObjectName(QStringLiteral("visionFileNameLabel"));
    m_fileNameLabel->setAccessibleName(QStringLiteral("当前视频文件名"));
    m_fileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    controls->addWidget(m_selectButton);
    controls->addWidget(m_playButton);
    controls->addWidget(m_pauseButton);
    controls->addWidget(m_stopButton);
    controls->addSpacing(12);
    controls->addWidget(m_fileNameLabel, 1);
    root->addLayout(controls);

    // 第二块：只保存最新 QImage；QPixmap 的创建与缩放始终留在 UI 主线程。
    m_frameLabel = new QLabel(QStringLiteral("请选择包含红色物体的本地视频"), this);
    m_frameLabel->setObjectName(QStringLiteral("visionFrameLabel"));
    m_frameLabel->setAccessibleName(QStringLiteral("视频目标检测画面"));
    m_frameLabel->setAlignment(Qt::AlignCenter);
    m_frameLabel->setMinimumSize(480, 280);
    m_frameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_frameLabel->setStyleSheet(QStringLiteral(
        "background:#0F172A;color:#CBD5E1;border:1px solid #334155;"
        "border-radius:6px;"));
    root->addWidget(m_frameLabel, 1);

    // 第三块：展示可验证事实，不给出生产质检式的合格/不合格结论。
    auto *resultGroup = new QGroupBox(QStringLiteral("播放与检测结果"), this);
    auto *resultLayout = new QGridLayout(resultGroup);
    m_stateLabel = new QLabel(QStringLiteral("空闲"), resultGroup);
    m_stateLabel->setObjectName(QStringLiteral("visionStateLabel"));
    m_progressLabel = new QLabel(QStringLiteral("00:00"), resultGroup);
    m_progressLabel->setObjectName(QStringLiteral("visionProgressLabel"));
    m_targetLabel = new QLabel(QStringLiteral("未检测到红色目标"), resultGroup);
    m_targetLabel->setObjectName(QStringLiteral("visionTargetLabel"));
    m_detectionTimeLabel = new QLabel(QStringLiteral("0.00 ms"), resultGroup);
    m_detectionTimeLabel->setObjectName(QStringLiteral("visionDetectionTimeLabel"));
    resultLayout->addWidget(new QLabel(QStringLiteral("播放状态："), resultGroup), 0, 0);
    resultLayout->addWidget(m_stateLabel, 0, 1);
    resultLayout->addWidget(new QLabel(QStringLiteral("视频位置："), resultGroup), 0, 2);
    resultLayout->addWidget(m_progressLabel, 0, 3);
    resultLayout->addWidget(new QLabel(QStringLiteral("当前检测："), resultGroup), 1, 0);
    resultLayout->addWidget(m_targetLabel, 1, 1);
    resultLayout->addWidget(new QLabel(QStringLiteral("单帧耗时："), resultGroup), 1, 2);
    resultLayout->addWidget(m_detectionTimeLabel, 1, 3);
    resultLayout->setColumnStretch(1, 1);
    resultLayout->setColumnStretch(3, 1);
    root->addWidget(resultGroup);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("visionErrorLabel"));
    m_errorLabel->setAccessibleName(QStringLiteral("视觉模块错误"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#B42318;font-weight:600;"));
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    connect(m_selectButton, &QPushButton::clicked,
            this, &VisionPage::chooseVideo);
    connect(m_playButton, &QPushButton::clicked,
            this, &VisionPage::playRequested);
    connect(m_pauseButton, &QPushButton::clicked,
            this, &VisionPage::pauseRequested);
    connect(m_stopButton, &QPushButton::clicked,
            this, &VisionPage::stopRequested);
    setPlaybackState(VisionPlaybackState::Idle, QStringLiteral("空闲"));
}

void VisionPage::setSourceInfo(const VisionSourceInfo &source)
{
    m_source = source;
    m_fileNameLabel->setText(source.displayName.isEmpty()
        ? QStringLiteral("未选择视频")
        : source.displayName);
    m_fileNameLabel->setToolTip(source.canonicalPath);
    m_errorLabel->clear();
    m_errorLabel->hide();
    updateProgress(0);
}

void VisionPage::setFrame(const VisionFrameResult &frame)
{
    m_latestImage = frame.image;
    updateDisplayedFrame();
    updateProgress(frame.positionMs);
    m_targetLabel->setText(frame.targetCount == 0
        ? QStringLiteral("未检测到红色目标")
        : QStringLiteral("检测到 %1 个红色目标").arg(frame.targetCount));
    m_detectionTimeLabel->setText(
        QStringLiteral("%1 ms").arg(frame.detectionTimeMs, 0, 'f', 2));
}

void VisionPage::setPlaybackState(VisionPlaybackState state,
                                  const QString &message)
{
    m_state = state;
    m_stateLabel->setText(message.isEmpty() ? defaultStateText(state) : message);
    m_selectButton->setEnabled(true);
    m_playButton->setEnabled(state == VisionPlaybackState::Ready
                             || state == VisionPlaybackState::Paused
                             || state == VisionPlaybackState::Finished);
    m_pauseButton->setEnabled(state == VisionPlaybackState::Playing);
    m_stopButton->setEnabled(state != VisionPlaybackState::Idle);

    m_playButton->setText(state == VisionPlaybackState::Paused
        ? QStringLiteral("继续")
        : state == VisionPlaybackState::Finished
            ? QStringLiteral("重播")
            : QStringLiteral("播放"));

    if (state == VisionPlaybackState::Idle) {
        m_source = VisionSourceInfo{};
        m_fileNameLabel->setText(QStringLiteral("未选择视频"));
        m_fileNameLabel->setToolTip({});
        m_targetLabel->setText(QStringLiteral("未检测到红色目标"));
        m_detectionTimeLabel->setText(QStringLiteral("0.00 ms"));
        updateProgress(0);
    }
    if (state == VisionPlaybackState::Error && !message.isEmpty()) {
        m_errorLabel->setText(message);
        m_errorLabel->show();
    } else if (state != VisionPlaybackState::Error) {
        m_errorLabel->clear();
        m_errorLabel->hide();
    }
}

void VisionPage::showError(const QString &message)
{
    setPlaybackState(VisionPlaybackState::Error, message);
}

void VisionPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayedFrame();
}

void VisionPage::chooseVideo()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择本地视频"), {},
        QStringLiteral("视频文件 (*.mp4 *.avi *.mov *.mkv);;所有文件 (*)"));
    if (!path.isEmpty()) {
        emit openVideoRequested(path);
    }
}

QString VisionPage::defaultStateText(VisionPlaybackState state)
{
    switch (state) {
    case VisionPlaybackState::Idle:
        return QStringLiteral("空闲");
    case VisionPlaybackState::Ready:
        return QStringLiteral("视频已就绪");
    case VisionPlaybackState::Playing:
        return QStringLiteral("视频播放中");
    case VisionPlaybackState::Paused:
        return QStringLiteral("视频已暂停");
    case VisionPlaybackState::Finished:
        return QStringLiteral("视频播放结束");
    case VisionPlaybackState::Error:
        return QStringLiteral("视频处理错误");
    }
    return QStringLiteral("未知状态");
}

QString VisionPage::formatDuration(qint64 milliseconds)
{
    const qint64 totalSeconds = qMax<qint64>(0, milliseconds / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void VisionPage::updateProgress(qint64 positionMs)
{
    const QString position = formatDuration(positionMs);
    m_progressLabel->setText(m_source.estimatedDurationMs >= 0
        ? QStringLiteral("%1 / %2")
              .arg(position, formatDuration(m_source.estimatedDurationMs))
        : position);
}

void VisionPage::updateDisplayedFrame()
{
    if (m_latestImage.isNull()) {
        return;
    }
    const QSize available = m_frameLabel->contentsRect().size();
    if (available.isEmpty()) {
        return;
    }
    m_frameLabel->setPixmap(QPixmap::fromImage(m_latestImage).scaled(
        available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
