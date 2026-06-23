import QtQuick

// 全局主题色板，由 Main.qml 实例化后通过 property 传给子组件
QtObject {
    readonly property color window: "#1e1e1e"
    readonly property color panel: "#252526"
    readonly property color panelHeader: "#2d2d30"
    readonly property color surface: "#2a2d2e"
    readonly property color border: "#3f3f46"
    readonly property color text: "#cccccc"
    readonly property color muted: "#808080"
    readonly property color accent: "#007acc"
    readonly property color accentSoft: "#0e639c"
    readonly property color accentHover: "#094771"
    readonly property color success: "#73c991"
    readonly property color warning: "#f48771"
    readonly property color danger: "#f44747"
}
