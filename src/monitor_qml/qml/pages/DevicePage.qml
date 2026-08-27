import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml
import "../components"

Item {
    id: root
    objectName: "devicePage"
    required property var facade

    function loadFacts() {
        protocolSelector.currentIndex = facade.protocolModel.indexOfKey(
                    facade.deviceModel.protocolKey)
        hostField.text = facade.deviceModel.host
        portField.text = String(facade.deviceModel.port)
        unitIdField.text = String(facade.deviceModel.unitId)
        pollField.text = String(facade.deviceModel.pollIntervalMs)
        timeoutField.text = String(facade.deviceModel.timeoutMs)
        enabledSwitch.checked = facade.deviceModel.enabled
    }

    function validateAndSave() {
        hostField.errorText = hostField.text.trim().length === 0
                ? "主机地址不能为空" : ""
        portField.errorText = portField.acceptableInput
                ? "" : "端口必须在 1 到 65535 之间"
        unitIdField.errorText = unitIdField.acceptableInput
                ? "" : "Unit ID 必须在 1 到 247 之间"
        pollField.errorText = pollField.acceptableInput
                ? "" : "轮询间隔不能小于 50 ms"
        timeoutField.errorText = timeoutField.acceptableInput
                ? "" : "超时时间必须大于 0 ms"

        if (protocolSelector.currentIndex < 0
                || hostField.errorText.length > 0
                || portField.errorText.length > 0
                || unitIdField.errorText.length > 0
                || pollField.errorText.length > 0
                || timeoutField.errorText.length > 0) {
            return
        }
        facade.saveDevice(protocolSelector.currentValue,
                          hostField.text,
                          Number(portField.text),
                          Number(unitIdField.text),
                          Number(pollField.text),
                          Number(timeoutField.text),
                          enabledSwitch.checked)
    }

    Component.onCompleted: loadFacts()

    Connections {
        target: facade.deviceModel
        function onDeviceChanged() { root.loadFacts() }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: Math.max(root.width - Theme.spacingLarge * 2, 760)
            spacing: Theme.spacingLarge

            ColumnLayout {
                Label {
                    text: "设备管理"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.titleSize
                    font.weight: Font.Bold
                }
                Label {
                    text: "编辑单设备连接参数；保存成功回传后才更新事实状态"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.bodySize
                }
            }

            SectionPanel {
                Layout.fillWidth: true
                title: "当前设备"

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: Theme.spacingLarge
                    rowSpacing: Theme.spacingMedium

                    Label { text: "设备 ID"; color: Theme.textSecondary }
                    Label {
                        text: facade.deviceModel.deviceId || "尚未初始化"
                        color: Theme.textPrimary
                        font.weight: Font.DemiBold
                    }
                    Label { text: "协议"; color: Theme.textSecondary }
                    Label {
                        text: facade.deviceModel.protocolName || "尚未加载"
                        color: Theme.textPrimary
                        font.weight: Font.DemiBold
                    }
                    Label { text: "连接状态"; color: Theme.textSecondary }
                    StatusBadge {
                        text: facade.deviceModel.connectionStateText
                        tone: facade.deviceModel.connectionState === 2
                              ? "healthy"
                              : facade.deviceModel.connectionState === 5
                              ? "error" : "neutral"
                    }
                    Label { text: "最近通信"; color: Theme.textSecondary }
                    Label {
                        text: facade.deviceModel.lastCommunicationText
                        color: Theme.textPrimary
                    }
                }
            }

            SectionPanel {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingLarge
                title: "连接配置"

                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 820 ? 2 : 1
                    columnSpacing: Theme.spacingLarge
                    rowSpacing: Theme.spacingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "通信协议"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.bodySize
                            font.weight: Font.DemiBold
                        }
                        ComboBox {
                            id: protocolSelector
                            Layout.fillWidth: true
                            Layout.minimumHeight: Theme.controlHeight
                            model: facade.protocolModel
                            textRole: "displayName"
                            valueRole: "protocolKey"
                            Accessible.name: "通信协议"
                        }
                    }

                    LabeledField {
                        id: hostField
                        objectName: "deviceHostField"
                        Layout.fillWidth: true
                        label: "主机地址"
                        placeholderText: "例如 127.0.0.1"
                    }
                    LabeledField {
                        id: portField
                        Layout.fillWidth: true
                        label: "端口"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 65535 }
                    }
                    LabeledField {
                        id: unitIdField
                        Layout.fillWidth: true
                        label: "Unit ID"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 247 }
                    }
                    LabeledField {
                        id: pollField
                        Layout.fillWidth: true
                        label: "轮询间隔 (ms)"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 50; top: 60000 }
                    }
                    LabeledField {
                        id: timeoutField
                        Layout.fillWidth: true
                        label: "超时时间 (ms)"
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 60000 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Switch {
                        id: enabledSwitch
                        text: "启用设备"
                        Accessible.name: text
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "saveDeviceButton"
                        text: facade.commandBusy ? "正在保存…" : "保存并应用"
                        implicitHeight: Theme.controlHeight
                        enabled: facade.initialized && !facade.commandBusy
                        onClicked: root.validateAndSave()
                    }
                }
            }
        }
    }
}
