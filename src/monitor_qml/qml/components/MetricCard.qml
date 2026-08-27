import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml

Rectangle {
    id: root

    property string displayName: "测点"
    property string unit: ""
    property var currentValue
    property var minimumValue
    property var maximumValue
    property var averageValue
    property int quality: 2
    property string qualityText: "尚无数据"
    property string timestampText: "尚无数据"

    function formatNumber(value) {
        return typeof value === "number" && isFinite(value)
                ? value.toFixed(2) : "—"
    }

    implicitHeight: 180
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border

    Accessible.name: displayName + "，当前值 "
                     + formatNumber(currentValue) + " " + unit

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.displayName
                color: Theme.textSecondary
                font.pixelSize: Theme.bodySize
                font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            StatusBadge {
                text: root.qualityText
                tone: root.quality === 0 ? "healthy"
                      : root.quality === 1 ? "warning" : "error"
            }
        }

        Label {
            text: root.formatNumber(root.currentValue)
                  + (root.unit.length > 0 ? " " + root.unit : "")
            color: Theme.textPrimary
            font.pixelSize: 30
            font.weight: Font.Bold
        }

        Label {
            Layout.fillWidth: true
            text: "最小 " + root.formatNumber(root.minimumValue)
                  + "  ·  最大 " + root.formatNumber(root.maximumValue)
                  + "  ·  平均 " + root.formatNumber(root.averageValue)
            color: Theme.textSecondary
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: "更新：" + root.timestampText
            color: Theme.textSecondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
}
