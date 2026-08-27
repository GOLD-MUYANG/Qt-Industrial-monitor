#include <QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "DatabaseMigrator.h"

namespace {

QString scalarText(QSqlDatabase &database, const QString &sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next()) {
        return {};
    }
    return query.value(0).toString();
}

int scalarInt(QSqlDatabase &database, const QString &sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace

class DatabaseMigratorTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsDocumentedSchemaAndDefaultsIdempotently();
};

void DatabaseMigratorTest::createsDocumentedSchemaAndDefaultsIdempotently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString connectionName =
        QStringLiteral("migrator-%1").arg(QUuid::createUuid().toString());

    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                   connectionName);
        database.setDatabaseName(directory.filePath(QStringLiteral("monitor.db")));
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        const auto first = DatabaseMigrator::migrate(database);
        QVERIFY2(first.success, qPrintable(first.errorMessage));
        QCOMPARE(first.schemaVersion, 1);
        const auto second = DatabaseMigrator::migrate(database);
        QVERIFY2(second.success, qPrintable(second.errorMessage));
        QCOMPARE(second.schemaVersion, 1);

        QCOMPARE(scalarText(database, QStringLiteral("PRAGMA journal_mode")),
                 QStringLiteral("wal"));
        QCOMPARE(scalarInt(database, QStringLiteral("PRAGMA foreign_keys")), 1);
        QCOMPARE(scalarInt(database, QStringLiteral("PRAGMA user_version")), 1);
        QCOMPARE(scalarInt(database,
                           QStringLiteral("SELECT COUNT(*) FROM device")), 1);
        QCOMPARE(scalarInt(database,
                           QStringLiteral("SELECT COUNT(*) FROM tag")), 6);
        QCOMPARE(scalarInt(database,
                           QStringLiteral("SELECT COUNT(*) FROM alarm_rule")), 5);
        QCOMPARE(scalarText(database,
                            QStringLiteral("SELECT name || ':' || protocol_key "
                                           "FROM device WHERE id='virtual-plc-1'")),
                 QStringLiteral("VirtualPLC:modbus-tcp"));
        QCOMPARE(scalarText(database,
                            QStringLiteral("SELECT name FROM tag "
                                           "WHERE id='temperature'")),
                 QStringLiteral("温度"));
        QCOMPARE(scalarInt(database,
                           QStringLiteral("SELECT COUNT(*) FROM sqlite_master "
                                          "WHERE type='table' AND name IN "
                                          "('device','tag','measurement',"
                                          "'alarm_rule','alarm')")),
                 5);
        QCOMPARE(scalarInt(database,
                           QStringLiteral("SELECT COUNT(*) FROM sqlite_master "
                                          "WHERE type='index' AND name IN "
                                          "('idx_measurement_device_tag_time',"
                                          "'idx_alarm_device_time')")),
                 2);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_APPLESS_MAIN(DatabaseMigratorTest)

#include "tst_database_migrator.moc"
