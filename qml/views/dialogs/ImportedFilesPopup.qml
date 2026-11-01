import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column/Repeater/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Popup 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它弹窗一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Popup {
    // 已导入文件弹窗：列出通过"导入…"加入的文件，可逐个取消导入
    id: importedPopup
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 circuitRef、statusText、dirty 等

    property var items: []
    // 已导入文件列表：每项含 baseName/subCount/path 等字段

    anchors.centerIn: parent
    // 居中显示在父元素内
    width: 440
    // 弹窗宽度
    height: Math.min(420, 60 * Math.max(1, items.length) + 90)
    // 弹窗高度：按条目数估算（每条 60px），上限 420，至少按 1 条算再留 90px
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
        // 内容区：纵向布局
        spacing: 0
        // 子项之间无间距

        Rectangle {
            // 弹窗标题栏
            width: importedPopup.width
            // 宽等于弹窗宽
            height: 34
            // 标题栏高度
            color: Theme.bgDialogHeader
            // 标题栏背景色
            Text {
                // 标题文字
                anchors.left: parent.left; anchors.leftMargin: 12
                // 左侧对齐并留 12px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                text: "已导入的文件"
                // 标题内容
                color: Theme.textGray
                // 次要文字色
                font.pixelSize: Theme.fsNormal
                // 常规字号
                font.bold: true
                // 加粗
            }
        }

        Text {
            // 空列表占位提示
            visible: importedPopup.items.length === 0
            // 仅在列表为空时显示
            width: importedPopup.width; height: 80
            // 宽度等于弹窗宽，高度 80
            horizontalAlignment: Text.AlignHCenter
            // 水平居中
            verticalAlignment: Text.AlignVCenter
            // 垂直居中
            text: "暂无导入的文件"
            // 占位文案
            color: Theme.textMuted
            // 弱化文字色
            font.pixelSize: Theme.fsNormal
            // 常规字号
        }

        Repeater {
            // 按已导入文件列表生成条目
            model: importedPopup.items
            // 数据模型：已导入文件列表
            delegate: Rectangle {
                // 单个已导入文件条目
                width: importedPopup.width
                // 宽等于弹窗宽
                height: 56
                // 条目高度
                color: "transparent"
                // 背景透明

                Column {
                    // 文件信息列：名称 + 路径
                    anchors.left: parent.left; anchors.leftMargin: 12
                    // 左侧对齐并留 12px 边距
                    anchors.right: delBtn.left; anchors.rightMargin: 8
                    // 右侧止于取消按钮左侧并留 8px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    spacing: 2
                    // 两行之间间距

                    Text {
                        // 文件基名与电路数量
                        text: modelData.baseName + "  (" + modelData.subCount + " 个电路)"
                        // 拼接基名与子电路数量
                        color: Theme.text
                        // 常规文字色
                        font.pixelSize: Theme.fsNormal
                        // 常规字号
                        elide: Text.ElideRight
                        // 过长时右侧省略
                        width: parent.width
                        // 宽度占满信息列
                    }
                    Text {
                        // 完整文件路径
                        text: modelData.path
                        // 显示完整路径
                        color: Theme.textFaint
                        // 极弱文字色
                        font.pixelSize: Theme.fsTiny
                        // 小字号
                        elide: Text.ElideMiddle
                        // 过长时中间省略，保留首尾
                        width: parent.width
                        // 宽度占满信息列
                    }
                }

                Rectangle {
                    // 取消导入按钮
                    id: delBtn
                    // 按钮 id，供信息列右侧对齐引用
                    anchors.right: parent.right; anchors.rightMargin: 8
                    // 右侧对齐并留 8px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    width: 68; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: delMa.pressed ? "#7a2020" : Theme.bgButtonDanger
                    // 颜色：按下深红 > 危险色
                    border.color: Theme.borderDanger; border.width: 1
                    // 危险色边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent; text: "取消导入"
                        // 居中文字"取消导入"
                        color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                        // 白字与小字号
                    }
                    MouseArea {
                        // 点击区
                        id: delMa; anchors.fill: parent
                        // 填满按钮
                        onClicked: {
                            // 点击处理
                            if (importedPopup.main.circuitRef.removeImportedFile(modelData.path)) {
                                // 取消导入成功
                                importedPopup.main.statusText = "已取消导入：" + modelData.baseName
                                // 状态栏提示
                                importedPopup.main.dirty = true
                                // 标记为已修改
                                importedPopup.refresh()
                                // 刷新列表
                            }
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
        // 底部留白
    }

    onOpened: refresh()
    // 打开时刷新列表

    // 定义"刷新已导入文件列表"函数
    function refresh() {
        // 从电路接口取已导入文件列表；main 为空时给空数组
        items = main ? main.circuitRef.importedFiles() : []
    }
}