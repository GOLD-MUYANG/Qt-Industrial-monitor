#include "DatabaseMigrator.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

bool loadDefaultConfiguration(QJsonObject *configuration,
                              QString *errorMessage)
{
    QFile file(QStringLiteral(":/industrial_monitor/config/default.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        *errorMessage = QStringLiteral("无法读取内嵌默认配置：%1")
                            .arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *errorMessage = QStringLiteral("默认配置 JSON 无效：%1")
                            .arg(parseError.errorString());
        return false;
    }
    const auto root = document.object();
    if (!root.value(QStringLiteral("device")).isObject()
        || !root.value(QStringLiteral("tags")).isArray()
        || !root.value(QStringLiteral("alarmRules")).isArray()) {
        *errorMessage = QStringLiteral(
            "默认配置必须包含 device、tags 和 alarmRules");
        return false;
    }
    *configuration = root;
    return true;
}

bool execute(QSqlDatabase &database,
             const QString &sql,
             QString *errorMessage)
{
    QSqlQuery query(database);
    if (query.exec(sql)) {
        return true;
    }
    *errorMessage = QStringLiteral("SQL 执行失败：%1；%2")
                        .arg(query.lastError().text(), sql);
    return false;
}

bool seedDefaultDevice(QSqlDatabase &database,
                       const QJsonObject &configuration,
                       QString *errorMessage)
{
    const auto seed = configuration.value(QStringLiteral("device")).toObject();
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO device "
            "(id,name,protocol_key,host,port,unit_id,poll_interval_ms,"
            "timeout_ms,protocol_retries,failure_threshold,enabled) "
            "VALUES (:id,:name,:protocol,:host,:port,:unit,:poll,:timeout,"
            ":retries,:failures,:enabled)"))) {
        *errorMessage = query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":id"), seed.value(QStringLiteral("id")).toString());
    query.bindValue(QStringLiteral(":name"), seed.value(QStringLiteral("name")).toString());
    query.bindValue(QStringLiteral(":protocol"), seed.value(QStringLiteral("protocolKey")).toString());
    query.bindValue(QStringLiteral(":host"), seed.value(QStringLiteral("host")).toString());
    query.bindValue(QStringLiteral(":port"), seed.value(QStringLiteral("port")).toInt());
    query.bindValue(QStringLiteral(":unit"), seed.value(QStringLiteral("unitId")).toInt());
    query.bindValue(QStringLiteral(":poll"), seed.value(QStringLiteral("pollIntervalMs")).toInt());
    query.bindValue(QStringLiteral(":timeout"), seed.value(QStringLiteral("timeoutMs")).toInt());
    query.bindValue(QStringLiteral(":retries"), seed.value(QStringLiteral("protocolRetries")).toInt());
    query.bindValue(QStringLiteral(":failures"), seed.value(QStringLiteral("failureThreshold")).toInt());
    query.bindValue(QStringLiteral(":enabled"), seed.value(QStringLiteral("enabled")).toBool() ? 1 : 0);
    if (query.exec()) {
        return true;
    }
    *errorMessage = query.lastError().text();
    return false;
}

bool seedDefaultTags(QSqlDatabase &database,
                     const QJsonObject &configuration,
                     QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO tag "
            "(id,device_id,name,unit,register_type,address,scale,writable) "
            "VALUES (:id,:device,:name,:unit,0,:address,:scale,:writable)"))) {
        *errorMessage = query.lastError().text();
        return false;
    }
    const QString deviceId = configuration.value(QStringLiteral("device"))
                                 .toObject()
                                 .value(QStringLiteral("id"))
                                 .toString();
    const auto seeds = configuration.value(QStringLiteral("tags")).toArray();
    for (const auto &seedValue : seeds) {
        const auto seed = seedValue.toObject();
        query.bindValue(QStringLiteral(":id"),
                        seed.value(QStringLiteral("id")).toString());
        query.bindValue(QStringLiteral(":device"), deviceId);
        query.bindValue(QStringLiteral(":name"),
                        seed.value(QStringLiteral("name")).toString());
        query.bindValue(QStringLiteral(":unit"),
                        seed.value(QStringLiteral("unit")).toString());
        query.bindValue(QStringLiteral(":address"),
                        seed.value(QStringLiteral("address")).toInt());
        query.bindValue(QStringLiteral(":scale"),
                        seed.value(QStringLiteral("scale")).toDouble());
        query.bindValue(QStringLiteral(":writable"),
                        seed.value(QStringLiteral("writable")).toBool() ? 1 : 0);
        if (!query.exec()) {
            *errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool seedDefaultAlarmRules(QSqlDatabase &database,
                           const QJsonObject &configuration,
                           QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO alarm_rule "
            "(id,device_id,tag_id,rule_type,lower_limit,upper_limit,hysteresis,"
            "activation_samples,recovery_samples,severity,enabled,message) "
            "VALUES (:id,:device,:tag,:kind,:lower,:upper,:hysteresis,"
            ":activation,:recovery,:severity,:enabled,:message)"))) {
        *errorMessage = query.lastError().text();
        return false;
    }
    const QString deviceId = configuration.value(QStringLiteral("device"))
                                 .toObject()
                                 .value(QStringLiteral("id"))
                                 .toString();
    const auto seeds =
        configuration.value(QStringLiteral("alarmRules")).toArray();
    for (const auto &seedValue : seeds) {
        const auto seed = seedValue.toObject();
        const auto tag = seed.value(QStringLiteral("tagId"));
        const auto lower = seed.value(QStringLiteral("lower"));
        const auto upper = seed.value(QStringLiteral("upper"));
        query.bindValue(QStringLiteral(":id"),
                        seed.value(QStringLiteral("id")).toString());
        query.bindValue(QStringLiteral(":device"), deviceId);
        query.bindValue(QStringLiteral(":tag"),
                        tag.isNull() ? QVariant() : tag.toVariant());
        query.bindValue(QStringLiteral(":kind"),
                        seed.value(QStringLiteral("kind")).toInt());
        query.bindValue(QStringLiteral(":lower"),
                        lower.isNull() ? QVariant() : lower.toVariant());
        query.bindValue(QStringLiteral(":upper"),
                        upper.isNull() ? QVariant() : upper.toVariant());
        query.bindValue(QStringLiteral(":hysteresis"),
                        seed.value(QStringLiteral("hysteresis")).toDouble());
        query.bindValue(QStringLiteral(":activation"),
                        seed.value(QStringLiteral("activationSamples")).toInt());
        query.bindValue(QStringLiteral(":recovery"),
                        seed.value(QStringLiteral("recoverySamples")).toInt());
        query.bindValue(QStringLiteral(":severity"),
                        seed.value(QStringLiteral("severity")).toInt());
        query.bindValue(QStringLiteral(":enabled"),
                        seed.value(QStringLiteral("enabled")).toBool() ? 1 : 0);
        query.bindValue(QStringLiteral(":message"),
                        seed.value(QStringLiteral("message")).toString());
        if (!query.exec()) {
            *errorMessage = query.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

DatabaseMigrationResult DatabaseMigrator::migrate(QSqlDatabase &database)
{
    Q_INIT_RESOURCE(DefaultConfig);
    DatabaseMigrationResult result;
    if (!database.isValid() || !database.isOpen()) {
        result.errorMessage = QStringLiteral("数据库连接无效或尚未打开");
        return result;
    }

    QString errorMessage;
    QJsonObject defaultConfiguration;
    if (!loadDefaultConfiguration(&defaultConfiguration, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }
    const QStringList pragmas = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA busy_timeout=500"),
    };
    for (const auto &pragma : pragmas) {
        if (!execute(database, pragma, &errorMessage)) {
            result.errorMessage = errorMessage;
            return result;
        }
    }

    if (!database.transaction()) {
        result.errorMessage = database.lastError().text();
        return result;
    }

    const QStringList schemaStatements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS device ("
            "id TEXT PRIMARY KEY,name TEXT NOT NULL,protocol_key TEXT NOT NULL,"
            "host TEXT NOT NULL,port INTEGER NOT NULL,unit_id INTEGER NOT NULL,"
            "poll_interval_ms INTEGER NOT NULL,timeout_ms INTEGER NOT NULL,"
            "protocol_retries INTEGER NOT NULL,failure_threshold INTEGER NOT NULL,"
            "enabled INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tag ("
            "id TEXT PRIMARY KEY,device_id TEXT NOT NULL,name TEXT NOT NULL,"
            "unit TEXT NOT NULL,register_type INTEGER NOT NULL,address INTEGER NOT NULL,"
            "scale REAL NOT NULL,writable INTEGER NOT NULL DEFAULT 0,"
            "FOREIGN KEY(device_id) REFERENCES device(id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS measurement ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,"
            "tag_id TEXT NOT NULL,value REAL NOT NULL,quality INTEGER NOT NULL,"
            "timestamp_utc INTEGER NOT NULL,sequence INTEGER NOT NULL,"
            "FOREIGN KEY(device_id) REFERENCES device(id),"
            "FOREIGN KEY(tag_id) REFERENCES tag(id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarm_rule ("
            "id TEXT PRIMARY KEY,device_id TEXT NOT NULL,tag_id TEXT,"
            "rule_type INTEGER NOT NULL,lower_limit REAL,upper_limit REAL,"
            "hysteresis REAL NOT NULL DEFAULT 0,activation_samples INTEGER NOT NULL DEFAULT 3,"
            "recovery_samples INTEGER NOT NULL DEFAULT 3,severity INTEGER NOT NULL,"
            "enabled INTEGER NOT NULL DEFAULT 1,message TEXT NOT NULL,"
            "FOREIGN KEY(device_id) REFERENCES device(id),"
            "FOREIGN KEY(tag_id) REFERENCES tag(id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarm ("
            "id TEXT PRIMARY KEY,rule_id TEXT NOT NULL,device_id TEXT NOT NULL,"
            "tag_id TEXT,message TEXT NOT NULL,severity INTEGER NOT NULL,"
            "trigger_value REAL,activated_at_utc INTEGER NOT NULL,"
            "acknowledged_at_utc INTEGER,recovered_at_utc INTEGER,"
            "acknowledgement_note TEXT,"
            "FOREIGN KEY(rule_id) REFERENCES alarm_rule(id))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_measurement_device_tag_time "
            "ON measurement(device_id,tag_id,timestamp_utc)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_alarm_device_time "
            "ON alarm(device_id,activated_at_utc)"),
    };
    for (const auto &statement : schemaStatements) {
        if (!execute(database, statement, &errorMessage)) {
            database.rollback();
            result.errorMessage = errorMessage;
            return result;
        }
    }

    if (!seedDefaultDevice(database, defaultConfiguration, &errorMessage)
        || !seedDefaultTags(database, defaultConfiguration, &errorMessage)
        || !seedDefaultAlarmRules(database, defaultConfiguration, &errorMessage)
        || !execute(database,
                    QStringLiteral("PRAGMA user_version=1"),
                    &errorMessage)) {
        database.rollback();
        result.errorMessage = errorMessage;
        return result;
    }

    if (!database.commit()) {
        result.errorMessage = database.lastError().text();
        database.rollback();
        return result;
    }

    result.success = true;
    result.schemaVersion = CurrentSchemaVersion;
    return result;
}
