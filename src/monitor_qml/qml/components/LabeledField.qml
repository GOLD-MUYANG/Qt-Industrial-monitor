import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml

ColumnLayout {
    id: root

    property string label: "字段"
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property var validator: null
    property int inputMethodHints: Qt.ImhNone
    property string errorText: ""
    readonly property bool acceptableInput: input.acceptableInput

    spacing: 6

    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.bodySize
        font.weight: Font.DemiBold
    }

    TextField {
        id: input
        Layout.fillWidth: true
        Layout.minimumHeight: Theme.controlHeight
        validator: root.validator
        inputMethodHints: root.inputMethodHints
        selectByMouse: true
        Accessible.name: root.label
    }

    Label {
        Layout.fillWidth: true
        visible: root.errorText.length > 0
        text: root.errorText
        color: Theme.danger
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }
}
