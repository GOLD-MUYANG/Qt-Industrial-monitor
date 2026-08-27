#pragma once

#include "AlarmTypes.h"

#include <QHash>
#include <QObject>

#include <industrial/protocol/ProtocolTypes.h>

namespace industrial::monitor {

class AlarmEngine final : public QObject
{
    Q_OBJECT

public:
    explicit AlarmEngine(QObject *parent = nullptr);

    bool setRules(const AlarmRuleList &rules);
    bool acknowledge(const QString &alarmId,
                     const QString &note,
                     const QDateTime &acknowledgedAtUtc =
                         QDateTime::currentDateTimeUtc());

public slots:
    void processSamples(const industrial::protocol::SampleBatch &samples);
    void handleDeviceState(const industrial::protocol::DeviceState &state);

signals:
    void alarmChanged(const industrial::monitor::AlarmRecord &alarm);
    void ruleRejected(const QString &ruleId, const QString &reason);

private:
    struct RuleRuntime
    {
        AlarmRule rule;
        int activationCount = 0;
        int recoveryCount = 0;
        bool hasRecord = false;
        AlarmRecord record;
    };

    static bool isActive(AlarmState state);
    static bool isAcknowledged(AlarmState state);
    static bool isRuleValid(const AlarmRule &rule, QString *reason);
    static bool violates(const AlarmRule &rule, double value);
    static bool isInsideRecoveryBand(const AlarmRule &rule, double value);

    void processThreshold(RuleRuntime &runtime,
                          const industrial::protocol::MeasurementSample &sample);
    void activate(RuleRuntime &runtime,
                  double triggerValue,
                  const QDateTime &timestampUtc);
    void recover(RuleRuntime &runtime, const QDateTime &timestampUtc);

    QHash<QString, RuleRuntime> m_rules;
};

} // namespace industrial::monitor
