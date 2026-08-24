#include "RollingStatistics.h"

#include <algorithm>

RollingStatistics::RollingStatistics(qint64 windowDurationMs,
                                     int maximumSamples)
    : m_windowDurationMs(std::max<qint64>(1, windowDurationMs))
    , m_maximumSamples(std::max(1, maximumSamples))
{
}

void RollingStatistics::add(double value, const QDateTime &timestampUtc)
{
    m_entries.enqueue({value, timestampUtc});
    m_sum += value;
    trim(timestampUtc);
}

bool RollingStatistics::isEmpty() const
{
    return m_entries.isEmpty();
}

int RollingStatistics::count() const
{
    return m_entries.size();
}

double RollingStatistics::current() const
{
    return isEmpty() ? 0.0 : m_entries.constLast().value;
}

double RollingStatistics::minimum() const
{
    if (isEmpty()) {
        return 0.0;
    }
    double result = m_entries.constFirst().value;
    for (const auto &entry : m_entries) {
        result = std::min(result, entry.value);
    }
    return result;
}

double RollingStatistics::maximum() const
{
    if (isEmpty()) {
        return 0.0;
    }
    double result = m_entries.constFirst().value;
    for (const auto &entry : m_entries) {
        result = std::max(result, entry.value);
    }
    return result;
}

double RollingStatistics::average() const
{
    return isEmpty() ? 0.0 : m_sum / static_cast<double>(m_entries.size());
}

void RollingStatistics::trim(const QDateTime &latestTimestampUtc)
{
    const QDateTime cutoff = latestTimestampUtc.addMSecs(-m_windowDurationMs);
    while (!m_entries.isEmpty()
           && (m_entries.constFirst().timestampUtc < cutoff
               || m_entries.size() > m_maximumSamples)) {
        m_sum -= m_entries.dequeue().value;
    }
}
