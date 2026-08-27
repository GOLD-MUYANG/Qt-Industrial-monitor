import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts
import IndustrialMonitor.Qml
import "../components"

Item {
    id: root
    objectName: "realtimePage"
    required property var facade

    function yMinimum(tagId) {
        if (tagId === "pressure") return 0
        return tagId === "speed" ? 0 : 0
    }

    function yMaximum(tagId) {
        if (tagId === "temperature") return 120
        if (tagId === "pressure") return 2
        if (tagId === "speed") return 3000
        return 500
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: Math.max(root.width - Theme.spacingLarge * 2, 760)
            spacing: Theme.spacingLarge

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Label {
                        text: "实时监控"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.titleSize
                        font.weight: Font.Bold
                    }
                    Label {
                        text: "采集状态、最近 60 秒统计与目标转速写入"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.bodySize
                    }
                }
                Item { Layout.fillWidth: true }
                StatusBadge {
                    text: facade.deviceModel.connectionStateText
                    tone: facade.deviceModel.connectionState === 2 ? "healthy"
                          : facade.deviceModel.connectionState === 3
                            || facade.deviceModel.connectionState === 1
                          ? "warning"
                          : facade.deviceModel.connectionState === 5
                          ? "error" : "neutral"
                }
                Button {
                    objectName: "connectButton"
                    text: "连接设备"
                    implicitHeight: Theme.controlHeight
                    enabled: facade.initialized && !facade.commandBusy
                    Accessible.name: text
                    onClicked: facade.connectDevice()
                }
                Button {
                    objectName: "disconnectButton"
                    text: "断开连接"
                    implicitHeight: Theme.controlHeight
                    enabled: facade.initialized && !facade.commandBusy
                    Accessible.name: text
                    onClicked: facade.disconnectDevice()
                }
            }

            Flow {
                id: metricsFlow
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: Theme.spacingMedium

                Repeater {
                    model: facade.realtimeModel

                    MetricCard {
                        width: metricsFlow.width >= 900
                               ? (metricsFlow.width - Theme.spacingMedium * 2) / 3
                               : (metricsFlow.width - Theme.spacingMedium) / 2
                        displayName: model.displayName
                        unit: model.unit
                        currentValue: model.currentValue
                        minimumValue: model.minimumValue
                        maximumValue: model.maximumValue
                        averageValue: model.averageValue
                        quality: model.quality
                        qualityText: model.qualityText
                        timestampText: model.timestampText
                    }
                }
            }

            SectionPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 430
                title: "最近 60 秒趋势"

                RowLayout {
                    Layout.fillWidth: true

                    ComboBox {
                        id: trendSelector
                        Layout.preferredWidth: 210
                        implicitHeight: Theme.controlHeight
                        textRole: "text"
                        model: [
                            {"text": "温度 / ℃", "value": "temperature"},
                            {"text": "压力 / MPa", "value": "pressure"},
                            {"text": "转速 / rpm", "value": "speed"},
                            {"text": "电压 / V", "value": "voltage"}
                        ]
                        onActivated: {
                            facade.selectTrendTag(model[currentIndex].value)
                            valueAxis.min = root.yMinimum(model[currentIndex].value)
                            valueAxis.max = root.yMaximum(model[currentIndex].value)
                        }
                    }
                    Button {
                        text: facade.displayPaused ? "恢复显示" : "暂停显示"
                        implicitHeight: Theme.controlHeight
                        onClicked: facade.setDisplayPaused(!facade.displayPaused)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: facade.displayPaused
                              ? "图形已暂停；采集、报警和存储仍继续"
                              : "图形跟随最新 Good 数据刷新"
                        color: facade.displayPaused
                               ? Theme.warning : Theme.textSecondary
                        horizontalAlignment: Text.AlignRight
                    }
                }

                ChartView {
                    id: trendChart
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    antialiasing: true
                    legend.visible: false
                    backgroundColor: Theme.surface
                    plotAreaColor: "#FAFCFE"

                    DateTimeAxis {
                        id: timeAxis
                        format: "HH:mm:ss"
                        min: new Date(Date.now() - 60000)
                        max: new Date()
                        titleText: "本地时间"
                    }
                    ValueAxis {
                        id: valueAxis
                        min: 0
                        max: 120
                        titleText: "工程值"
                    }
                    LineSeries {
                        id: trendSeries
                        axisX: timeAxis
                        axisY: valueAxis
                        color: Theme.primary
                        width: 2
                    }
                    VXYModelMapper {
                        model: facade.trendModel
                        series: trendSeries
                        firstRow: 0
                        rowCount: facade.trendModel.pointCount
                        xColumn: 0
                        yColumn: 1
                    }
                    Timer {
                        interval: 1000
                        repeat: true
                        running: !facade.displayPaused
                        onTriggered: {
                            timeAxis.min = new Date(Date.now() - 60000)
                            timeAxis.max = new Date()
                        }
                    }
                }
            }

            SectionPanel {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingLarge
                title: "目标转速写入"

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "目标转速 (rpm)"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.bodySize
                    }
                    TextField {
                        id: targetSpeed
                        objectName: "targetSpeedField"
                        Layout.preferredWidth: 240
                        implicitHeight: Theme.controlHeight
                        placeholderText: "0 - 65535"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 0; top: 65535 }
                        Accessible.name: "目标转速"
                    }
                    Button {
                        objectName: "writeTargetSpeedButton"
                        text: "写入目标转速"
                        implicitHeight: Theme.controlHeight
                        enabled: targetSpeed.acceptableInput
                                 && facade.initialized
                                 && !facade.commandBusy
                        onClicked: facade.writeTargetSpeed(Number(targetSpeed.text))
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
