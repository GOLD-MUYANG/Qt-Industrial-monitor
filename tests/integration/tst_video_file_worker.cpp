#include <QtTest>

#include "VideoFileWorker.h"

#include <QFile>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>

#include <opencv2/imgproc.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/videoio.hpp>

using namespace industrial::monitor::vision;

namespace {

class WorkerHarness final
{
public:
    WorkerHarness()
        : worker(new VideoFileWorker)
    {
        thread.setObjectName(QStringLiteral("vision-worker-test"));
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::started,
                         worker, &VideoFileWorker::initialize,
                         Qt::QueuedConnection);
        QObject::connect(worker, &VideoFileWorker::shutdownFinished,
                         &thread, &QThread::quit,
                         Qt::DirectConnection);
        QObject::connect(&thread, &QThread::finished,
                         worker, &QObject::deleteLater);
    }

    ~WorkerHarness()
    {
        if (thread.isRunning() && worker) {
            QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
            thread.wait(2'000);
        }
    }

    void start()
    {
        thread.start();
    }

    QThread thread;
    QPointer<VideoFileWorker> worker;
};

bool createMjpegVideo(const QString &path, int frameCount = 24)
{
    cv::VideoWriter writer(path.toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           20.0, cv::Size(160, 120), true);
    if (!writer.isOpened()) {
        return false;
    }
    for (int index = 0; index < frameCount; ++index) {
        cv::Mat frame = cv::Mat::zeros(120, 160, CV_8UC3);
        cv::rectangle(frame, cv::Rect(20 + index, 35, 50, 45),
                      cv::Scalar(0, 0, 255), cv::FILLED);
        writer.write(frame);
    }
    writer.release();
    return QFileInfo(path).size() > 0;
}

bool containsState(const QSignalSpy &spy, VisionPlaybackState expected)
{
    for (const auto &arguments : spy) {
        if (qvariant_cast<VisionPlaybackState>(arguments.at(0)) == expected) {
            return true;
        }
    }
    return false;
}

} // namespace

class VideoFileWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void rejectsMissingAndUndecodableFiles();
    void playsPausesResumesStopsAndReopensInWorkerThread();

private:
    int m_originalOpenCvLogLevel = 0;
};

void VideoFileWorkerTest::initTestCase()
{
    registerVisionMetaTypes();
    m_originalOpenCvLogLevel = static_cast<int>(cv::utils::logging::getLogLevel());
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
}

void VideoFileWorkerTest::cleanupTestCase()
{
    cv::utils::logging::setLogLevel(
        static_cast<cv::utils::logging::LogLevel>(m_originalOpenCvLogLevel));
}

void VideoFileWorkerTest::rejectsMissingAndUndecodableFiles()
{
    WorkerHarness harness;
    QSignalSpy initializedSpy(harness.worker, &VideoFileWorker::initialized);
    QSignalSpy errorSpy(harness.worker, &VideoFileWorker::videoError);
    harness.start();
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 1'000);

    QMetaObject::invokeMethod(harness.worker, "openVideo", Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("/definitely/missing/video.avi")));
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 1'000);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString fakeVideo = temporaryDirectory.filePath(QStringLiteral("not-video.avi"));
    QFile file(fakeVideo);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not a video"), 11LL);
    file.close();

    QMetaObject::invokeMethod(harness.worker, "openVideo", Qt::QueuedConnection,
                              Q_ARG(QString, fakeVideo));
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 2, 1'000);
}

void VideoFileWorkerTest::playsPausesResumesStopsAndReopensInWorkerThread()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("red-target.avi"));
    if (!createMjpegVideo(videoPath)) {
        QSKIP("当前 OpenCV 构建没有可用的 MJPEG VideoWriter 后端");
    }

    WorkerHarness harness;
    QSignalSpy initializedSpy(harness.worker, &VideoFileWorker::initialized);
    QSignalSpy sourceSpy(harness.worker, &VideoFileWorker::sourceOpened);
    QSignalSpy frameSpy(harness.worker, &VideoFileWorker::frameReady);
    QSignalSpy stateSpy(harness.worker, &VideoFileWorker::stateChanged);
    quintptr directEmissionThreadId = 0;
    quintptr queuedReceiverThreadId = 0;
    QObject::connect(harness.worker, &VideoFileWorker::frameReady,
                     harness.worker,
                     [&directEmissionThreadId](const VisionFrameResult &) {
                         directEmissionThreadId = reinterpret_cast<quintptr>(
                             QThread::currentThreadId());
                     },
                     Qt::DirectConnection);
    QObject::connect(harness.worker, &VideoFileWorker::frameReady,
                     this,
                     [&queuedReceiverThreadId](const VisionFrameResult &) {
                         queuedReceiverThreadId = reinterpret_cast<quintptr>(
                             QThread::currentThreadId());
                     },
                     Qt::QueuedConnection);
    harness.start();
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 1'000);

    QMetaObject::invokeMethod(harness.worker, "openVideo", Qt::QueuedConnection,
                              Q_ARG(QString, videoPath));
    QTRY_COMPARE_WITH_TIMEOUT(sourceSpy.count(), 1, 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(frameSpy.count(), 1, 2'000);
    QVERIFY(containsState(stateSpy, VisionPlaybackState::Ready));
    QCOMPARE(qvariant_cast<VisionFrameResult>(frameSpy.constFirst().at(0)).targetCount, 1);

    QMetaObject::invokeMethod(harness.worker, "play", Qt::QueuedConnection);
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 4, 2'000);
    QVERIFY(containsState(stateSpy, VisionPlaybackState::Playing));

    QMetaObject::invokeMethod(harness.worker, "pause", Qt::QueuedConnection);
    QTRY_VERIFY_WITH_TIMEOUT(containsState(stateSpy, VisionPlaybackState::Paused), 1'000);
    QTest::qWait(80);
    const int pausedFrameCount = frameSpy.count();
    QTest::qWait(180);
    QCOMPARE(frameSpy.count(), pausedFrameCount);

    QMetaObject::invokeMethod(harness.worker, "play", Qt::QueuedConnection);
    QTRY_VERIFY_WITH_TIMEOUT(containsState(stateSpy, VisionPlaybackState::Finished), 3'000);
    QVERIFY(directEmissionThreadId != 0);
    QVERIFY(directEmissionThreadId
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QCOMPARE(queuedReceiverThreadId,
             reinterpret_cast<quintptr>(QThread::currentThreadId()));

    const int framesBeforeReplay = frameSpy.count();
    QMetaObject::invokeMethod(harness.worker, "play", Qt::QueuedConnection);
    QTRY_COMPARE_WITH_TIMEOUT(sourceSpy.count(), 2, 2'000);
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > framesBeforeReplay, 2'000);

    QMetaObject::invokeMethod(harness.worker, "stop", Qt::QueuedConnection);
    QTRY_VERIFY_WITH_TIMEOUT(containsState(stateSpy, VisionPlaybackState::Idle), 1'000);
    QMetaObject::invokeMethod(harness.worker, "openVideo", Qt::QueuedConnection,
                              Q_ARG(QString, videoPath));
    QTRY_COMPARE_WITH_TIMEOUT(sourceSpy.count(), 3, 2'000);
    QVERIFY(frameSpy.count() > pausedFrameCount);

    QSignalSpy shutdownSpy(harness.worker, &VideoFileWorker::shutdownFinished);
    QMetaObject::invokeMethod(harness.worker, "shutdown", Qt::QueuedConnection);
    QTRY_COMPARE_WITH_TIMEOUT(shutdownSpy.count(), 1, 1'000);
    QVERIFY(harness.thread.wait(1'000));
}

QTEST_GUILESS_MAIN(VideoFileWorkerTest)

#include "tst_video_file_worker.moc"
