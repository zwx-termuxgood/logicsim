import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Row/Repeater/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：本文件按需使用（保持与其它栏组件一致的依赖）
import QtQuick.Layouts
// 引入 Layouts 模块：顶部上下文按钮内用 RowLayout 布局
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：Circuit 类型及其属性/方法、主题等

Rectangle {
    // 顶部工具栏根元素：一个贴顶矩形条
    id: topBar
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：用于访问模式、视图缩放、电路接口等

    property alias ctxBtn: ctxBtn
    // 把上下文切换按钮暴露给外部（ContextPopup 用作锚点）
    property alias menuBtn: menuBtn
    // 把菜单按钮暴露给外部（MenuPopup 用作锚点）

    anchors.top: parent.top
    // 顶部对齐父元素
    anchors.left: parent.left
    // 左侧对齐父元素
    anchors.right: parent.right
    // 右侧对齐父元素
    height: Theme.topBarH
    // 高度取主题定义的顶栏高度
    color: Theme.bgPanel
    // 背景色取主题面板背景色
    border.color: Theme.border
    // 边框颜色取主题边框色
    border.width: 1
    // 边框宽度 1px

    Flickable {
        // 可横向滑动的容器：窄屏时工具栏可左右滚动
        id: topFlick
        // 容器 id
        anchors.fill: parent
        // 填满根矩形
        contentWidth: topRow.implicitWidth + 12
        // 内容宽度 = 行隐式宽度 + 左右留白
        contentHeight: height
        // 内容高度等于容器高度（无纵向滚动）
        flickableDirection: Flickable.HorizontalFlick
        // 只允许横向拖动
        clip: true
        // 裁剪超出部分，避免滑动时溢出
        boundsBehavior: Flickable.StopAtBounds
        // 到达边界即停止，不做回弹

        Row {
            // 工具栏内容行
            id: topRow
            // 行 id
            x: 6
            // 左侧留白 6px
            y: 0
            // 顶部对齐
            height: parent.height
            // 行高等于容器高度
            spacing: 3
            // 子项间距 3px

            Text {
                // 应用标题
                text: "LogicSim"
                // 显示的应用名
                color: Theme.textStrong
                // 强调文字色
                font.pixelSize: Theme.fsLarge
                // 较大字号
                font.bold: true
                // 加粗
                anchors.verticalCenter: parent.verticalCenter
                // 相对行垂直居中
            }
            Rectangle {
                // 标题后的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Rectangle {
                // 菜单按钮
                id: menuBtn
                // 按钮 id，供外部作为弹窗锚点
                width: 52; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                color: menuBtnMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "菜单"
                    // 居中文字"菜单"
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与字号
                }
                MouseArea {
                    // 点击区
                    id: menuBtnMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: main.menuPopupRef.open()
                    // 点击打开主菜单弹窗
                }
            }

            Rectangle {
                // 菜单后的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Repeater {
                // 模式切换按钮组：编辑/控制/选择
                model: [ { id: "edit", name: "编辑" },
                         { id: "control", name: "控制" },
                         { id: "select", name: "选择" } ]
                // 三种模式的数据模型
                delegate: Rectangle {
                    // 单个模式按钮
                    width: 44; height: 28; radius: Theme.buttonRadius
                    // 尺寸与圆角
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    color: main.mode === modelData.id ? Theme.bgButtonActive : Theme.bgButton
                    // 颜色：当前模式用激活色，否则普通
                    border.color: main.mode === modelData.id ? Theme.borderBtnActive : Theme.borderWeak
                    // 边框：当前模式用激活边框，否则弱边框
                    border.width: 1
                    // 边框宽度
                    opacity: main.circuitViewOnly ? 0.5 : 1
                    // 只读浏览模式下置半透明表示不可用

                    Text {
                        // 模式名
                        anchors.centerIn: parent
                        // 居中
                        text: modelData.name
                        // 显示模式名
                        color: main.mode === modelData.id ? Theme.textWhite : Theme.textGray
                        // 当前模式白字，否则灰字
                        font.pixelSize: Theme.fsSmall
                        // 字号
                    }
                    MouseArea {
                        // 点击区
                        anchors.fill: parent
                        // 填满按钮
                        enabled: !main.circuitViewOnly
                        // 只读模式禁用
                        onClicked: {
                            // 点击切换模式
                            main.mode = modelData.id
                            // 设置当前模式
                            main.wiring = false
                            // 取消进行中的拉线
                            main.placingType = ""
                            // 取消进行中的放置
                            if (modelData.id !== "select") main.selectedIds = []
                            // 非选择模式时清空多选
                        }
                    }
                }
            }

            // 可切换模式下的连线类型切换按钮
            Rectangle {
                // 线型切换前的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                visible: main.circuitRef.wireMode === "switchable"
                // 仅可切换模式显示
            }
            Rectangle {
                // 线型切换按钮
                id: wireTypeBtn
                // 按钮 id
                width: 78; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                visible: main.circuitRef.wireMode === "switchable"
                // 仅可切换模式显示
                color: wireTypeMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 按钮文字：当前线型
                    anchors.centerIn: parent
                    // 居中
                    text: main.circuitRef.currentWireType === "ortho" ? "⚡ 正交" : "〜 贝塞尔"
                    // 当前线型为正交则显示正交，否则贝塞尔
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与字号
                }
                MouseArea {
                    // 点击区
                    id: wireTypeMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: {
                        // 点击切换线型
                        var t = main.circuitRef.currentWireType
                        // 取当前线型
                        main.circuitRef.currentWireType = (t === "ortho") ? "bezier" : "ortho"
                        // 正交↔贝塞尔切换
                    }
                }
            }

            Rectangle {
                // 线型切换后的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Rectangle {
                // 撤销按钮
                width: 28; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: undoBtn.pressed && main.circuitRef.canUndo ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：可按且按下时高亮，否则普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                opacity: main.circuitRef.canUndo ? 1 : 0.4
                // 不可撤销时置半透明
                Text { anchors.centerIn: parent; text: "↶"; color: Theme.text; font.pixelSize: 15 }
                // 居中撤销图标
                MouseArea {
                    // 点击区
                    id: undoBtn; anchors.fill: parent
                    // 填满按钮
                    enabled: main.circuitRef.canUndo && !main.circuitViewOnly
                    // 可撤销且非只读时才启用
                    onClicked: main.circuitRef.undo()
                    // 点击执行撤销
                }
            }
            Rectangle {
                // 重做按钮
                width: 28; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: redoBtn.pressed && main.circuitRef.canRedo ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：可按且按下时高亮，否则普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                opacity: main.circuitRef.canRedo ? 1 : 0.4
                // 不可重做时置半透明
                Text { anchors.centerIn: parent; text: "↷"; color: Theme.text; font.pixelSize: 15 }
                // 居中重做图标
                MouseArea {
                    // 点击区
                    id: redoBtn; anchors.fill: parent
                    // 填满按钮
                    enabled: main.circuitRef.canRedo && !main.circuitViewOnly
                    // 可重做且非只读时才启用
                    onClicked: main.circuitRef.redo()
                    // 点击执行重做
                }
            }

            Rectangle {
                // 撤销/重做后的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Rectangle {
                // 缩小按钮
                width: 28; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: zo.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text { anchors.centerIn: parent; text: "−"; color: Theme.text; font.pixelSize: 15 }
                // 居中减号图标
                MouseArea { id: zo; anchors.fill: parent; onClicked: main.zoomOut() }
                // 点击缩小视图
            }
            Rectangle {
                // 缩放比例按钮：显示当前百分比，点击重置视图
                width: 46; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: rst.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 缩放百分比文字
                    anchors.centerIn: parent
                    // 居中
                    text: Math.round(main.viewScale * 100) + "%"
                    // 当前缩放换算成百分比
                    color: Theme.text; font.pixelSize: Theme.fsTiny
                    // 文字颜色与小字号
                }
                MouseArea { id: rst; anchors.fill: parent; onClicked: main.resetView() }
                // 点击重置视图
            }
            Rectangle {
                // 放大按钮
                width: 28; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: zi.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text { anchors.centerIn: parent; text: "+"; color: Theme.text; font.pixelSize: 15 }
                // 居中加号图标
                MouseArea { id: zi; anchors.fill: parent; onClicked: main.zoomIn() }
                // 点击放大视图
            }

            Rectangle {
                // 缩放按钮组后的分隔线
                width: 1; height: 22; color: Theme.borderStrong
                // 竖直分隔线
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Rectangle {
                // 上下文切换按钮：显示当前上下文名，点击弹出上下文列表
                id: ctxBtn
                // 按钮 id，供外部作为弹窗锚点
                width: 150; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: ctxBtnMouse.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderDialog; border.width: 1
                // 对话框色边框
                RowLayout {
                    // 按钮内水平布局：名称 + 下拉箭头
                    anchors.fill: parent
                    // 填满按钮
                    anchors.leftMargin: 8; anchors.rightMargin: 6; spacing: 4
                    // 左右边距与间距
                    Text {
                        // 当前上下文名
                        Layout.fillWidth: true
                        // 占满剩余宽度
                        text: main.circuitRef.contextName
                        // 显示当前上下文名
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                        // 文字颜色与字号
                        elide: Text.ElideRight
                        // 过长时右侧省略
                    }
                    Text { text: "▼"; color: Theme.textWeak; font.pixelSize: 9 }
                    // 下拉箭头提示可展开
                }
                MouseArea {
                    // 点击区
                    id: ctxBtnMouse; anchors.fill: parent
                    // 填满按钮
                    onClicked: main.contextPopupRef.open()
                    // 点击打开上下文选择弹窗
                }
            }

            Text {
                // 未保存脏标记
                visible: main.dirty
                // 仅在脏状态显示
                text: "●"
                // 圆点标记
                color: Theme.accentDirty
                // 脏标记强调色
                font.pixelSize: 14
                // 字号
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
            }

            Rectangle {
                // 收起顶栏按钮
                width: 28; height: 28; radius: Theme.buttonRadius
                // 尺寸与圆角
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                color: collapseMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text { anchors.centerIn: parent; text: "▲"; color: Theme.text; font.pixelSize: 13 }
                // 居中上箭头图标
                MouseArea {
                    // 点击区
                    id: collapseMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: main.topCollapsed = true
                    // 点击收起顶栏
                }
            }

            Item { width: 8; height: 1 }
            // 末尾留白
        }
    }
}