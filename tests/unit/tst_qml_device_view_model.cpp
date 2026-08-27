#include "QmlDeviceViewModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace industrial::protocol;

class QmlDeviceViewModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void projectsDeviceConfiguration();
    void acceptsOnlyMatchingDeviceStateAndValidCommunicationTime();
};

void QmlDeviceViewModelTest::projectsDeviceConfiguration()
{
    QmlDeviceViewModel model;
    QSignalSpy changedSpy(&model, &QmlDeviceViewModel::deviceChanged);
    DeviceConfig config;
    config.id = QStringLiteral("device-1");
    config.name = QStringLiteral("一号设备");
    config.protocolKey = QStringLiteral("modbus-tcp");
    config.host = QStringLiteral("192.0.2.10");
    config.port = 1502;
    config.unitId = 7;
    config.pollIntervalMs = 800;
    config.timeoutMs = 1200;
    config.enabled = false;

    model.setProtocolDisplayName(QStringLiteral("modbus-tcp"),
                                 QStringLiteral("Modbus TCP"));
    model.setDevice(config);

    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(model.deviceId(), config.id);
    QCOMPARE(model.deviceName(), config.name);
    QCOMPARE(model.protocolKey(), config.protocolKey);
    QCOMPARE(model.protocolName(), QStringLiteral("Modbus TCP"));
    QCOMPARE(model.host(), config.host);
    QCOMPARE(model.port(), 1502);
    QCOMPARE(model.unitId(), 7);
    QCOMPARE(model.pollIntervalMs(), 800);
    QCOMPARE(model.timeoutMs(), 1200);
    QCOMPARE(model.enabled(), false);
}

void QmlDeviceViewModelTest::acceptsOnlyMatchingDeviceStateAndValidCommunicationTime()
{
    QmlDeviceViewModel model;
    DeviceConfig config;
    config.id = QStringLiteral("device-1");
    model.setDevice(config);

    model.setState({QStringLiteral("other"), ConnectionState::Online,
                    QStringLiteral("错误设备")});
    QCOMPARE(model.connectionState(), static_cast<int>(ConnectionState::Stopped));

    model.setState({config.id, ConnectionState::Reconnecting,
                    QStringLiteral("等待重连")});
    QCOMPARE(model.connectionState(), static_cast<int>(ConnectionState::Reconnecting));
    QCOMPARE(model.connectionStateText(), QStringLiteral("重连中 (Reconnecting)"));
    QCOMPARE(model.connectionMessage(), QStringLiteral("等待重连"));

    model.setLastCommunicationTime({});
    QCOMPARE(model.lastCommunicationText(), QStringLiteral("尚无数据"));
    model.setLastCommunicationTime(
        QDateTime::fromString(QStringLiteral("2026-08-27T08:00:00Z"), Qt::ISODate));
    QVERIFY(model.lastCommunicationText() != QStringLiteral("尚无数据"));
}

QTEST_GUILESS_MAIN(QmlDeviceViewModelTest)

#include "tst_qml_device_view_model.moc"
