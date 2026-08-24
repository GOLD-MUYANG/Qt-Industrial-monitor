#include <QtTest>

#include "PluginManager.h"

class PluginManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsMissingDirectory();
    void acceptsEmptyDirectory();
    void rejectsIncompatibleApiVersion();
};

void PluginManagerTest::reportsMissingDirectory()
{
    QTemporaryDir parent;
    PluginManager manager;

    manager.scan(parent.filePath(QStringLiteral("missing")));

    QVERIFY(manager.plugins().isEmpty());
    QCOMPARE(manager.errors().size(), 1);
    QVERIFY(manager.errors().constFirst().message.contains(QStringLiteral("不存在")));
}

void PluginManagerTest::acceptsEmptyDirectory()
{
    QTemporaryDir directory;
    PluginManager manager;

    manager.scan(directory.path());

    QVERIFY(manager.plugins().isEmpty());
    QVERIFY(manager.errors().isEmpty());
}

void PluginManagerTest::rejectsIncompatibleApiVersion()
{
    PluginManager manager;

    manager.scan(QString::fromUtf8(INVALID_API_PLUGIN_DIR));

    QVERIFY(manager.plugins().isEmpty());
    QCOMPARE(manager.errors().size(), 1);
    QVERIFY(manager.errors().constFirst().message.contains(QStringLiteral("apiVersion")));
    QVERIFY(manager.errors().constFirst().message.contains(QStringLiteral("999")));
}

QTEST_GUILESS_MAIN(PluginManagerTest)

#include "tst_plugin_manager.moc"
