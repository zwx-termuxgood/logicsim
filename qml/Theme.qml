pragma Singleton
// 声明为单例：整个应用共享同一个 Theme 实例，通过 import LogicSim 使用
import QtQuick
// 引入 QtQuick：QtObject 基类与 color 类型在此

QtObject {
    // 主题单例：集中定义应用的颜色、尺寸、字号等视觉常量
    // 背景
    readonly property color bg:               "#1a1a1a"
    // 应用主背景色（深黑）
    readonly property color bgBar:            "#1e1e1e"
    // 工具条/状态栏背景色
    readonly property color bgPanel:          "#222222"
    // 面板背景色（左侧面板/顶栏）
    readonly property color bgDialogHeader:   "#222222"
    // 弹窗标题栏背景色
    readonly property color bgDialog:         "#2a2a2a"
    // 对话框背景色
    readonly property color bgInput:          "#1a1a1a"
    // 输入框背景色
    readonly property color bgButton:         "#2d2d2d"
    // 按钮默认背景色
    readonly property color bgButtonHover:    "#3a3a3a"
    // 按钮悬停/按下背景色
    readonly property color bgButtonActive:   "#4a4a4a"
    // 按钮激活态背景色
    readonly property color bgButtonDanger:   "#5a1a1a"
    // 危险操作按钮背景色
    readonly property color bgButtonDangerHi: "#7a2020"
    // 危险操作按钮按下背景色
    readonly property color bgButtonSuccess:  "#238636"
    // 成功操作按钮背景色
    readonly property color bgButtonSuccessHi:"#2ea043"
    // 成功操作按钮按下背景色
    readonly property color bgButtonRun:      "#2e5e2e"
    // 运行类按钮背景色
    readonly property color bgButtonRunHi:    "#3a5a3a"
    // 运行类按钮按下背景色
    readonly property color bgButtonReset:    "#3a1a1a"
    // 复位类按钮背景色
    readonly property color bgButtonResetHi:  "#5a1a1a"
    // 复位类按钮按下背景色
    readonly property color bgReadonly:       "#3a2a10"
    // 只读提示条背景色
    readonly property color bgBack:           "#4a4a4a"
    // 返回按钮背景色
    readonly property color bgBackHi:         "#5a5a5a"
    // 返回按钮按下背景色

    // 边框
    readonly property color border:           "#333333"
    // 通用边框色
    readonly property color borderStrong:     "#444444"
    // 强调边框色
    readonly property color borderWeak:       "#383838"
    // 弱化边框色
    readonly property color borderDialog:     "#555555"
    // 对话框边框色
    readonly property color borderBtnActive:  "#cccccc"
    // 激活按钮边框色
    readonly property color borderDanger:     "#8a3030"
    // 危险操作边框色
    readonly property color borderDangerHi:   "#aa4040"
    // 危险操作按下边框色
    readonly property color borderSuccess:    "#2ea043"
    // 成功操作边框色
    readonly property color borderSub:        "#5a3f8a"
    // 子电路紫色边框色
    readonly property color borderReadonly:   "#5a3f10"
    // 只读提示条边框色
    readonly property color borderGray:       "#999999"
    // 灰色边框色

    // 文字
    readonly property color text:             "#dddddd"
    // 常规文字色
    readonly property color textStrong:       "#eeeeee"
    // 强调文字色（标题）
    readonly property color textWeak:         "#888888"
    // 弱化文字色（次要标签）
    readonly property color textMuted:        "#666666"
    // 极弱文字色（占位提示）
    readonly property color textWhite:        "#ffffff"
    // 纯白文字（彩色按钮上）
    readonly property color textGray:         "#aaaaaa"
    // 灰色文字（说明）
    readonly property color textFaint:        "#777777"
    // 极淡文字（辅助说明）

    // 强调
    readonly property color accent:           "#58a6ff"
    // 主强调色（蓝）
    readonly property color accentPurple:     "#a371f7"
    // 子电路强调色（紫）
    readonly property color accentOrange:     "#ffb74d"
    // 只读/高阻强调色（橙）
    readonly property color accentGreen:      "#4caf50"
    // 高电平强调色（绿）
    readonly property color accentRed:        "#e05252"
    // 错误强调色（红）
    readonly property color accentYellow:     "#ffd700"
    // LED 默认色（金）
    readonly property color accentYellowHi:   "#ffeb3b"
    // LED 高亮色
    readonly property color accentDirty:      "#ffb74d"
    // 未保存标记色
    readonly property color wireOff:          "#666666"
    // 低电平连线色
    readonly property color grid:             "#252525"
    // 背景网格线色

    // 尺寸
    readonly property int topBarH:            42
    // 顶栏高度
    readonly property int clockBarH:          36
    // 时钟栏高度
    readonly property int readonlyBannerH:    30
    // 只读提示条高度
    readonly property int leftPanelW:         200
    // 左侧面板宽度
    readonly property int statusBarH:         22
    // 状态栏高度
    readonly property int buttonRadius:       5
    // 按钮圆角半径
    readonly property int dialogRadius:       10
    // 对话框圆角半径
    readonly property int smallRadius:        4
    // 小元素圆角半径
    readonly property int collapsedBarH:      32
    // 收起条高度
    readonly property int collapsedBarW:      34
    // 收起条宽度

    // 字体
    readonly property int fsTiny:             10
    // 极小字号
    readonly property int fsSmall:            11
    // 小字号
    readonly property int fsNormal:           12
    // 常规字号
    readonly property int fsMedium:           13
    // 中号字号
    readonly property int fsLarge:            14
    // 大字号
}