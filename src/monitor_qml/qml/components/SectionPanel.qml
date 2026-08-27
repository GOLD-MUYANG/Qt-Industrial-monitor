import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml

Rectangle {
    id: root

    property string title: "分组"
    default property alias content: contentColumn.data

    implicitHeight: panelLayout.implicitHeight + Theme.spacingLarge * 2
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border

    ColumnLayout {
        id: panelLayout
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        Label {
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: 18
            font.weight: Font.Bold
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: Theme.spacingMedium
        }
    }
}
