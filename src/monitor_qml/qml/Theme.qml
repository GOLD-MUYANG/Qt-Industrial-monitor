pragma Singleton

import QtQuick

QtObject {
    readonly property color appBackground: "#F4F7FB"
    readonly property color sidebar: "#102A43"
    readonly property color sidebarActive: "#1F4F78"
    readonly property color surface: "#FFFFFF"
    readonly property color surfaceMuted: "#EAF0F6"
    readonly property color border: "#D6E0EA"
    readonly property color textPrimary: "#102A43"
    readonly property color textSecondary: "#52667A"
    readonly property color primary: "#1769AA"
    readonly property color primaryDark: "#0F4C81"
    readonly property color success: "#147D4F"
    readonly property color warning: "#A15C00"
    readonly property color danger: "#B42318"
    readonly property color focus: "#2E90FA"

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 10
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int bodySize: 15
    readonly property int titleSize: 24
    readonly property int controlHeight: 44
}
