#include "SimulationEngine.h"

#include "RegisterBank.h"

#include <algorithm>
#include <array>

namespace
{

// 用于改变模拟寄存器里的值，每隔500ms生成一批可预测设备数据的“设备数据发生器”。
template <std::size_t Size>
quint16 valueFromWave(quint16 base, const std::array<int, Size> &wave, quint64 tick)
{
    // 当前值 = 基础值 + 当前波动量
    return static_cast<quint16>(static_cast<int>(base) + wave.at(tick % Size));
}

} // namespace

void SimulationEngine::advance(RegisterBank &registers)
{
    static constexpr std::array<int, 8> TemperatureWave{0, 2, 4, 5, 3, 0, -3, -5};
    static constexpr std::array<int, 8> PressureWave{0, 1, 2, 1, 0, -1, -2, -1};
    static constexpr std::array<int, 8> VoltageWave{0, 1, 2, 1, 0, -1, -2, -1};

    registers.setValue(RegisterBank::Temperature, valueFromWave(420, TemperatureWave, m_tick));
    registers.setValue(RegisterBank::Pressure, valueFromWave(120, PressureWave, m_tick));
    registers.setValue(RegisterBank::Voltage, valueFromWave(2200, VoltageWave, m_tick));

    const int currentSpeed = registers.value(RegisterBank::Speed);
    const int targetSpeed = registers.value(RegisterBank::TargetSpeed);
    // std::clamp 是 C++ 17 引入的一个工具函数，用来把一个值限制在指定范围内。
    // 这里就是限制速度变化的范围在 -50 到 50 之间。每隔500ms刷新一次的时候再调用这个函数，
    // 模拟电机收到目标转速命令后，不会瞬间到达目标，而是逐渐加速的情景。
    const int delta = std::clamp(targetSpeed - currentSpeed, -50, 50);
    registers.setValue(RegisterBank::Speed, static_cast<quint16>(currentSpeed + delta));
    registers.setValue(RegisterBank::Status, 1);

    ++m_tick;
}
