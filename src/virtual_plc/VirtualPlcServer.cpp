#include "VirtualPlcServer.h"

#include <QModbusDataUnit>
#include <QModbusTcpServer>
#include <QVariant>

VirtualPlcServer::VirtualPlcServer(QObject *parent)
    : QObject(parent)
    , m_server(new QModbusTcpServer(this))
{
    m_simulationTimer.setInterval(500);
    connect(&m_simulationTimer, &QTimer::timeout,
            this, &VirtualPlcServer::advanceOnce);
    connect(m_server, &QModbusServer::dataWritten,
            this, &VirtualPlcServer::handleDataWritten);
    connect(m_server, &QModbusDevice::stateChanged, this,
            [this](QModbusDevice::State state) {
                const bool running = state == QModbusDevice::ConnectedState;
                if (running) {
                    m_simulationTimer.start();
                } else {
                    m_simulationTimer.stop();
                }
                emit runningChanged(running);
            });
}

bool VirtualPlcServer::start(const QHostAddress &address,
                             quint16 port,
                             int unitId)
{
    m_lastError.clear();
    if (isRunning()) {
        m_lastError = QStringLiteral("VirtualPLC 已经在运行");
        return false;
    }
    if (address.isNull() || port == 0 || unitId < 1 || unitId > 247) {
        m_lastError = QStringLiteral("无效的监听地址、端口或 Unit ID");
        return false;
    }
    if (!resetDataMap()) {
        return false;
    }

    m_server->setServerAddress(unitId);
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                     address.toString());
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                     port);
    if (!m_server->connectDevice()) {
        m_lastError = m_server->errorString();
        return false;
    }
    return true;
}

void VirtualPlcServer::stop()
{
    m_simulationTimer.stop();
    if (m_server->state() != QModbusDevice::UnconnectedState) {
        m_server->disconnectDevice();
    }
}

bool VirtualPlcServer::isRunning() const
{
    return m_server->state() == QModbusDevice::ConnectedState;
}

QString VirtualPlcServer::errorString() const
{
    return m_lastError.isEmpty() ? m_server->errorString() : m_lastError;
}

const RegisterBank &VirtualPlcServer::registerBank() const
{
    return m_registers;
}

void VirtualPlcServer::advanceOnce()
{
    m_simulation.advance(m_registers);
    if (!syncRange(RegisterBank::Temperature, 5)) {
        m_lastError = m_server->errorString();
        return;
    }
    emit registersChanged();
}

bool VirtualPlcServer::setTargetSpeed(quint16 speed)
{
    if (!m_registers.setTargetSpeed(speed)) {
        m_lastError = QStringLiteral("目标转速超出 0..6000 rpm");
        return false;
    }
    if (isRunning() && !syncRange(RegisterBank::TargetSpeed, 1)) {
        m_lastError = m_server->errorString();
        return false;
    }
    emit registersChanged();
    return true;
}

void VirtualPlcServer::handleDataWritten(int table, int address, int size)
{
    if (table != QModbusDataUnit::HoldingRegisters
        || address > RegisterBank::TargetSpeed
        || address + size <= RegisterBank::TargetSpeed) {
        return;
    }

    quint16 targetSpeed = 0;
    if (m_server->data(QModbusDataUnit::HoldingRegisters,
                       RegisterBank::TargetSpeed,
                       &targetSpeed)) {
        m_registers.setTargetSpeed(targetSpeed);
        emit registersChanged();
    }
}

bool VirtualPlcServer::resetDataMap()
{
    QModbusDataUnitMap map;
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters,
                               0,
                               RegisterBank::HoldingRegisterCount));
    if (!m_server->setMap(map)) {
        m_lastError = QStringLiteral("无法建立 Holding Register 映射");
        return false;
    }
    return syncRange(0, RegisterBank::HoldingRegisterCount);
}

bool VirtualPlcServer::syncRange(quint16 startAddress, quint16 count)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,
                         startAddress,
                         count);
    for (quint16 offset = 0; offset < count; ++offset) {
        unit.setValue(offset, m_registers.value(startAddress + offset));
    }
    return m_server->setData(unit);
}
