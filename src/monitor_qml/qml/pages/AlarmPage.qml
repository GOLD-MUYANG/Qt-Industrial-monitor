import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml
import "../components"

Item {
    id: root
    objectName: "alarmPage"
    required property var facade
    property string selectedAlarmId: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        ColumnLayout {
            Label {
                text: "报警中心"
                color: Theme.textPrimary
                font.pixelSize: Theme.titleSize
                font.weight: Font.Bold
            }
            Label {
                text: "活动报警与完整生命周期；确认使用稳定报警 ID"
                color: Theme.textSecondary
                font.pixelSize: Theme.bodySize
            }
        }

        TabBar {
            id: alarmTabs
            Layout.fillWidth: true

            TabButton { text: "活动报警" }
            TabButton { text: "报警历史" }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusMedium
            border.color: Theme.border

            ListView {
                id: alarmList
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingSmall
                clip: true
                model: alarmTabs.currentIndex === 0
                       ? facade.activeAlarmModel
                       : facade.alarmHistoryModel

                delegate: Rectangle {
                    required property string alarmId
                    required property string activatedAtText
                    required property string deviceId
                    required property string tagText
                    required property string message
                    required property var triggerValue
                    required property int severity
                    required property string severityText
                    required property string stateText
                    required property bool acknowledgeable

                    width: alarmList.width
                    height: 126
                    radius: Theme.radiusSmall
                    color: root.selectedAlarmId === alarmId
                           ? "#E8F2FB" : "#FAFCFE"
                    border.color: severity === 1
                                  ? "#E6A29C" : "#E8B36A"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingMedium

                        Rectangle {
                            Layout.preferredWidth: 5
                            Layout.fillHeight: true
                            radius: 2
                            color: severity === 1
                                   ? Theme.danger : Theme.warning
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                StatusBadge {
                                    text: severityText
                                    tone: severity === 1 ? "error" : "warning"
                                }
                                Label {
                                    text: stateText
                                    color: Theme.textSecondary
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: activatedAtText
                                    color: Theme.textSecondary
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: message.length > 0 ? message : "未提供报警消息"
                                color: Theme.textPrimary
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "设备 " + deviceId + "  ·  测点/类型 " + tagText
                                      + (typeof triggerValue === "number"
                                         ? "  ·  触发值 " + triggerValue.toFixed(2)
                                         : "")
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                        Button {
                            text: root.selectedAlarmId === alarmId
                                  ? "已选择" : "选择确认"
                            implicitHeight: Theme.controlHeight
                            enabled: acknowledgeable
                            onClicked: root.selectedAlarmId = alarmId
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: alarmList.count === 0
                    text: alarmTabs.currentIndex === 0
                          ? "当前没有活动报警" : "尚无报警历史"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.bodySize
                }
            }
        }

        SectionPanel {
            Layout.fillWidth: true
            title: "报警确认"

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: alarmNote
                    objectName: "alarmNoteField"
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeight
                    placeholderText: root.selectedAlarmId.length > 0
                                     ? "可选：填写确认备注"
                                     : "请先选择一条可确认报警"
                    Accessible.name: "报警确认备注"
                }
                Button {
                    objectName: "acknowledgeAlarmButton"
                    text: "确认报警"
                    implicitHeight: Theme.controlHeight
                    enabled: root.selectedAlarmId.length > 0
                             && facade.initialized
                             && !facade.commandBusy
                    onClicked: {
                        facade.acknowledgeAlarm(root.selectedAlarmId,
                                                alarmNote.text)
                        root.selectedAlarmId = ""
                        alarmNote.clear()
                    }
                }
            }
        }
    }
}
