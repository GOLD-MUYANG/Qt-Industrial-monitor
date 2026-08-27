#include "DevicePage.h"

#include "DeviceTableModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

using namespace industrial::protocol;

DevicePage::DevicePage(QWidget *parent)
    : QWidget(parent)
    , m_model(new DeviceTableModel(this))
{
    setObjectName(QStringLiteral("devicePage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);
    auto *title = new QLabel(QStringLiteral("设备管理"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *table = new QTableView(this);
    table->setModel(m_model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setMaximumHeight(130);
    root->addWidget(table);

    // 当前第三周只管理文档限定的单台 VirtualPLC，但字段保持协议无关。
    auto *group = new QGroupBox(QStringLiteral("连接参数"), this);
    auto *form = new QFormLayout(group);
    m_hostEdit = new QLineEdit(group);
    m_hostEdit->setObjectName(QStringLiteral("deviceHostEdit"));
    m_hostEdit->setAccessibleName(QStringLiteral("PLC 主机地址"));
    m_protocolCombo = new QComboBox(group);
    m_protocolCombo->setObjectName(QStringLiteral("deviceProtocolCombo"));
    m_protocolCombo->setAccessibleName(QStringLiteral("设备协议"));
    m_enabledCheck = new QCheckBox(QStringLiteral("允许连接"), group);
    m_enabledCheck->setObjectName(QStringLiteral("deviceEnabledCheck"));
    m_enabledCheck->setAccessibleName(QStringLiteral("设备启用状态"));
    m_portSpin = new QSpinBox(group);
    m_portSpin->setRange(1, 65'535);
    m_portSpin->setAccessibleName(QStringLiteral("PLC TCP 端口"));
    m_unitSpin = new QSpinBox(group);
    m_unitSpin->setRange(1, 247);
    m_unitSpin->setAccessibleName(QStringLiteral("Modbus Unit ID"));
    m_pollSpin = new QSpinBox(group);
    m_pollSpin->setRange(50, 60'000);
    m_pollSpin->setSuffix(QStringLiteral(" ms"));
    m_pollSpin->setAccessibleName(QStringLiteral("轮询周期"));
    m_timeoutSpin = new QSpinBox(group);
    m_timeoutSpin->setRange(1, 60'000);
    m_timeoutSpin->setSuffix(QStringLiteral(" ms"));
    m_timeoutSpin->setAccessibleName(QStringLiteral("请求超时"));
    form->addRow(QStringLiteral("协议："), m_protocolCombo);
    form->addRow(QStringLiteral("启用状态："), m_enabledCheck);
    form->addRow(QStringLiteral("主机地址："), m_hostEdit);
    form->addRow(QStringLiteral("TCP 端口："), m_portSpin);
    form->addRow(QStringLiteral("Unit ID："), m_unitSpin);
    form->addRow(QStringLiteral("轮询周期："), m_pollSpin);
    form->addRow(QStringLiteral("请求超时："), m_timeoutSpin);
    root->addWidget(group);

    auto *buttons = new QHBoxLayout;
    auto *saveButton = new QPushButton(QStringLiteral("保存并应用"), this);
    auto *connectButton = new QPushButton(QStringLiteral("连接设备"), this);
    auto *disconnectButton = new QPushButton(QStringLiteral("断开连接"), this);
    buttons->addStretch();
    buttons->addWidget(saveButton);
    buttons->addWidget(connectButton);
    buttons->addWidget(disconnectButton);
    root->addLayout(buttons);
    root->addStretch();

    connect(saveButton, &QPushButton::clicked, this,
            [this]() { emit saveRequested(editedConfig()); });
    connect(connectButton, &QPushButton::clicked,
            this, &DevicePage::connectRequested);
    connect(disconnectButton, &QPushButton::clicked,
            this, &DevicePage::disconnectRequested);
}

void DevicePage::addProtocol(const ProtocolDescriptor &descriptor)
{
    if (descriptor.key.isEmpty()) {
        return;
    }
    if (m_protocolCombo->findData(descriptor.key) < 0) {
        m_protocolCombo->addItem(descriptor.displayName, descriptor.key);
    }
    m_model->addProtocol(descriptor);
}

void DevicePage::setDevice(const DeviceConfig &config)
{
    m_device = config;
    m_model->setDevice(config);
    int protocolIndex = m_protocolCombo->findData(config.protocolKey);
    if (protocolIndex < 0 && !config.protocolKey.isEmpty()) {
        m_protocolCombo->addItem(config.protocolKey, config.protocolKey);
        protocolIndex = m_protocolCombo->count() - 1;
    }
    m_protocolCombo->setCurrentIndex(protocolIndex);
    m_enabledCheck->setChecked(config.enabled);
    m_hostEdit->setText(config.host);
    m_portSpin->setValue(config.port);
    m_unitSpin->setValue(config.unitId);
    m_pollSpin->setValue(config.pollIntervalMs);
    m_timeoutSpin->setValue(config.timeoutMs);
}

void DevicePage::setDeviceState(const DeviceState &state)
{
    m_model->setState(state);
}

void DevicePage::setLastCommunicationTime(const QDateTime &timestampUtc)
{
    m_model->setLastCommunicationTime(timestampUtc);
}

DeviceConfig DevicePage::editedConfig() const
{
    DeviceConfig config = m_device;
    config.protocolKey = m_protocolCombo->currentData().toString();
    config.enabled = m_enabledCheck->isChecked();
    config.host = m_hostEdit->text().trimmed();
    config.port = static_cast<quint16>(m_portSpin->value());
    config.unitId = m_unitSpin->value();
    config.pollIntervalMs = m_pollSpin->value();
    config.timeoutMs = m_timeoutSpin->value();
    return config;
}
