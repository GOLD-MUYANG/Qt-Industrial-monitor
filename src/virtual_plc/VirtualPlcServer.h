#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>

#include "RegisterBank.h"
#include "SimulationEngine.h"

class QModbusTcpServer;

class VirtualPlcServer final : public QObject
{
    Q_OBJECT

public:
    explicit VirtualPlcServer(QObject *parent = nullptr);

    bool start(const QHostAddress &address = QHostAddress::LocalHost,
               quint16 port = 1502,
               int unitId = 1);
    void stop();

    bool isRunning() const;
    QString errorString() const;
    const RegisterBank &registerBank() const;

public slots:
    void advanceOnce();
    bool setTargetSpeed(quint16 speed);

signals:
    void runningChanged(bool running);
    void registersChanged();

private slots:
    void handleDataWritten(int table, int address, int size);

private:
    bool resetDataMap();
    bool syncRange(quint16 startAddress, quint16 count);

    QModbusTcpServer *m_server = nullptr;
    QTimer m_simulationTimer;
    RegisterBank m_registers;
    SimulationEngine m_simulation;
    QString m_lastError;
};
