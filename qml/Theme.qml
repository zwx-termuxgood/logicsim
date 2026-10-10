pragma Singleton
import QtQuick

QtObject {
    // 背景
    readonly property color bg:               "#1a1a1a"
    readonly property color bgBar:            "#1e1e1e"
    readonly property color bgPanel:          "#222222"
    readonly property color bgDialogHeader:   "#222222"
    readonly property color bgDialog:         "#2a2a2a"
    readonly property color bgInput:          "#1a1a1a"
    readonly property color bgButton:         "#2d2d2d"
    readonly property color bgButtonHover:    "#3a3a3a"
    readonly property color bgButtonActive:   "#4a4a4a"
    readonly property color bgButtonDanger:   "#5a1a1a"
    readonly property color bgButtonDangerHi: "#7a2020"
    readonly property color bgButtonSuccess:  "#238636"
    readonly property color bgButtonSuccessHi:"#2ea043"
    readonly property color bgButtonRun:      "#2e5e2e"
    readonly property color bgButtonRunHi:    "#3a5a3a"
    readonly property color bgButtonReset:    "#3a1a1a"
    readonly property color bgButtonResetHi:  "#5a1a1a"
    readonly property color bgReadonly:       "#3a2a10"
    readonly property color bgBack:           "#4a4a4a"
    readonly property color bgBackHi:         "#5a5a5a"

    // 边框
    readonly property color border:           "#333333"
    readonly property color borderStrong:     "#444444"
    readonly property color borderWeak:       "#383838"
    readonly property color borderDialog:     "#555555"
    readonly property color borderBtnActive:  "#cccccc"
    readonly property color borderDanger:     "#8a3030"
    readonly property color borderDangerHi:   "#aa4040"
    readonly property color borderSuccess:    "#2ea043"
    readonly property color borderSub:        "#5a3f8a"
    readonly property color borderReadonly:   "#5a3f10"
    readonly property color borderGray:       "#999999"

    // 文字
    readonly property color text:             "#dddddd"
    readonly property color textStrong:       "#eeeeee"
    readonly property color textWeak:         "#888888"
    readonly property color textMuted:        "#666666"
    readonly property color textWhite:        "#ffffff"
    readonly property color textGray:         "#aaaaaa"
    readonly property color textFaint:        "#777777"

    // 强调
    readonly property color accent:           "#58a6ff"
    readonly property color accentPurple:     "#a371f7"
    readonly property color accentOrange:     "#ffb74d"
    readonly property color accentGreen:      "#4caf50"
    readonly property color accentRed:        "#e05252"
    readonly property color accentYellow:     "#ffd700"
    readonly property color accentYellowHi:   "#ffeb3b"
    readonly property color accentDirty:      "#ffb74d"
    readonly property color wireOff:          "#666666"
    readonly property color grid:             "#252525"

    // 尺寸
    readonly property int topBarH:            42
    readonly property int clockBarH:          36
    readonly property int readonlyBannerH:    30
    readonly property int leftPanelW:         200
    readonly property int statusBarH:         22
    readonly property int buttonRadius:       5
    readonly property int dialogRadius:       10
    readonly property int smallRadius:        4
    readonly property int collapsedBarH:      32
    readonly property int collapsedBarW:      34

    // 字体
    readonly property int fsTiny:             10
    readonly property int fsSmall:            11
    readonly property int fsNormal:           12
    readonly property int fsMedium:           13
    readonly property int fsLarge:            14
}