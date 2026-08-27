#include <QtTest>

#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>

#include <industrial/protocol/IProtocolPlugin.h>

#include "DeviceSession.h"

using namespace industrial::protocol;

namespace {

struct ThreadRecord
{
    QMutex mutex;
    quintptr startThreadId = 0;
    quintptr stopThreadId = 0;
    quintptr destructionThreadId = 0;
};

class RecordingWorker final : public AbstractDeviceWorker
{
    Q_OBJECT

public:
    explicit RecordingWorker(ThreadRecord *record, int stopDelayMs)
        : m_record(record)
        , m_stopDelayMs(stopDelayMs)
    {
    }

    ~RecordingWorker() override
    {
        QMutexLocker locker(&m_record->mutex);
        m_record->destructionThreadId =
            reinterpret_cast<quintptr>(QThread::currentThreadId());
    }

public slots:
    void start() override
    {
        {
            QMutexLocker locker(&m_record->mutex);
            m_record->startThreadId =
                reinterpret_cast<quintptr>(QThread::currentThreadId());
        }
        emit stateChanged({QStringLiteral("recording-device"),
                           ConnectionState::Online,
                           {}});
    }

    void stop() override
    {
        {
            QMutexLocker locker(&m_record->mutex);
            m_record->stopThreadId =
                reinterpret_cast<quintptr>(QThread::currentThreadId());
        }
        MeasurementSample finalSample;
        finalSample.deviceId = QStringLiteral("recording-device");
        finalSample.tagId = QStringLiteral("temperature");
        finalSample.quality = DataQuality::Good;
        finalSample.timestampUtc = QDateTime::currentDateTimeUtc();
        finalSample.sequence = 1;
        emit samplesReady({finalSample});
        const auto finish = [this]() {
            emit stateChanged({QStringLiteral("recording-device"),
                               ConnectionState::Stopped,
                               {}});
            emit stopped();
        };
        if (m_stopDelayMs > 0) {
            QTimer::singleShot(m_stopDelayMs, this, finish);
        } else {
            finish();
        }
    }

    void writeValue(const WriteRequest &request) override
    {
        emit writeFinished({request.requestId, true, {}});
    }

private:
    ThreadRecord *m_record;
    int m_stopDelayMs = 0;
};

class RecordingPlugin final : public IProtocolPlugin
{
public:
    explicit RecordingPlugin(ThreadRecord *record, int stopDelayMs = 0)
        : m_record(record)
        , m_stopDelayMs(stopDelayMs)
    {
    }

    ProtocolDescriptor descriptor() const override
    {
        return {QStringLiteral("recording"),
                QStringLiteral("Recording"),
                1,
                {}};
    }

    AbstractDeviceWorker *createWorker(const DeviceConfig &) override
    {
        return new RecordingWorker(m_record, m_stopDelayMs);
    }

private:
    ThreadRecord *m_record;
    int m_stopDelayMs = 0;
};

} // namespace

class DeviceSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void runsWorkerLifecycleInDedicatedThread();
    void stopWaitProcessesFinalQueuedSampleAndReportsTimeout();
    void reportsMissingPluginWithoutStartingThread();
};

void DeviceSessionTest::initTestCase()
{
    registerProtocolMetaTypes();
}

void DeviceSessionTest::runsWorkerLifecycleInDedicatedThread()
{
    ThreadRecord record;
    RecordingPlugin plugin(&record);
    DeviceConfig config;
    config.id = QStringLiteral("recording-device");
    DeviceSession session(&plugin, config);
    QSignalSpy stateSpy(&session, &DeviceSession::stateChanged);
    QSignalSpy sampleSpy(&session, &DeviceSession::samplesReady);

    QVERIFY(session.start());
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.count(), 1, 1'000);
    QVERIFY(session.isRunning());
    QVERIFY(session.stopAndWait(1'000));
    QVERIFY(!session.isRunning());
    QCOMPARE(sampleSpy.count(), 1);

    QMutexLocker locker(&record.mutex);
    QVERIFY(record.startThreadId != 0);
    QCOMPARE(record.stopThreadId, record.startThreadId);
    QCOMPARE(record.destructionThreadId, record.startThreadId);
    QVERIFY(record.startThreadId
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

void DeviceSessionTest::stopWaitProcessesFinalQueuedSampleAndReportsTimeout()
{
    ThreadRecord record;
    RecordingPlugin plugin(&record, 150);
    DeviceConfig config;
    config.id = QStringLiteral("recording-device");
    DeviceSession session(&plugin, config);
    QSignalSpy sampleSpy(&session, &DeviceSession::samplesReady);
    QSignalSpy errorSpy(&session, &DeviceSession::communicationError);

    QVERIFY(session.start());
    QTRY_VERIFY_WITH_TIMEOUT(session.isRunning(), 500);
    QVERIFY(!session.stopAndWait(20));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(sampleSpy.count(), 1);
    QVERIFY(session.stopAndWait(1'000));
    QVERIFY(!session.isRunning());
}

void DeviceSessionTest::reportsMissingPluginWithoutStartingThread()
{
    DeviceConfig config;
    config.id = QStringLiteral("missing-plugin-device");
    DeviceSession session(nullptr, config);
    QSignalSpy errorSpy(&session, &DeviceSession::communicationError);

    QVERIFY(!session.start());
    QCOMPARE(errorSpy.count(), 1);
    const auto error = qvariant_cast<DeviceError>(errorSpy.constFirst().at(0));
    QCOMPARE(error.category, DeviceErrorCategory::Lifecycle);
    QVERIFY(!error.recoverable);
    QVERIFY(!session.isRunning());
}

QTEST_GUILESS_MAIN(DeviceSessionTest)

#include "tst_device_session.moc"
