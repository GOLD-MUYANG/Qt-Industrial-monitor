import QtQuick
import QtQuick.Controls
import IndustrialMonitor.Qml

Rectangle {
    id: root

    property string text: "未知"
    property string tone: "neutral"

    implicitWidth: badgeLabel.implicitWidth + 24
    implicitHeight: 32
    radius: implicitHeight / 2
    color: tone === "healthy" ? "#E8F5EE"
          : tone === "warning" ? "#FFF4E5"
          : tone === "error" ? "#FDECEC"
          : Theme.surfaceMuted
    border.color: tone === "healthy" ? "#8BC9AA"
                  : tone === "warning" ? "#E8B36A"
                  : tone === "error" ? "#E6A29C"
                  : Theme.border

    Label {
        id: badgeLabel
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: root.tone === "healthy" ? Theme.success
               : root.tone === "warning" ? Theme.warning
               : root.tone === "error" ? Theme.danger
               : Theme.textSecondary
    }
}
