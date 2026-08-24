#pragma once

#include <QMainWindow>

#include <array>

#include "VirtualPlcServer.h"

class QLabel;
class QPushButton;
class QSpinBox;

class VirtualPlcWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit VirtualPlcWindow(QWidget *parent = nullptr);

    bool startDefaultServer();

private slots:
    void startServer();
    void refreshValues();
    void applyTargetSpeed();

private:
    VirtualPlcServer m_server;
    QLabel *m_statusLabel = nullptr;
    std::array<QLabel *, 6> m_valueLabels{};
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QSpinBox *m_targetSpeedSpinBox = nullptr;
};
