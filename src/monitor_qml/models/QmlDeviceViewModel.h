#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>

#include <industrial/protocol/ProtocolTypes.h>

class QmlDeviceViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString protocolKey READ protocolKey NOTIFY deviceChanged)
    Q_PROPERTY(QString protocolName READ protocolName NOTIFY protocolNameChanged)
    Q_PROPERTY(QString host READ host NOTIFY deviceChanged)
    Q_PROPERTY(int port READ port NOTIFY deviceChanged)
    Q_PROPERTY(int unitId READ unitId NOTIFY deviceChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs NOTIFY deviceChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs NOTIFY deviceChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY deviceChanged)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionStateText READ connectionStateText
                   NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionMessage READ connectionMessage
                   NOTIFY connectionChanged)
    Q_PROPERTY(QString lastCommunicationText READ lastCommunicationText
                   NOTIFY lastCommunicationChanged)

public:
    explicit QmlDeviceViewModel(QObject *parent = nullptr);

    QString deviceId() const;
    QString deviceName() const;
    QString protocolKey() const;
    QString protocolName() const;
    QString host() const;
    int port() const;
    int unitId() const;
    int pollIntervalMs() const;
    int timeoutMs() const;
    bool enabled() const;
    int connectionState() const;
    QString connectionStateText() const;
    QString connectionMessage() const;
    QString lastCommunicationText() const;

public slots:
    void setDevice(const industrial::protocol::DeviceConfig &config);
    void setState(const industrial::protocol::DeviceState &state);
    void setLastCommunicationTime(const QDateTime &timestampUtc);
    void setProtocolDisplayName(const QString &protocolKey,
                                const QString &displayName);

signals:
    void deviceChanged();
    void protocolNameChanged();
    void connectionChanged();
    void lastCommunicationChanged();

private:
    static QString stateText(industrial::protocol::ConnectionState state);

    industrial::protocol::DeviceConfig m_device;
    industrial::protocol::DeviceState m_state;
    QHash<QString, QString> m_protocolNames;
    QDateTime m_lastCommunicationUtc;
};
