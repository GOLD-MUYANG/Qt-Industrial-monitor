#pragma once

#include <QAbstractListModel>

#include <industrial/protocol/ProtocolTypes.h>

class QmlProtocolModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        ProtocolKeyRole = Qt::UserRole + 1,
        DisplayNameRole
    };
    Q_ENUM(Role)

    explicit QmlProtocolModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOfKey(const QString &protocolKey) const;

public slots:
    void addProtocol(
        const industrial::protocol::ProtocolDescriptor &descriptor);

private:
    QList<industrial::protocol::ProtocolDescriptor> m_protocols;
};
