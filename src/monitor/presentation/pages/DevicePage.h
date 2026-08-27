#pragma once

#include <QWidget>

#include <industrial/protocol/ProtocolTypes.h>

class DeviceTableModel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class DevicePage final : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePage(QWidget *parent = nullptr);

public slots:
    void addProtocol(
        const industrial::protocol::ProtocolDescriptor &descriptor);
    void setDevice(const industrial::protocol::DeviceConfig &config);
    void setDeviceState(const industrial::protocol::DeviceState &state);
    void setLastCommunicationTime(const QDateTime &timestampUtc);

signals:
    void saveRequested(const industrial::protocol::DeviceConfig &config);
    void connectRequested();
    void disconnectRequested();

private:
    industrial::protocol::DeviceConfig editedConfig() const;

    DeviceTableModel *m_model = nullptr;
    industrial::protocol::DeviceConfig m_device;
    QLineEdit *m_hostEdit = nullptr;
    QComboBox *m_protocolCombo = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QSpinBox *m_unitSpin = nullptr;
    QSpinBox *m_pollSpin = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
};
