#include "VirtualPlcWindow.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr std::array<RegisterBank::Address, 6> DisplayAddresses{
    RegisterBank::Temperature,
    RegisterBank::Pressure,
    RegisterBank::Speed,
    RegisterBank::Voltage,
    RegisterBank::Status,
    RegisterBank::TargetSpeed
};

constexpr std::array<const char *, 6> DisplayNames{
    "温度（0.1 ℃）",
    "压力（0.01 MPa）",
    "实际转速（rpm）",
    "电压（0.1 V）",
    "运行状态位",
    "目标转速（rpm）"
};

} // namespace

VirtualPlcWindow::VirtualPlcWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("VirtualPLC - Modbus TCP"));
    resize(520, 360);

    // 连接控制区：窗口只发送命令，协议状态由 VirtualPlcServer 返回。
    auto *centralWidget = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(centralWidget);
    auto *connectionLayout = new QHBoxLayout;
    m_statusLabel = new QLabel(QStringLiteral("已停止"), centralWidget);
    m_startButton = new QPushButton(QStringLiteral("启动 127.0.0.1:1502"),
                                    centralWidget);
    m_stopButton = new QPushButton(QStringLiteral("停止"), centralWidget);
    m_stopButton->setEnabled(false);
    connectionLayout->addWidget(m_statusLabel, 1);
    connectionLayout->addWidget(m_startButton);
    connectionLayout->addWidget(m_stopButton);
    rootLayout->addLayout(connectionLayout);

    // 寄存器展示区：工业地址与 Qt 零基地址在标签中同时呈现。
    auto *registerGroup = new QGroupBox(QStringLiteral("Holding Registers"),
                                        centralWidget);
    auto *registerLayout = new QGridLayout(registerGroup);
    for (std::size_t index = 0; index < DisplayAddresses.size(); ++index) {
        const auto address = DisplayAddresses.at(index);
        const QString label = QStringLiteral("%1 / %2（Qt 地址 %3）")
                                  .arg(QString::fromUtf8(DisplayNames.at(index)))
                                  .arg(40001 + address)
                                  .arg(address);
        registerLayout->addWidget(new QLabel(label, registerGroup),
                                  static_cast<int>(index), 0);
        m_valueLabels.at(index) = new QLabel(registerGroup);
        registerLayout->addWidget(m_valueLabels.at(index),
                                  static_cast<int>(index), 1);
    }
    rootLayout->addWidget(registerGroup);

    // 写入区：只修改文档规定的目标转速寄存器 40011 / Qt 地址 10。
    auto *writeLayout = new QHBoxLayout;
    m_targetSpeedSpinBox = new QSpinBox(centralWidget);
    m_targetSpeedSpinBox->setRange(0, 6000);
    m_targetSpeedSpinBox->setValue(1500);
    auto *applyButton = new QPushButton(QStringLiteral("设置目标转速"),
                                        centralWidget);
    writeLayout->addWidget(m_targetSpeedSpinBox, 1);
    writeLayout->addWidget(applyButton);
    rootLayout->addLayout(writeLayout);

    setCentralWidget(centralWidget);
    statusBar()->showMessage(QStringLiteral("Unit ID 1，500 ms 确定性模拟"));

    connect(m_startButton, &QPushButton::clicked,
            this, &VirtualPlcWindow::startServer);
    connect(m_stopButton, &QPushButton::clicked,
            &m_server, &VirtualPlcServer::stop);
    connect(applyButton, &QPushButton::clicked,
            this, &VirtualPlcWindow::applyTargetSpeed);
    connect(&m_server, &VirtualPlcServer::registersChanged,
            this, &VirtualPlcWindow::refreshValues);
    connect(&m_server, &VirtualPlcServer::runningChanged, this,
            [this](bool running) {
                m_statusLabel->setText(running
                    ? QStringLiteral("监听中")
                    : QStringLiteral("已停止"));
                m_startButton->setEnabled(!running);
                m_stopButton->setEnabled(running);
            });

    refreshValues();
}

bool VirtualPlcWindow::startDefaultServer()
{
    const bool started = m_server.start(QHostAddress::LocalHost, 1502, 1);
    if (!started) {
        statusBar()->showMessage(m_server.errorString());
    }
    return started;
}

void VirtualPlcWindow::startServer()
{
    startDefaultServer();
}

void VirtualPlcWindow::refreshValues()
{
    const RegisterBank &registers = m_server.registerBank();
    for (std::size_t index = 0; index < DisplayAddresses.size(); ++index) {
        m_valueLabels.at(index)->setText(
            QString::number(registers.value(DisplayAddresses.at(index))));
    }
    m_targetSpeedSpinBox->setValue(
        registers.value(RegisterBank::TargetSpeed));
}

void VirtualPlcWindow::applyTargetSpeed()
{
    if (!m_server.setTargetSpeed(
            static_cast<quint16>(m_targetSpeedSpinBox->value()))) {
        statusBar()->showMessage(m_server.errorString());
    }
}
