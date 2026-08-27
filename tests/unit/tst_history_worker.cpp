#include <QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include "DatabaseMigrator.h"
#include "HistoryWorker.h"

using namespace industrial::monitor;
using namespace industrial::protocol;

namespace {

bool prepareHistory(const QString &path)
{
    const QString connectionName =
        QStringLiteral("history-seed-%1").arg(QUuid::createUuid().toString());
    bool success = false;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                   connectionName);
        database.setDatabaseName(path);
        if (database.open() && DatabaseMigrator::migrate(database).success) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "INSERT INTO measurement "
                "(device_id,tag_id,value,quality,timestamp_utc,sequence) "
                "VALUES ('virtual-plc-1','temperature',:value,0,:time,:sequence)"));
            success = true;
            for (int index = 0; index < 3; ++index) {
                query.bindValue(QStringLiteral(":value"), 40.0 + index);
                query.bindValue(QStringLiteral(":time"),
                                1'700'000'000'000LL + index * 1'000);
                query.bindValue(QStringLiteral(":sequence"), index + 1);
                if (!query.exec()) {
                    success = false;
                    break;
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

} // namespace

class HistoryWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void queriesInReadThreadWithValidationAndPagination();
};

void HistoryWorkerTest::initTestCase()
{
    registerStorageMetaTypes();
}

void HistoryWorkerTest::queriesInReadThreadWithValidationAndPagination()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("history.db"));
    QVERIFY(prepareHistory(path));

    QThread thread;
    auto *worker = new HistoryWorker;
    worker->moveToThread(&thread);
    QSignalSpy readySpy(worker, &HistoryWorker::ready);
    QSignalSpy resultSpy(worker, &HistoryWorker::queryFinished);
    QSignalSpy stoppedSpy(worker, &HistoryWorker::stopped);
    connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.start();

    const quintptr mainThreadId =
        reinterpret_cast<quintptr>(QThread::currentThreadId());
    QVERIFY(QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                                      Q_ARG(QString, path)));
    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 2'000);
    QVERIFY(readySpy.constFirst().at(0).value<quintptr>() != mainThreadId);

    HistoryQuery invalid;
    invalid.requestId = 1;
    invalid.deviceId = QStringLiteral("virtual-plc-1");
    invalid.tagId = QStringLiteral("temperature");
    invalid.fromUtc = QDateTime::fromMSecsSinceEpoch(1'700'000'000'000LL,
                                                     Qt::UTC);
    invalid.toUtc = invalid.fromUtc.addSecs(25 * 60 * 60);
    QVERIFY(QMetaObject::invokeMethod(worker, "query", Qt::QueuedConnection,
                                      Q_ARG(industrial::monitor::HistoryQuery,
                                            invalid)));
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 1, 2'000);
    auto invalidResult =
        qvariant_cast<HistoryQueryResult>(resultSpy.constFirst().at(0));
    QVERIFY(!invalidResult.success);
    QVERIFY(invalidResult.errorMessage.contains(QStringLiteral("24")));

    HistoryQuery firstPage;
    firstPage.requestId = 2;
    firstPage.deviceId = QStringLiteral("virtual-plc-1");
    firstPage.tagId = QStringLiteral("temperature");
    firstPage.fromUtc = invalid.fromUtc;
    firstPage.toUtc = invalid.fromUtc.addSecs(10);
    firstPage.limit = 2;
    QVERIFY(QMetaObject::invokeMethod(worker, "query", Qt::QueuedConnection,
                                      Q_ARG(industrial::monitor::HistoryQuery,
                                            firstPage)));
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 2, 2'000);
    const auto page =
        qvariant_cast<HistoryQueryResult>(resultSpy.constLast().at(0));
    QVERIFY2(page.success, qPrintable(page.errorMessage));
    QCOMPARE(page.points.size(), 2);
    QVERIFY(page.hasMore);
    QCOMPARE(page.points.at(0).value, 40.0);
    QCOMPARE(page.points.at(1).value, 41.0);

    firstPage.requestId = 3;
    firstPage.offset = 2;
    QVERIFY(QMetaObject::invokeMethod(worker, "query", Qt::QueuedConnection,
                                      Q_ARG(industrial::monitor::HistoryQuery,
                                            firstPage)));
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 3, 2'000);
    const auto finalPage =
        qvariant_cast<HistoryQueryResult>(resultSpy.constLast().at(0));
    QVERIFY(finalPage.success);
    QCOMPARE(finalPage.points.size(), 1);
    QVERIFY(!finalPage.hasMore);
    QCOMPARE(finalPage.points.constFirst().value, 42.0);

    QVERIFY(QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 2'000);
    thread.quit();
    QVERIFY(thread.wait(2'000));
}

QTEST_GUILESS_MAIN(HistoryWorkerTest)

#include "tst_history_worker.moc"
