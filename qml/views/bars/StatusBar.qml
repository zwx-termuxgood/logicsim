import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text 等基础元素
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Rectangle {
    // 状态栏根元素：一个贴底矩形条
    id: statusBar
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：用于访问 mode、statusText、errorList 等状态

    anchors.left: parent.left
    // 左侧对齐父元素
    anchors.right: parent.right
    // 右侧对齐父元素
    anchors.bottom: parent.bottom
    // 底部对齐父元素
    height: Theme.statusBarH
    // 高度取主题定义的状态栏高度
    color: Theme.bg
    // 背景色取主题背景色

    Text {
        // 状态文本：显示当前模式与状态消息
        anchors.left: parent.left; anchors.leftMargin: 8
        // 左侧对齐并留 8px 边距
        anchors.verticalCenter: parent.verticalCenter
        // 相对状态栏垂直居中
        text: "[" + main.mode + "] " + main.statusText
        // 文案拼接当前模式与状态消息
        color: main.errorList.length > 0 ? Theme.accentRed : Theme.textWeak
        // 有错误时用强调红，否则用弱化文字色
        font.pixelSize: Theme.fsSmall
        // 字号取主题小号
        elide: Text.ElideRight
        // 文本过长时右侧省略，避免溢出状态栏
    }
}