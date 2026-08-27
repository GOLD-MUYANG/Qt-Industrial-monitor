#include "AlarmPage.h"

#include "AlarmTableModel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

AlarmPage::AlarmPage(QWidget *parent)
    : QWidget(parent)
    , m_activeModel(new AlarmTableModel(AlarmTableModel::Mode::Active, this))
    , m_historyModel(new AlarmTableModel(AlarmTableModel::Mode::History, this))
{
    setObjectName(QStringLiteral("alarmPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("报警中心"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);
    root->addWidget(new QLabel(
        QStringLiteral("等级、活动/恢复和确认状态均以文字与颜色共同表达。"),
        this));

    auto *tabs = new QTabWidget(this);
    m_activeTable = new QTableView(tabs);
    m_activeTable->setObjectName(QStringLiteral("activeAlarmTable"));
    m_activeTable->setModel(m_activeModel);
    m_activeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_activeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_activeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto *historyTable = new QTableView(tabs);
    historyTable->setObjectName(QStringLiteral("historyAlarmTable"));
    historyTable->setModel(m_historyModel);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabs->addTab(m_activeTable, QStringLiteral("活动报警"));
    tabs->addTab(historyTable, QStringLiteral("报警历史"));
    root->addWidget(tabs, 1);

    auto *acknowledgeLayout = new QHBoxLayout;
    acknowledgeLayout->addWidget(new QLabel(QStringLiteral("确认备注："), this));
    m_noteEdit = new QLineEdit(this);
    m_noteEdit->setAccessibleName(QStringLiteral("报警确认备注"));
    m_noteEdit->setPlaceholderText(QStringLiteral("可选，例如：现场已知悉"));
    auto *acknowledgeButton =
        new QPushButton(QStringLiteral("确认选中报警"), this);
    acknowledgeLayout->addWidget(m_noteEdit, 1);
    acknowledgeLayout->addWidget(acknowledgeButton);
    root->addLayout(acknowledgeLayout);

    connect(acknowledgeButton, &QPushButton::clicked, this, [this]() {
        const QString alarmId =
            m_activeModel->alarmIdAt(m_activeTable->currentIndex().row());
        if (!alarmId.isEmpty()) {
            emit acknowledgeRequested(alarmId, m_noteEdit->text());
        }
    });
}

void AlarmPage::upsertAlarm(const industrial::monitor::AlarmRecord &alarm)
{
    m_activeModel->upsertAlarm(alarm);
    m_historyModel->upsertAlarm(alarm);
}
