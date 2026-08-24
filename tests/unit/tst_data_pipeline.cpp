#include <QtTest>

#include <QSignalSpy>

#include <industrial/protocol/ProtocolTypes.h>

#include "DataPipeline.h"

using namespace industrial::protocol;

namespace {

MeasurementSample sample(double value,
                         DataQuality quality,
                         quint64 sequence,
                         const QDateTime &timestamp,
                         const QString &tagId = QStringLiteral("temperature"))
{
    return {
        QStringLiteral("plc-1"),
        tagId,
        0,
        value,
        quality,
        timestamp,
        sequence
    };
}

RealtimeSnapshot lastSnapshot(const QSignalSpy &spy)
{
    const auto batch = qvariant_cast<RealtimeSnapshotBatch>(spy.last().at(0));
    return batch.constLast();
}

} // namespace

class DataPipelineTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void aggregatesGoodSamples();
    void staleAndBadSamplesDoNotChangeStatistics();
    void reconnectMarksExistingSnapshotsStale();
    void rejectsOutOfOrderAndOutOfRangeSamples();
};

void DataPipelineTest::initTestCase()
{
    registerProtocolMetaTypes();
}

void DataPipelineTest::aggregatesGoodSamples()
{
    DataPipeline pipeline;
    QSignalSpy snapshotSpy(&pipeline, &DataPipeline::snapshotsReady);
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    pipeline.processSamples({sample(10.0, DataQuality::Good, 1, base)});
    pipeline.processSamples({sample(20.0, DataQuality::Good, 2, base.addMSecs(500))});

    QCOMPARE(snapshotSpy.count(), 2);
    const auto snapshot = lastSnapshot(snapshotSpy);
    QCOMPARE(snapshot.current, 20.0);
    QCOMPARE(snapshot.minimum, 10.0);
    QCOMPARE(snapshot.maximum, 20.0);
    QCOMPARE(snapshot.average, 15.0);
    QCOMPARE(snapshot.sampleCount, 2);
    QCOMPARE(snapshot.quality, DataQuality::Good);
}

void DataPipelineTest::staleAndBadSamplesDoNotChangeStatistics()
{
    DataPipeline pipeline;
    QSignalSpy snapshotSpy(&pipeline, &DataPipeline::snapshotsReady);
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    pipeline.processSamples({sample(10.0, DataQuality::Good, 1, base)});
    pipeline.processSamples({sample(999.0, DataQuality::Bad, 2, base.addMSecs(500))});
    auto snapshot = lastSnapshot(snapshotSpy);
    QCOMPARE(snapshot.current, 10.0);
    QCOMPARE(snapshot.average, 10.0);
    QCOMPARE(snapshot.sampleCount, 1);
    QCOMPARE(snapshot.quality, DataQuality::Bad);

    pipeline.processSamples({sample(888.0, DataQuality::Stale, 3, base.addSecs(1))});
    snapshot = lastSnapshot(snapshotSpy);
    QCOMPARE(snapshot.current, 10.0);
    QCOMPARE(snapshot.average, 10.0);
    QCOMPARE(snapshot.sampleCount, 1);
    QCOMPARE(snapshot.quality, DataQuality::Stale);
}

void DataPipelineTest::reconnectMarksExistingSnapshotsStale()
{
    DataPipeline pipeline;
    QSignalSpy snapshotSpy(&pipeline, &DataPipeline::snapshotsReady);
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);
    pipeline.processSamples({sample(42.0, DataQuality::Good, 1, base)});

    pipeline.handleDeviceState({QStringLiteral("plc-1"),
                                ConnectionState::Reconnecting,
                                QStringLiteral("连接中断")});

    QCOMPARE(snapshotSpy.count(), 2);
    const auto snapshot = lastSnapshot(snapshotSpy);
    QCOMPARE(snapshot.current, 42.0);
    QCOMPARE(snapshot.quality, DataQuality::Stale);
}

void DataPipelineTest::rejectsOutOfOrderAndOutOfRangeSamples()
{
    DataPipeline pipeline;
    QSignalSpy snapshotSpy(&pipeline, &DataPipeline::snapshotsReady);
    QSignalSpy errorSpy(&pipeline, &DataPipeline::pipelineError);
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1'000, Qt::UTC);

    pipeline.processSamples({sample(42.0, DataQuality::Good, 2, base)});
    pipeline.processSamples({sample(43.0, DataQuality::Good, 1, base.addMSecs(500))});
    pipeline.processSamples({sample(201.0, DataQuality::Good, 3, base.addSecs(1))});

    QCOMPARE(snapshotSpy.count(), 2);
    const auto badSnapshot = lastSnapshot(snapshotSpy);
    QCOMPARE(badSnapshot.current, 42.0);
    QCOMPARE(badSnapshot.sampleCount, 1);
    QCOMPARE(badSnapshot.quality, DataQuality::Bad);
    QCOMPARE(errorSpy.count(), 2);
    const auto sequenceError = qvariant_cast<DeviceError>(errorSpy.at(0).at(0));
    QCOMPARE(sequenceError.category, DeviceErrorCategory::Data);
    QVERIFY(sequenceError.message.contains(QStringLiteral("序号")));
    const auto rangeError = qvariant_cast<DeviceError>(errorSpy.at(1).at(0));
    QVERIFY(rangeError.message.contains(QStringLiteral("范围")));
}

QTEST_GUILESS_MAIN(DataPipelineTest)

#include "tst_data_pipeline.moc"
