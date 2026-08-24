#include <QtTest>

#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QThread>

#include <industrial/protocol/IProtocolPlugin.h>

#include "DeviceSession.h"

using namespace industrial::protocol;

namespace {

struct ThreadRecord
{
    QMutex mutex;
    QThread *startThread = nullptr;
    QThread *stopThread = nullptr;
    QThread *destructionThread = nullptr;
};

class RecordingWorker final : public AbstractDeviceWorker
{
    Q_OBJECT

public:
    explicit RecordingWorker(ThreadRecord *record)
        : m_record(record)
    {
    }

    ~RecordingWorker() override
    {
        QMutexLocker locker(&m_record->mutex);
        m_record->destructionThread = QThread::currentThread();
    }

public slots:
    void start() override
    {
        {
            QMutexLocker locker(&m_record->mutex);
            m_record->startThread = QThread::currentThread();
        }
        emit stateChanged({QStringLiteral("recording-device"),
                           ConnectionState::Online,
                           {}});
    }

    void stop() override
    {
        {
            QMutexLocker locker(&m_record->mutex);
            m_record->stopThread = QThread::currentThread();
        }
        emit stateChanged({QStringLiteral("recording-device"),
                           ConnectionState::Stopped,
                           {}});
        emit stopped();
    }

    void writeValue(const WriteRequest &request) override
    {
        emit writeFinished({request.requestId, true, {}});
    }

private:
    ThreadRecord *m_record;
};

class RecordingPlugin final : public IProtocolPlugin
{
public:
    explicit RecordingPlugin(ThreadRecord *record)
        : m_record(record)
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
        return new RecordingWorker(m_record);
    }

private:
    ThreadRecord *m_record;
};

} // namespace

class DeviceSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void runsWorkerLifecycleInDedicatedThread();
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

    QVERIFY(session.start());
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.count(), 1, 1'000);
    QVERIFY(session.isRunning());
    QVERIFY(session.stopAndWait(1'000));
    QVERIFY(!session.isRunning());

    QMutexLocker locker(&record.mutex);
    QVERIFY(record.startThread != nullptr);
    QCOMPARE(record.stopThread, record.startThread);
    QCOMPARE(record.destructionThread, record.startThread);
    QVERIFY(record.startThread != QThread::currentThread());
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
