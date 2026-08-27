#include "QmlProtocolModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace industrial::protocol;

class QmlProtocolModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesNamedRolesAndUpsertsByKey();
    void ignoresEmptyKeys();
};

void QmlProtocolModelTest::exposesNamedRolesAndUpsertsByKey()
{
    QmlProtocolModel model;
    model.addProtocol({QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP"), 1, {}});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.roleNames().value(QmlProtocolModel::ProtocolKeyRole),
             QByteArray("protocolKey"));
    QCOMPARE(model.roleNames().value(QmlProtocolModel::DisplayNameRole),
             QByteArray("displayName"));
    QCOMPARE(model.data(model.index(0), QmlProtocolModel::ProtocolKeyRole).toString(),
             QStringLiteral("modbus-tcp"));
    QCOMPARE(model.indexOfKey(QStringLiteral("modbus-tcp")), 0);

    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    model.addProtocol({QStringLiteral("modbus-tcp"), QStringLiteral("Modbus TCP 插件"), 1, {}});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(model.data(model.index(0), QmlProtocolModel::DisplayNameRole).toString(),
             QStringLiteral("Modbus TCP 插件"));
}

void QmlProtocolModelTest::ignoresEmptyKeys()
{
    QmlProtocolModel model;
    model.addProtocol({{}, QStringLiteral("无效"), 1, {}});
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.indexOfKey(QStringLiteral("missing")), -1);
}

QTEST_GUILESS_MAIN(QmlProtocolModelTest)

#include "tst_qml_protocol_model.moc"
