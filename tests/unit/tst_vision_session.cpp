#include <QtTest>

#include "VisionSession.h"

#include <QThread>

using namespace industrial::monitor::vision;

class VisionSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void initializesWorkerAndDeliversResultsAcrossThreads();
    void shutsDownWithFiniteWaitAndIsIdempotent();
};

void VisionSessionTest::initializesWorkerAndDeliversResultsAcrossThreads()
{
    VisionSession session;
    QSignalSpy initializedSpy(&session, &VisionSession::workerInitialized);
    QSignalSpy errorSpy(&session, &VisionSession::videoError);

    QVERIFY(session.start());
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 1'000);
    const quintptr workerThreadId = initializedSpy.constFirst().at(0).value<quintptr>();
    QVERIFY(workerThreadId != 0);
    QVERIFY(workerThreadId
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QVERIFY(session.isRunning());

    session.openVideo(QStringLiteral("/definitely/missing/session-video.avi"));
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 1'000);
    QVERIFY(errorSpy.constFirst().at(0).toString().contains(QStringLiteral("不可访问")));

    QVERIFY(session.shutdown(1'000));
    QVERIFY(!session.isRunning());
}

void VisionSessionTest::shutsDownWithFiniteWaitAndIsIdempotent()
{
    VisionSession session;
    QSignalSpy initializedSpy(&session, &VisionSession::workerInitialized);

    QVERIFY(session.start());
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 1'000);
    QVERIFY(session.shutdown(1'000));
    QVERIFY(!session.isRunning());
    QVERIFY(session.shutdown(1'000));
}

QTEST_GUILESS_MAIN(VisionSessionTest)

#include "tst_vision_session.moc"
