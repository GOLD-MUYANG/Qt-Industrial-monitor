#include "AlarmEngine.h"

#include <QUuid>

#include <cmath>

using namespace industrial::protocol;

namespace industrial::monitor
{

AlarmEngine::AlarmEngine(QObject *parent) : QObject(parent)
{
}

bool AlarmEngine::setRules(const AlarmRuleList &rules)
{
    QHash<QString, RuleRuntime> accepted;
    bool allValid = true;
    for (const auto &rule : rules)
    {
        QString reason;
        if (!isRuleValid(rule, &reason) || accepted.contains(rule.id))
        {
            if (accepted.contains(rule.id))
            {
                reason = QStringLiteral("报警规则 ID 重复");
            }
            emit ruleRejected(rule.id, reason);
            allValid = false;
            continue;
        }
        RuleRuntime runtime;
        runtime.rule = rule;
        accepted.insert(rule.id, runtime);
    }
    m_rules = accepted;
    return allValid;
}

bool AlarmEngine::acknowledge(const QString &alarmId,
                              const QString &note,
                              const QDateTime &acknowledgedAtUtc)
{
    if (!acknowledgedAtUtc.isValid() || acknowledgedAtUtc.timeSpec() != Qt::UTC)
    {
        return false;
    }

    for (auto it = m_rules.begin(); it != m_rules.end(); ++it)
    {
        auto &runtime = it.value();
        if (!runtime.hasRecord || runtime.record.id != alarmId ||
            isAcknowledged(runtime.record.state))
        {
            continue;
        }
        runtime.record.acknowledgedAtUtc = acknowledgedAtUtc;
        runtime.record.acknowledgementNote = note.trimmed();
        runtime.record.state = isActive(runtime.record.state) ? AlarmState::ActiveAcknowledged
                                                              : AlarmState::RecoveredAcknowledged;
        emit alarmChanged(runtime.record);
        return true;
    }
    return false;
}

void AlarmEngine::processSamples(const SampleBatch &samples)
{
    for (const auto &sample : samples)
    {
        if (sample.quality != DataQuality::Good || !sample.timestampUtc.isValid() ||
            sample.timestampUtc.timeSpec() != Qt::UTC || !std::isfinite(sample.engineeringValue))
        {
            continue;
        }

        for (auto it = m_rules.begin(); it != m_rules.end(); ++it)
        {
            auto &runtime = it.value();
            const auto &rule = runtime.rule;
            if (!rule.enabled || rule.kind != AlarmKind::Threshold ||
                rule.deviceId != sample.deviceId || rule.tagId != sample.tagId)
            {
                continue;
            }
            processThreshold(runtime, sample);
        }
    }
}

void AlarmEngine::handleDeviceState(const DeviceState &state)
{
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it)
    {
        auto &runtime = it.value();
        const auto &rule = runtime.rule;
        if (!rule.enabled || rule.kind != AlarmKind::Communication ||
            rule.deviceId != state.deviceId)
        {
            continue;
        }

        if (state.connectionState == ConnectionState::Reconnecting)
        {
            if (!runtime.hasRecord || !isActive(runtime.record.state))
            {
                activate(runtime, 0.0, QDateTime::currentDateTimeUtc());
            }
        }
        else if (state.connectionState == ConnectionState::Online && runtime.hasRecord &&
                 isActive(runtime.record.state))
        {
            recover(runtime, QDateTime::currentDateTimeUtc());
        }
    }
}

bool AlarmEngine::isActive(AlarmState state)
{
    return state == AlarmState::ActiveUnacknowledged || state == AlarmState::ActiveAcknowledged;
}

bool AlarmEngine::isAcknowledged(AlarmState state)
{
    return state == AlarmState::ActiveAcknowledged || state == AlarmState::RecoveredAcknowledged;
}

bool AlarmEngine::isRuleValid(const AlarmRule &rule, QString *reason)
{
    if (rule.id.trimmed().isEmpty() || rule.deviceId.trimmed().isEmpty())
    {
        *reason = QStringLiteral("规则 ID 和设备 ID 不能为空");
        return false;
    }
    if (rule.activationSamples < 1 || rule.recoverySamples < 1 || rule.hysteresis < 0.0)
    {
        *reason = QStringLiteral("连续样本数必须为正数且回差不能为负数");
        return false;
    }
    if (rule.kind == AlarmKind::Threshold)
    {
        if (rule.tagId.trimmed().isEmpty() || (!rule.hasLowerLimit && !rule.hasUpperLimit))
        {
            *reason = QStringLiteral("阈值规则必须指定测点和至少一个限值");
            return false;
        }
        if (rule.hasLowerLimit && rule.hasUpperLimit && rule.lowerLimit >= rule.upperLimit)
        {
            *reason = QStringLiteral("下限必须小于上限");
            return false;
        }
    }
    return true;
}

bool AlarmEngine::violates(const AlarmRule &rule, double value)
{
    return (rule.hasLowerLimit && value < rule.lowerLimit) ||
           (rule.hasUpperLimit && value > rule.upperLimit);
}

bool AlarmEngine::isInsideRecoveryBand(const AlarmRule &rule, double value)
{
    const bool aboveLower = !rule.hasLowerLimit || value >= rule.lowerLimit + rule.hysteresis;
    const bool belowUpper = !rule.hasUpperLimit || value <= rule.upperLimit - rule.hysteresis;
    return aboveLower && belowUpper;
}

void AlarmEngine::processThreshold(RuleRuntime &runtime, const MeasurementSample &sample)
{
    if (!runtime.hasRecord || !isActive(runtime.record.state))
    {
        runtime.recoveryCount = 0;
        if (!violates(runtime.rule, sample.engineeringValue))
        {
            runtime.activationCount = 0;
            return;
        }
        // 一旦有三次超出阈值的值，就会开始报警。
        if (++runtime.activationCount >= runtime.rule.activationSamples)
        {
            /**
            清零两个计数器；
            生成新的报警 UUID；
            保存规则、设备、测点、触发值和激活时间；
            状态设置为 ActiveUnacknowledged；
            发出 alarmChanged。
            */
            activate(runtime, sample.engineeringValue, sample.timestampUtc);
        }
        return;
    }

    // 如果报警已经激活，那就不需要继续累加超限次数了，考虑恢复，这个恢复就是连续有三次在回差带内，
    // 就说明机器可能正常了，等下次再有三次不正常的，就可以再报警了。
    runtime.activationCount = 0;
    if (!isInsideRecoveryBand(runtime.rule, sample.engineeringValue))
    {
        runtime.recoveryCount = 0;
        return;
    }
    if (++runtime.recoveryCount >= runtime.rule.recoverySamples)
    {
        recover(runtime, sample.timestampUtc);
    }
}

void AlarmEngine::activate(RuleRuntime &runtime, double triggerValue, const QDateTime &timestampUtc)
{
    runtime.activationCount = 0;
    runtime.recoveryCount = 0;
    runtime.hasRecord = true;
    runtime.record = {};
    runtime.record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    runtime.record.ruleId = runtime.rule.id;
    runtime.record.deviceId = runtime.rule.deviceId;
    runtime.record.tagId = runtime.rule.tagId;
    runtime.record.kind = runtime.rule.kind;
    runtime.record.severity = runtime.rule.severity;
    runtime.record.state = AlarmState::ActiveUnacknowledged;
    runtime.record.message = runtime.rule.message;
    runtime.record.triggerValue = triggerValue;
    runtime.record.activatedAtUtc = timestampUtc;
    emit alarmChanged(runtime.record);
}

void AlarmEngine::recover(RuleRuntime &runtime, const QDateTime &timestampUtc)
{
    runtime.recoveryCount = 0;
    runtime.record.recoveredAtUtc = timestampUtc;
    runtime.record.state = isAcknowledged(runtime.record.state)
                               ? AlarmState::RecoveredAcknowledged
                               : AlarmState::RecoveredUnacknowledged;
    emit alarmChanged(runtime.record);
}

} // namespace industrial::monitor
