#include "MainWindow.h"

#include "AlarmPage.h"
#include "DevicePage.h"
#include "RealtimePage.h"
#include "VisionPage.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QWidget>

using namespace industrial::monitor::vision;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_realtimePage(new RealtimePage(this))
    , m_devicePage(new DevicePage(this))
    , m_alarmPage(new AlarmPage(this))
    , m_visionPage(new VisionPage(this))
{
    setWindowTitle(QStringLiteral("Industrial Monitor"));
    resize(1180, 760);
    setMinimumSize(900, 620);

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *navigation = new QListWidget(central);
    navigation->setObjectName(QStringLiteral("mainNavigation"));
    navigation->setFixedWidth(170);
    navigation->addItem(QStringLiteral("实时监控"));
    navigation->addItem(QStringLiteral("设备管理"));
    navigation->addItem(QStringLiteral("报警中心"));
    navigation->addItem(QStringLiteral("视觉实验"));
    navigation->setCurrentRow(0);
    auto *pages = new QStackedWidget(central);
    pages->addWidget(m_realtimePage);
    pages->addWidget(m_devicePage);
    pages->addWidget(m_alarmPage);
    pages->addWidget(m_visionPage);
    layout->addWidget(navigation);
    layout->addWidget(pages, 1);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget{background:#F8FAFC;color:#0F172A;}"
        "QListWidget{background:#172033;color:#E2E8F0;border:0;padding-top:18px;}"
        "QListWidget::item{min-height:46px;padding-left:18px;}"
        "QListWidget::item:selected{background:#2563EB;color:white;}"
        "QPushButton{min-height:34px;padding:0 14px;border:1px solid #CBD5E1;"
        "border-radius:5px;background:white;}"
        "QPushButton:focus{border:2px solid #2563EB;}"
        "QTableView{background:white;alternate-background-color:#F1F5F9;"
        "gridline-color:#E2E8F0;}"
        "QGroupBox{font-weight:600;border:1px solid #CBD5E1;border-radius:6px;"
        "margin-top:12px;padding-top:12px;}"));
    statusBar()->showMessage(QStringLiteral("SQLite：正在初始化"));

    connect(navigation, &QListWidget::currentRowChanged,
            pages, &QStackedWidget::setCurrentIndex);
    connect(m_realtimePage, &RealtimePage::connectRequested,
            this, &MainWindow::connectRequested);
    connect(m_realtimePage, &RealtimePage::disconnectRequested,
            this, &MainWindow::disconnectRequested);
    connect(m_realtimePage, &RealtimePage::writeTargetSpeedRequested,
            this, &MainWindow::writeTargetSpeedRequested);
    connect(m_devicePage, &DevicePage::connectRequested,
            this, &MainWindow::connectRequested);
    connect(m_devicePage, &DevicePage::disconnectRequested,
            this, &MainWindow::disconnectRequested);
    connect(m_devicePage, &DevicePage::saveRequested,
            this, &MainWindow::saveDeviceRequested);
    connect(m_alarmPage, &AlarmPage::acknowledgeRequested,
            this, &MainWindow::acknowledgeRequested);
    connect(m_visionPage, &VisionPage::openVideoRequested,
            this, &MainWindow::openVisionVideoRequested);
    connect(m_visionPage, &VisionPage::playRequested,
            this, &MainWindow::playVisionVideoRequested);
    connect(m_visionPage, &VisionPage::pauseRequested,
            this, &MainWindow::pauseVisionVideoRequested);
    connect(m_visionPage, &VisionPage::stopRequested,
            this, &MainWindow::stopVisionVideoRequested);
}

void MainWindow::showVisionSource(const VisionSourceInfo &source)
{
    m_visionPage->setSourceInfo(source);
}

void MainWindow::showVisionFrame(const VisionFrameResult &frame)
{
    m_visionPage->setFrame(frame);
}

void MainWindow::setVisionPlaybackState(VisionPlaybackState state,
                                        const QString &message)
{
    m_visionPage->setPlaybackState(state, message);
}

void MainWindow::showVisionError(const QString &message)
{
    m_visionPage->showError(message);
}

void MainWindow::addProtocol(
    const industrial::protocol::ProtocolDescriptor &descriptor)
{
    m_devicePage->addProtocol(descriptor);
}

void MainWindow::showWriteResult(
    const industrial::protocol::WriteResult &result)
{
    m_realtimePage->showWriteResult(result);
}

void MainWindow::applySnapshots(
    const industrial::protocol::RealtimeSnapshotBatch &snapshots)
{
    m_realtimePage->applySnapshots(snapshots);
    QDateTime lastGoodUtc;
    for (const auto &snapshot : snapshots) {
        if (snapshot.quality == industrial::protocol::DataQuality::Good
            && (!lastGoodUtc.isValid()
                || snapshot.timestampUtc > lastGoodUtc)) {
            lastGoodUtc = snapshot.timestampUtc;
        }
    }
    if (lastGoodUtc.isValid()) {
        m_devicePage->setLastCommunicationTime(lastGoodUtc);
    }
}

void MainWindow::setDevice(const industrial::protocol::DeviceConfig &config)
{
    m_devicePage->setDevice(config);
}

void MainWindow::setDeviceState(
    const industrial::protocol::DeviceState &state)
{
    m_realtimePage->setDeviceState(state);
    m_devicePage->setDeviceState(state);
}

void MainWindow::upsertAlarm(
    const industrial::monitor::AlarmRecord &alarm)
{
    m_alarmPage->upsertAlarm(alarm);
}

void MainWindow::setStorageStatus(const QString &message, bool healthy)
{
    // 控制器会在同一区域报告数据、通信和存储状态，消息自身携带来源。
    statusBar()->showMessage(message);
    statusBar()->setStyleSheet(healthy
        ? QStringLiteral("color:#137333;")
        : QStringLiteral("color:#B42318;"));
}
