#pragma once

#include <QVector>
#include <QtGlobal>

class RegisterBank
{
public:
    enum Address : quint16
    {
        Temperature = 0,
        Pressure = 1,
        Speed = 2,
        Voltage = 3,
        Status = 4,
        TargetSpeed = 10
    };

    static constexpr qsizetype HoldingRegisterCount = 11;

    RegisterBank();

    quint16 value(quint16 address) const;
    bool setValue(quint16 address, quint16 value);
    bool setTargetSpeed(quint16 speed);
    QVector<quint16> holdingRegisters() const;

private:
    static bool isKnownAddress(quint16 address);

    QVector<quint16> m_holdingRegisters;
};
