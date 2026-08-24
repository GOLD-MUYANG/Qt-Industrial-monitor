#pragma once

#include <QtGlobal>

class RegisterBank;

class SimulationEngine
{
public:
    void advance(RegisterBank &registers);

private:
    // m_tick表示第几个模拟时间步
    quint64 m_tick = 0;
};
