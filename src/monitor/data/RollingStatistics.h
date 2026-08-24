#pragma once

#include <QDateTime>
#include <QQueue>

class RollingStatistics final
{
public:
    explicit RollingStatistics(qint64 windowDurationMs = 60'000,
                               int maximumSamples = 120);

    void add(double value, const QDateTime &timestampUtc);

    bool isEmpty() const;
    int count() const;
    double current() const;
    double minimum() const;
    double maximum() const;
    double average() const;

private:
    struct Entry
    {
        double value = 0.0;
        QDateTime timestampUtc;
    };

    void trim(const QDateTime &latestTimestampUtc);

    qint64 m_windowDurationMs;
    int m_maximumSamples;
    QQueue<Entry> m_entries;
    double m_sum = 0.0;
};
