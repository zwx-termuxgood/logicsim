import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Popup 弹窗基类在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Popup {
    // 上下文选择弹窗：列出根与所有子电路，点击切换编辑上下文
    id: ctxPopup
    // 元素 id，供外部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 editCtxList、circuitRef 等
    property Item anchorItem: null
    // 锚点元素：弹窗位置据此计算（通常为顶栏上下文按钮）

    x: anchorItem ? Math.max(10, anchorItem.mapToItem(null, 0, 0).x + anchorItem.width - 240) : 10
    // 弹窗 x：优先对齐锚点右边缘（右对齐弹窗宽度 240），并保证不小于 10；无锚点时固定 10
    y: anchorItem ? anchorItem.mapToItem(null, 0, anchorItem.height).y : 50
    // 弹窗 y：紧贴锚点底边；无锚点时固定 50
    width: 240
    // 弹窗宽度
    height: Math.min(320, 36 * Math.max(1, main.editCtxList.length) + 16)
    // 弹窗高度：按条目数估算（每条 36px），上限 320，至少按 1 条算再留 16px
    modal: true
    // 模态：阻止用户操作背后的界面
    focus: true
    // 打开时获取焦点，支持键盘操作
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // 按 Esc 或点击外部关闭

    background: Rectangle {
        // 自定义弹窗背景
        color: Theme.bgDialog
        // 背景色取主题对话框背景色
        border.color: Theme.borderDialog
        // 边框颜色取主题对话框边框色
        border.width: 1
        // 边框宽度
        radius: 8
        // 圆角半径
    }

    contentItem: Column {
        // 内容区：纵向列表
        spacing: 0
        // 条目之间无间距
        Repeater {
            // 按可编辑上下文列表生成条目
            model: main.editCtxList
            // 数据模型：根 + 所有子电路
            delegate: Rectangle {
                // 单个上下文条目
                width: ctxPopup.width; height: 36
                // 条目宽等于弹窗宽，高 36
                color: (main.circuitRef.contextId === modelData.id && !main.circuitViewOnly)
                ? Theme.bgButtonHover
                : (ma.pressed ? Theme.bgButtonHover : "transparent")
                // 颜色：当前上下文高亮，其次按下高亮，否则透明

                Text {
                    // 上下文名称文本
                    anchors.left: parent.left; anchors.leftMargin: 12
                    // 左侧对齐并留 12px 边距
                    anchors.right: parent.right; anchors.rightMargin: 12
                    // 右侧对齐并留 12px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    text: modelData.name + (modelData.id === "" ? "" : "  (子电路)")
                    // 根上下文只显示名称，子电路追加"(子电路)"标记
                    color: (main.circuitRef.contextId === modelData.id && !main.circuitViewOnly)
                    ? Theme.textWhite : Theme.text
                    // 当前上下文白字，其余常规文字色
                    font.pixelSize: Theme.fsNormal
                    // 常规字号
                    elide: Text.ElideRight
                    // 过长时右侧省略
                }
                MouseArea {
                    // 条目点击区
                    id: ma; anchors.fill: parent
                    // 填满条目
                    onClicked: {
                        // 点击处理
                        main.circuitRef.switchEditContext(modelData.id)
                        // 切换到该编辑上下文
                        ctxPopup.close()
                        // 关闭弹窗
                    }
                }
            }
        }
    }
}