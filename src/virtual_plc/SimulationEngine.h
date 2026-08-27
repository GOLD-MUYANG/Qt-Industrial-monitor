#pragma once

#include <QtGlobal>

class RegisterBank;

class SimulationEngine
{
public:
    bool setTemperatureOverride(quint16 rawTemperature);
    void clearTemperatureOverride();
    void advance(RegisterBank &registers);

private:
    // m_tick表示第几个模拟时间步
    quint64 m_tick = 0;
    bool m_hasTemperatureOverride = false;
    quint16 m_temperatureOverride = 0;
};
