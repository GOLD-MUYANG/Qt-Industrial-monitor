#include "RegisterBank.h"

RegisterBank::RegisterBank()
    : m_holdingRegisters(HoldingRegisterCount, 0)
{
    m_holdingRegisters[Temperature] = 420;
    m_holdingRegisters[Pressure] = 120;
    m_holdingRegisters[Speed] = 1500;
    m_holdingRegisters[Voltage] = 2200;
    m_holdingRegisters[Status] = 1;
    m_holdingRegisters[TargetSpeed] = 1500;
}

quint16 RegisterBank::value(quint16 address) const
{
    if (address >= m_holdingRegisters.size()) {
        return 0;
    }
    return m_holdingRegisters.at(address);
}

bool RegisterBank::setValue(quint16 address, quint16 value)
{
    if (!isKnownAddress(address)) {
        return false;
    }
    m_holdingRegisters[address] = value;
    return true;
}

bool RegisterBank::setTargetSpeed(quint16 speed)
{
    if (speed > 6000) {
        return false;
    }
    return setValue(TargetSpeed, speed);
}

QVector<quint16> RegisterBank::holdingRegisters() const
{
    return m_holdingRegisters;
}

bool RegisterBank::isKnownAddress(quint16 address)
{
    return address <= Status || address == TargetSpeed;
}
