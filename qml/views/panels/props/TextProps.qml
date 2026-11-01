import QtQuick
// 引入 QtQuick 基础模块：Column/Text/Rectangle 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：TextField 输入框在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Column {
    // 文本元件属性面板：选中 text 元件时显示内容编辑
    id: textProps
    // 元素 id，供外部与内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 selectedComp、circuitRef 等

    property alias textContentField: textContentField
    // 把内容输入框暴露给外部（LeftPanel/PropertyPanel 需要直接读写）

    visible: main !== null
             && main.selectedComp !== null
             && main.selectedComp !== undefined
             && main.selectedComp.type === "text"
    // 仅当选中元件存在且类型为 text 时可见
    spacing: 4
    // 子项间距

    Text {
        // 面板标签
        text: "文字内容"; color: Theme.textGray
        // 标签文字与次要文字色
        font.pixelSize: Theme.fsSmall
        // 小字号
    }
    Rectangle {
        // 输入框外框
        width: parent.width; height: 26
        // 宽度铺满，固定高度
        color: Theme.bgInput
        // 输入框背景色
        border.color: Theme.borderStrong
        // 边框颜色
        border.width: 1; radius: Theme.smallRadius
        // 边框宽度与圆角
        TextField {
            // 文本内容输入框
            id: textContentField
            // 控件 id，供 alias 暴露
            anchors.fill: parent
            // 填满外框
            anchors.leftMargin: 5; anchors.rightMargin: 5
            // 左右内边距
            color: Theme.text
            // 文字色
            font.pixelSize: Theme.fsSmall
            // 小字号
            background: null; selectByMouse: true
            // 不画自带背景；支持鼠标选择
            readOnly: main ? main.circuitViewOnly : true
            // 只读模式下不可编辑
            onEditingFinished: {
                // 编辑结束处理
                if (!main) return
                // main 为空则忽略
                if (main.selectedCompId === "" || main.circuitViewOnly) return
                // 无选中元件或只读模式忽略
                main.circuitRef.setTextContent(main.selectedCompId, text)
                // 把输入文本提交为文本元件内容
            }
        }
    }
}