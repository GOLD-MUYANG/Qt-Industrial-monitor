import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IndustrialMonitor.Qml
import "pages"

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 680
    visible: true
    title: "工业设备监控系统"
    color: Theme.appBackground

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 238
            Layout.fillHeight: true
            color: Theme.sidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: "INDUSTRIAL"
                        color: "#8FC7F2"
                        font.pixelSize: 12
                        font.letterSpacing: 2
                    }
                    Label {
                        text: "设备监控中心"
                        color: "white"
                        font.pixelSize: 21
                        font.weight: Font.Bold
                    }
                    Label {
                        text: "QML 并行前端"
                        color: "#B8CADB"
                        font.pixelSize: 13
                    }
                }

                ListView {
                    id: navigation
                    objectName: "navigationList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    currentIndex: 0
                    model: ["实时监控", "设备管理", "报警中心"]

                    delegate: ItemDelegate {
                        id: navigationDelegate
                        required property string modelData
                        required property int index
                        width: navigation.width
                        height: 48
                        text: modelData
                        highlighted: navigation.currentIndex === index
                        font.pixelSize: Theme.bodySize
                        Accessible.name: modelData
                        onClicked: navigation.currentIndex = index

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: navigationDelegate.highlighted
                                   ? Theme.sidebarActive : "transparent"
                        }
                        contentItem: Label {
                            text: navigationDelegate.text
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            font: navigationDelegate.font
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: appFacade.initialized
                          ? "后端已初始化" : "后端初始化中"
                    color: appFacade.initialized ? "#8EE3B5" : "#FFD08A"
                    font.pixelSize: 13
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                objectName: "globalStatusBar"
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                color: appFacade.statusHealthy ? "#EDF8F2" : "#FFF0EF"
                border.color: appFacade.statusHealthy ? "#B8DEC9" : "#F1B5AF"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    anchors.rightMargin: Theme.spacingLarge

                    StatusBadge {
                        text: appFacade.statusHealthy ? "系统状态" : "需要处理"
                        tone: appFacade.statusHealthy ? "healthy" : "error"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: appFacade.statusMessage
                        color: appFacade.statusHealthy
                               ? Theme.success : Theme.danger
                        font.pixelSize: Theme.bodySize
                        elide: Text.ElideRight
                    }
                    BusyIndicator {
                        running: appFacade.commandBusy
                        visible: running
                        implicitWidth: 28
                        implicitHeight: 28
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: navigation.currentIndex

                RealtimePage {
                    facade: appFacade
                }
                DevicePage {
                    facade: appFacade
                }
                AlarmPage {
                    facade: appFacade
                }
            }
        }
    }
}
