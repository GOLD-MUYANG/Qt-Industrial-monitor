#include <QtTest>

#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>

#include "AlarmPage.h"
#include "DevicePage.h"
#include "MainWindow.h"
#include "RealtimePage.h"
#include "VisionPage.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

RealtimeSnapshot temperature(double value, quint64 sequence)
{
    RealtimeSnapshot result;
    result.deviceId = QStringLiteral("virtual-plc-1");
    result.tagId = QStringLiteral("temperature");
    result.current = value;
    result.minimum = value;
    result.maximum = value;
    result.average = value;
    result.sampleCount = static_cast<int>(sequence);
    result.quality = DataQuality::Good;
    result.timestampUtc = QDateTime::fromMSecsSinceEpoch(
        1'700'000'000'000LL + static_cast<qint64>(sequence) * 500,
        Qt::UTC);
    result.sequence = sequence;
    return result;
}

} // namespace

class MonitorWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesFourAccessiblePagesAndRetainsPausedChartData();
    void requestsTargetSpeedWriteAndShowsResult();
    void deviceEditorUsesLoadedProtocolAndEnabledState();
};

void MonitorWindowTest::exposesFourAccessiblePagesAndRetainsPausedChartData()
{
    MainWindow window;
    auto *realtime = window.findChild<RealtimePage *>(QStringLiteral("realtimePage"));
    auto *device = window.findChild<DevicePage *>(QStringLiteral("devicePage"));
    auto *alarm = window.findChild<AlarmPage *>(QStringLiteral("alarmPage"));
    auto *vision = window.findChild<VisionPage *>(QStringLiteral("visionPage"));
    QVERIFY(realtime);
    QVERIFY(device);
    QVERIFY(alarm);
    QVERIFY(vision);
    auto *navigation = window.findChild<QListWidget *>(QStringLiteral("mainNavigation"));
    QVERIFY(navigation);
    QCOMPARE(navigation->count(), 4);
    QCOMPARE(navigation->item(3)->text(), QStringLiteral("视觉实验"));

    auto *hostEdit =
        window.findChild<QLineEdit *>(QStringLiteral("deviceHostEdit"));
    QVERIFY(hostEdit);
    QVERIFY(!hostEdit->accessibleName().isEmpty());

    window.applySnapshots({temperature(42.0, 1)});
    window.applySnapshots({temperature(42.1, 2)});
    QCOMPARE(realtime->seriesPointCount(QStringLiteral("temperature")), 2);

    realtime->setPaused(true);
    window.applySnapshots({temperature(42.2, 3)});
    QCOMPARE(realtime->seriesPointCount(QStringLiteral("temperature")), 2);
    realtime->setPaused(false);
    QCOMPARE(realtime->seriesPointCount(QStringLiteral("temperature")), 3);
}

void MonitorWindowTest::deviceEditorUsesLoadedProtocolAndEnabledState()
{
    MainWindow window;
    const ProtocolDescriptor descriptor{
        QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP"), 1, {}};
    window.addProtocol(descriptor);
    DeviceConfig config;
    config.id = QStringLiteral("virtual-plc-1");
    config.protocolKey = descriptor.key;
    config.enabled = false;
    window.setDevice(config);

    auto *protocol = window.findChild<QComboBox *>(
        QStringLiteral("deviceProtocolCombo"));
    auto *enabled = window.findChild<QCheckBox *>(
        QStringLiteral("deviceEnabledCheck"));
    QVERIFY(protocol);
    QVERIFY(enabled);
    QCOMPARE(protocol->currentData().toString(), descriptor.key);
    QCOMPARE(protocol->currentText(), descriptor.displayName);
    QVERIFY(!enabled->isChecked());
}

void MonitorWindowTest::requestsTargetSpeedWriteAndShowsResult()
{
    MainWindow window;
    auto *targetSpeed =
        window.findChild<QSpinBox *>(QStringLiteral("targetSpeedSpin"));
    auto *writeButton = window.findChild<QPushButton *>(
        QStringLiteral("writeTargetSpeedButton"));
    auto *resultLabel = window.findChild<QLabel *>(
        QStringLiteral("writeTargetSpeedResult"));
    QVERIFY(targetSpeed);
    QVERIFY(writeButton);
    QVERIFY(resultLabel);

    QSignalSpy writeSpy(&window, &MainWindow::writeTargetSpeedRequested);
    targetSpeed->setValue(1'800);
    QTest::mouseClick(writeButton, Qt::LeftButton);
    QCOMPARE(writeSpy.count(), 1);
    QCOMPARE(writeSpy.takeFirst().at(0).toUInt(), 1'800U);

    window.showWriteResult({1, true, {}});
    QVERIFY(resultLabel->text().contains(QStringLiteral("成功")));
    window.showWriteResult({2, false, QStringLiteral("设备离线")});
    QVERIFY(resultLabel->text().contains(QStringLiteral("设备离线")));
}

QTEST_MAIN(MonitorWindowTest)

#include "tst_monitor_window.moc"
