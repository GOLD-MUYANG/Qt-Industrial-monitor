#include "RealtimePage.h"

#include "RealtimeTableModel.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

using namespace industrial::protocol;

RealtimePage::RealtimePage(QWidget *parent)
    : QWidget(parent)
    , m_model(new RealtimeTableModel(this))
{
    setObjectName(QStringLiteral("realtimePage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // 顶部操作区：状态始终使用明确文字，颜色只作辅助。
    auto *toolbar = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("实时监控"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    m_stateLabel = new QLabel(QStringLiteral("已停止 (Stopped)"), this);
    m_stateLabel->setObjectName(QStringLiteral("realtimeStateLabel"));
    m_stateLabel->setAccessibleName(QStringLiteral("设备连接状态"));
    auto *connectButton = new QPushButton(QStringLiteral("连接设备"), this);
    auto *disconnectButton = new QPushButton(QStringLiteral("断开连接"), this);
    m_pauseButton = new QPushButton(QStringLiteral("暂停显示"), this);
    m_pauseButton->setCheckable(true);
    toolbar->addWidget(title);
    toolbar->addSpacing(16);
    toolbar->addWidget(m_stateLabel);
    toolbar->addStretch();
    toolbar->addWidget(connectButton);
    toolbar->addWidget(disconnectButton);
    toolbar->addWidget(m_pauseButton);
    root->addLayout(toolbar);

    // 数据表是曲线的可访问替代视图，同时明确 60 秒统计含义。
    auto *windowLabel =
        new QLabel(QStringLiteral("当前值与最近 60 秒滚动统计"), this);
    root->addWidget(windowLabel);
    auto *table = new QTableView(this);
    table->setObjectName(QStringLiteral("realtimeTable"));
    table->setModel(m_model);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(210);
    root->addWidget(table);

    // 可写控制与只读采样分区展示，写结果必须给出文字反馈。
    auto *writeLayout = new QHBoxLayout;
    writeLayout->addWidget(new QLabel(QStringLiteral("目标转速："), this));
    m_targetSpeedSpin = new QSpinBox(this);
    m_targetSpeedSpin->setObjectName(QStringLiteral("targetSpeedSpin"));
    m_targetSpeedSpin->setAccessibleName(QStringLiteral("目标转速"));
    m_targetSpeedSpin->setRange(0, 6'000);
    m_targetSpeedSpin->setValue(1'500);
    m_targetSpeedSpin->setSuffix(QStringLiteral(" rpm"));
    auto *writeButton = new QPushButton(QStringLiteral("写入目标转速"), this);
    writeButton->setObjectName(QStringLiteral("writeTargetSpeedButton"));
    m_writeResultLabel = new QLabel(QStringLiteral("尚未写入"), this);
    m_writeResultLabel->setObjectName(QStringLiteral("writeTargetSpeedResult"));
    m_writeResultLabel->setAccessibleName(QStringLiteral("目标转速写入结果"));
    writeLayout->addWidget(m_targetSpeedSpin);
    writeLayout->addWidget(writeButton);
    writeLayout->addWidget(m_writeResultLabel);
    writeLayout->addStretch();
    root->addLayout(writeLayout);

    auto *chartHeader = new QHBoxLayout;
    chartHeader->addWidget(new QLabel(QStringLiteral("最近 60 秒趋势"), this));
    chartHeader->addStretch();
    chartHeader->addWidget(new QLabel(QStringLiteral("测点："), this));
    m_tagSelector = new QComboBox(this);
    m_tagSelector->addItem(QStringLiteral("温度 / ℃"),
                           QStringLiteral("temperature"));
    m_tagSelector->addItem(QStringLiteral("压力 / MPa"),
                           QStringLiteral("pressure"));
    m_tagSelector->addItem(QStringLiteral("转速 / rpm"),
                           QStringLiteral("speed"));
    m_tagSelector->addItem(QStringLiteral("电压 / V"),
                           QStringLiteral("voltage"));
    m_tagSelector->setAccessibleName(QStringLiteral("实时曲线测点"));
    chartHeader->addWidget(m_tagSelector);
    root->addLayout(chartHeader);

    auto *chart = new QChart;
    chart->legend()->hide();
    chart->setTitle(QStringLiteral("温度 / ℃"));
    m_series = new QLineSeries(chart);
    chart->addSeries(m_series);
    m_timeAxis = new QDateTimeAxis(chart);
    m_timeAxis->setFormat(QStringLiteral("HH:mm:ss"));
    m_timeAxis->setTitleText(QStringLiteral("本地时间"));
    m_valueAxis = new QValueAxis(chart);
    m_valueAxis->setTitleText(QStringLiteral("工程值"));
    m_valueAxis->setRange(0.0, 100.0);
    chart->addAxis(m_timeAxis, Qt::AlignBottom);
    chart->addAxis(m_valueAxis, Qt::AlignLeft);
    m_series->attachAxis(m_timeAxis);
    m_series->attachAxis(m_valueAxis);
    auto *chartView = new QChartView(chart, this);
    chartView->setObjectName(QStringLiteral("realtimeChart"));
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(260);
    root->addWidget(chartView, 1);

    connect(connectButton, &QPushButton::clicked,
            this, &RealtimePage::connectRequested);
    connect(disconnectButton, &QPushButton::clicked,
            this, &RealtimePage::disconnectRequested);
    connect(writeButton, &QPushButton::clicked, this, [this]() {
        m_writeResultLabel->setText(QStringLiteral("正在写入…"));
        m_writeResultLabel->setStyleSheet(QStringLiteral("color:#475569;"));
        emit writeTargetSpeedRequested(
            static_cast<quint16>(m_targetSpeedSpin->value()));
    });
    connect(m_pauseButton, &QPushButton::toggled,
            this, &RealtimePage::setPaused);
    connect(m_tagSelector, &QComboBox::currentIndexChanged,
            this, [this, chart](int) {
                chart->setTitle(m_tagSelector->currentText());
                refreshSeries();
            });
}

void RealtimePage::showWriteResult(const WriteResult &result)
{
    if (result.success) {
        m_writeResultLabel->setText(QStringLiteral("写入成功"));
        m_writeResultLabel->setStyleSheet(
            QStringLiteral("color:#137333;font-weight:600;"));
        return;
    }
    m_writeResultLabel->setText(
        QStringLiteral("写入失败：%1").arg(result.errorMessage));
    m_writeResultLabel->setStyleSheet(
        QStringLiteral("color:#B42318;font-weight:600;"));
}

int RealtimePage::seriesPointCount(const QString &tagId) const
{
    if (m_tagSelector->currentData().toString() != tagId) {
        return 0;
    }
    return m_series->count();
}

bool RealtimePage::isPaused() const
{
    return m_paused;
}

void RealtimePage::applySnapshots(const RealtimeSnapshotBatch &snapshots)
{
    for (const auto &snapshot : snapshots) {
        appendHistory(snapshot);
        bool replaced = false;
        for (auto &latest : m_latestSnapshots) {
            if (latest.tagId == snapshot.tagId) {
                latest = snapshot;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            m_latestSnapshots.append(snapshot);
        }
    }
    if (!m_paused) {
        m_model->applySnapshots(snapshots);
        refreshSeries();
    }
}

void RealtimePage::setDeviceState(const DeviceState &state)
{
    m_stateLabel->setText(stateText(state.connectionState));
    const bool online = state.connectionState == ConnectionState::Online;
    m_stateLabel->setStyleSheet(online
        ? QStringLiteral("color:#137333;font-weight:600;")
        : state.connectionState == ConnectionState::Reconnecting
            ? QStringLiteral("color:#9A6700;font-weight:600;")
            : QStringLiteral("color:#475569;font-weight:600;"));
}

void RealtimePage::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    {
        const QSignalBlocker blocker(m_pauseButton);
        m_pauseButton->setChecked(paused);
    }
    m_pauseButton->setText(paused
        ? QStringLiteral("恢复显示")
        : QStringLiteral("暂停显示"));
    if (!paused) {
        m_model->applySnapshots(m_latestSnapshots);
        refreshSeries();
    }
    emit pauseChanged(paused);
}

bool RealtimePage::isNumericTag(const QString &tagId)
{
    return tagId == QStringLiteral("temperature")
        || tagId == QStringLiteral("pressure")
        || tagId == QStringLiteral("speed")
        || tagId == QStringLiteral("voltage");
}

QString RealtimePage::stateText(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Stopped:
        return QStringLiteral("已停止 (Stopped)");
    case ConnectionState::Connecting:
        return QStringLiteral("连接中 (Connecting)");
    case ConnectionState::Online:
        return QStringLiteral("在线 (Online)");
    case ConnectionState::Reconnecting:
        return QStringLiteral("重连中 (Reconnecting)");
    case ConnectionState::Stopping:
        return QStringLiteral("停止中 (Stopping)");
    case ConnectionState::Faulted:
        return QStringLiteral("故障 (Faulted)");
    }
    return QStringLiteral("未知");
}

void RealtimePage::appendHistory(const RealtimeSnapshot &snapshot)
{
    if (!isNumericTag(snapshot.tagId)
        || snapshot.quality != DataQuality::Good
        || !snapshot.timestampUtc.isValid()) {
        return;
    }
    auto &points = m_history[snapshot.tagId];
    points.append(QPointF(snapshot.timestampUtc.toMSecsSinceEpoch(),
                          snapshot.current));
    const qreal cutoff = snapshot.timestampUtc.addSecs(-60).toMSecsSinceEpoch();
    while (!points.isEmpty()
           && (points.constFirst().x() < cutoff || points.size() > 120)) {
        points.removeFirst();
    }
}

void RealtimePage::refreshSeries()
{
    const QString tagId = m_tagSelector->currentData().toString();
    const auto points = m_history.value(tagId);
    m_series->replace(points);
    if (points.isEmpty()) {
        const QDateTime now = QDateTime::currentDateTime();
        m_timeAxis->setRange(now.addSecs(-60), now);
        m_valueAxis->setRange(0.0, 100.0);
        return;
    }

    const qreal firstTime = points.constFirst().x();
    const qreal lastTime = points.constLast().x();
    m_timeAxis->setRange(QDateTime::fromMSecsSinceEpoch(
                             static_cast<qint64>(std::min(firstTime,
                                                         lastTime - 1'000))),
                         QDateTime::fromMSecsSinceEpoch(
                             static_cast<qint64>(lastTime)));
    qreal minimum = points.constFirst().y();
    qreal maximum = minimum;
    for (const auto &point : points) {
        minimum = std::min(minimum, point.y());
        maximum = std::max(maximum, point.y());
    }
    const qreal padding = std::max<qreal>(1.0, (maximum - minimum) * 0.1);
    m_valueAxis->setRange(minimum - padding, maximum + padding);
}
