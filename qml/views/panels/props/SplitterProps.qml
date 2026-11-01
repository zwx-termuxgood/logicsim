import QtQuick
// 引入 QtQuick 基础模块：Column/Text/Rectangle 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：TextField 输入框在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Column {
    // 分线器/集线器属性面板：选中 splitter 或 hub 时显示分段编辑
    id: splitterProps
    // 元素 id，供外部与内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 selectedComp、circuitRef 等

    property alias splitterSplitsInput: splitterSplitsInput
    // 把分段输入框暴露给外部（LeftPanel/PropertyPanel 需要直接读写）

    visible: main !== null
             && main.selectedComp !== null
             && main.selectedComp !== undefined
             && (main.selectedComp.type === "splitter"
                 || main.selectedComp.type === "hub")
    // 仅当选中元件是分线器或集线器时可见
    spacing: 4
    // 子项间距

    Text {
        // 面板标签
        text: "分段（逗号分隔，和 = 位宽）"
        // 标签说明格式与约束
        color: Theme.textGray; font.pixelSize: Theme.fsSmall
        // 次要文字色与小字号
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
            // 分段文本输入框
            id: splitterSplitsInput
            // 控件 id，供 alias 暴露
            anchors.fill: parent
            // 填满外框
            anchors.leftMargin: 5; anchors.rightMargin: 5
            // 左右内边距
            color: Theme.text
            // 文字色
            font.pixelSize: Theme.fsSmall; font.family: "monospace"
            // 小字号等宽字体，便于对齐数字
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
                main.circuitRef.setSplitterSplits(main.selectedCompId, text)
                // 把输入文本提交为分段方案
            }
        }
    }
    Text {
        // 底部说明：区分分线器与集线器语义
        text: (main && main.selectedComp && main.selectedComp.type === "splitter")
              ? "分线器：1 输入 → N 输出"
              : "集线器：N 输入 → 1 输出"
        // 按类型显示不同说明
        color: Theme.textFaint
        // 极弱文字色
        font.pixelSize: Theme.fsTiny
        // 极小字号
        width: parent.width; wrapMode: Text.Wrap
        // 宽度铺满并自动换行
    }
}