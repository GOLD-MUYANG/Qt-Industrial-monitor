#pragma once

#include <QAbstractTableModel>
#include <QHash>

#include <industrial/protocol/ProtocolTypes.h>

class DeviceTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        IdColumn,
        ProtocolColumn,
        EndpointColumn,
        UnitIdColumn,
        PollColumn,
        LastCommunicationColumn,
        StateColumn,
        ColumnCount
    };

    explicit DeviceTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

public slots:
    void addProtocol(
        const industrial::protocol::ProtocolDescriptor &descriptor);
    void setDevice(const industrial::protocol::DeviceConfig &config);
    void setState(const industrial::protocol::DeviceState &state);
    void setLastCommunicationTime(const QDateTime &timestampUtc);

private:
    static QString stateText(industrial::protocol::ConnectionState state);
    static QColor stateColor(industrial::protocol::ConnectionState state);

    industrial::protocol::DeviceConfig m_device;
    industrial::protocol::DeviceState m_state;
    QDateTime m_lastCommunicationUtc;
    QHash<QString, QString> m_protocolNames;
    bool m_hasDevice = false;
};
