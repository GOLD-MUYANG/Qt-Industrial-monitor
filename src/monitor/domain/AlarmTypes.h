#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

namespace industrial::monitor {

enum class AlarmSeverity : quint8
{
    Warning,
    Critical
};

enum class AlarmKind : quint8
{
    Threshold,
    Communication
};

enum class AlarmState : quint8
{
    Pending,
    ActiveUnacknowledged,
    ActiveAcknowledged,
    RecoveredUnacknowledged,
    RecoveredAcknowledged
};

struct AlarmRule
{
    QString id;
    QString deviceId;
    QString tagId;
    AlarmKind kind = AlarmKind::Threshold;
    bool hasLowerLimit = false;
    double lowerLimit = 0.0;
    bool hasUpperLimit = false;
    double upperLimit = 0.0;
    double hysteresis = 0.0;
    int activationSamples = 3;
    int recoverySamples = 3;
    AlarmSeverity severity = AlarmSeverity::Warning;
    bool enabled = true;
    QString message;
};

using AlarmRuleList = QList<AlarmRule>;

struct AlarmRecord
{
    QString id;
    QString ruleId;
    QString deviceId;
    QString tagId;
    AlarmKind kind = AlarmKind::Threshold;
    AlarmSeverity severity = AlarmSeverity::Warning;
    AlarmState state = AlarmState::Pending;
    QString message;
    double triggerValue = 0.0;
    QDateTime activatedAtUtc;
    QDateTime acknowledgedAtUtc;
    QDateTime recoveredAtUtc;
    QString acknowledgementNote;
};

using AlarmRecordList = QList<AlarmRecord>;

inline void registerAlarmMetaTypes()
{
    qRegisterMetaType<AlarmRule>();
    qRegisterMetaType<AlarmRuleList>();
    qRegisterMetaType<AlarmRecord>();
    qRegisterMetaType<AlarmRecordList>();
}

} // namespace industrial::monitor

Q_DECLARE_METATYPE(industrial::monitor::AlarmSeverity)
Q_DECLARE_METATYPE(industrial::monitor::AlarmKind)
Q_DECLARE_METATYPE(industrial::monitor::AlarmState)
Q_DECLARE_METATYPE(industrial::monitor::AlarmRule)
Q_DECLARE_METATYPE(industrial::monitor::AlarmRuleList)
Q_DECLARE_METATYPE(industrial::monitor::AlarmRecord)
Q_DECLARE_METATYPE(industrial::monitor::AlarmRecordList)
