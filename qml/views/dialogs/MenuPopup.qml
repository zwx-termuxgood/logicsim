import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Popup 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它弹窗一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Popup {
    // 主菜单弹窗：保存/打开/导入/最近/统计/新建/设置等入口
    id: menuPopup
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问各种动作与弹窗引用
    property Item anchorItem: null
    // 锚点元素：弹窗位置据此计算（通常为顶栏菜单按钮）

    readonly property real screenW: main ? main.width : 360
    // 屏幕/窗口宽度：优先取主窗口宽，缺省 360
    readonly property real screenH: main ? main.height : 640
    // 屏幕/窗口高度：优先取主窗口高，缺省 640

    width: Math.min(260, screenW - 16)
    // 弹窗宽度：上限 260，且不超过窗口宽减去左右留白
    height: Math.min(430, screenH - 40)
    // 弹窗高度：上限 430，且不超过窗口高减去上下留白

    x: {
        // 弹窗 x 计算
        if (!anchorItem) return 8
        // 无锚点时固定 8
        var ax = anchorItem.mapToItem(null, 0, 0).x
        // 锚点左上角在根坐标系中的 x
        var maxX = screenW - width - 8
        // 最大 x：保证弹窗右侧不出界
        return Math.max(8, Math.min(ax, maxX))
        // 左对齐锚点，但钳位在 [8, maxX] 内
    }

    y: {
        // 弹窗 y 计算
        if (!anchorItem) return 50
        // 无锚点时固定 50
        var ay = anchorItem.mapToItem(null, 0, anchorItem.height).y + 4
        // 锚点底边下方 4px 处的 y
        var maxY = screenH - height - 8
        // 最大 y：保证弹窗底部不出界
        return Math.max(8, Math.min(ay, maxY))
        // 贴在锚点下方，但钳位在 [8, maxY] 内
    }

    modal: true
    // 模态：阻止用户操作背后的界面
    focus: true
    // 打开时获取焦点，支持键盘操作
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // 按 Esc 或点击外部关闭
    padding: 0
    // 内容无内边距，由各条目自行控制

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

    contentItem: Flickable {
        // 内容区：可纵向滚动的容器
        id: menuFlick
        // 容器 id
        implicitWidth: menuPopup.width
        // 隐式宽度等于弹窗宽
        implicitHeight: contentColumn.implicitHeight
        // 隐式高度等于内容列隐式高度
        contentWidth: width
        // 内容宽度等于自身宽（无横向滚动）
        contentHeight: contentColumn.height
        // 内容高度等于内容列高度
        clip: true
        // 裁剪超出部分
        flickableDirection: Flickable.VerticalFlick
        // 只允许纵向拖动
        boundsBehavior: Flickable.StopAtBounds
        // 到达边界即停止，不做回弹

        Column {
            // 菜单内容列
            id: contentColumn
            // 列 id
            width: menuFlick.width
            // 宽等于滚动容器宽
            spacing: 0
            // 条目之间无间距

            Rectangle {
                // 菜单标题栏
                width: contentColumn.width
                // 宽等于列宽
                height: 40
                // 标题栏高度
                color: Theme.bgDialogHeader
                // 标题栏背景色
                radius: 8
                // 圆角，贴合弹窗顶部
                Text {
                    // 标题文字
                    anchors.left: parent.left; anchors.leftMargin: 16
                    // 左侧对齐并留 16px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    text: "菜单"
                    // 标题内容
                    color: Theme.textGray
                    // 次要文字色
                    font.pixelSize: Theme.fsMedium
                    // 中号字号
                    font.bold: true
                    // 加粗
                }
            }

            MenuRow {
                // 菜单项：保存
                label: "保存"
                // 显示文字
                onTriggered: { menuPopup.close(); main.doSave() }
                // 点击：关闭弹窗并执行保存
            }
            MenuRow {
                // 菜单项：另存为
                label: "另存为"
                // 显示文字
                onTriggered: { menuPopup.close(); main.doSaveAs() }
                // 点击：关闭弹窗并另存为
            }
            MenuRow {
                // 菜单项：打开
                label: "打开"
                // 显示文字
                onTriggered: { menuPopup.close(); main.openDialogRef.open() }
                // 点击：关闭弹窗并打开文件对话框
            }
            MenuRow {
                // 菜单项：导入
                label: "导入…"
                // 显示文字
                onTriggered: { menuPopup.close(); main.importDialogRef.open() }
                // 点击：关闭弹窗并打开导入对话框
            }
            MenuRow {
                // 菜单项：已导入
                label: "已导入…"
                // 显示文字
                onTriggered: { menuPopup.close(); main.importedFilesPopupRef.open() }
                // 点击：关闭弹窗并打开已导入文件列表
            }
            MenuRow {
                // 菜单项：最近
                label: "最近"
                // 显示文字
                onTriggered: { menuPopup.close(); main.recentPopupRef.open() }
                // 点击：关闭弹窗并打开最近文件弹窗
            }
            MenuRow {
                // 菜单项：统计
                label: "统计"
                // 显示文字
                onTriggered: { menuPopup.close(); main.statPopupRef.open() }
                // 点击：关闭弹窗并打开统计弹窗
            }
            MenuRow {
                // 菜单项：新建
                label: "新建"
                // 显示文字
                onTriggered: {
                    // 点击处理
                    menuPopup.close()
                    // 关闭弹窗
                    main.doOpenFile(function() {
                        // 走未保存确认流程后执行清空
                        main.circuitRef.clear()
                        // 清空整个电路
                        main.currentFilePath = ""
                        // 清空当前文件路径
                        main.dirty = false
                        // 清除脏标记
                    })
                }
            }

            Rectangle {
                // 分组分隔线
                width: contentColumn.width
                // 宽等于列宽
                height: 1
                // 高度 1px
                color: Theme.border
                // 边框色
            }

            MenuRow {
                // 菜单项：设置
                label: "⚙ 设置"
                // 显示文字
                onTriggered: { menuPopup.close(); main.settingsOpen = true }
                // 点击：关闭弹窗并打开设置页
            }

            Item { width: 1; height: 8 }
            // 底部留白
        }
    }

    component MenuRow: Rectangle {
        // 内部组件：单个菜单行
        property string label: ""
        // 行显示文字
        signal triggered()
        // 行被点击时发出的信号

        width: contentColumn.width
        // 宽等于内容列宽
        height: 40
        // 行高
        color: ma.pressed ? Theme.bgButtonHover : "transparent"
        // 颜色：按下高亮 > 透明
        radius: 0
        // 无圆角

        Text {
            // 行文字
            anchors.left: parent.left; anchors.leftMargin: 16
            // 左侧对齐并留 16px 边距
            anchors.verticalCenter: parent.verticalCenter
            // 垂直居中
            text: parent.label
            // 显示行文字
            color: Theme.text
            // 常规文字色
            font.pixelSize: Theme.fsNormal
            // 常规字号
        }
        MouseArea {
            // 行点击区
            id: ma
            // 点击区 id
            anchors.fill: parent
            // 填满整行
            onClicked: parent.triggered()
            // 点击发出 triggered 信号
        }
    }
}