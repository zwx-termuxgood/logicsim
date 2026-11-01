import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column/Repeater/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Popup 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它弹窗一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Popup {
    // 最近打开文件弹窗：列出最近文件，可点击打开或逐条删除
    id: recentPopup
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 recentList、circuitRef、doOpenFile 等

    anchors.centerIn: parent
    // 居中显示在父元素内
    width: 380
    // 弹窗宽度
    height: Math.min(400, 44 * Math.max(1, main.recentList.length) + 70)
    // 弹窗高度：按条目数估算（每条 44px），上限 400，至少按 1 条算再留 70px
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
            // 标题栏
            width: recentPopup.width
            // 宽等于弹窗宽
            height: 32
            // 标题栏高度
            color: Theme.bgDialogHeader
            // 标题栏背景色

            Text {
                // 标题文字
                anchors.left: parent.left; anchors.leftMargin: 12
                // 左侧对齐并留 12px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                text: "最近打开"
                // 标题内容
                color: Theme.textGray
                // 次要文字色
                font.pixelSize: Theme.fsNormal
                // 常规字号
                font.bold: true
                // 加粗
            }
            Rectangle {
                // 清空按钮
                anchors.right: parent.right; anchors.rightMargin: 8
                // 右侧对齐并留 8px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                width: 60; height: 22; radius: Theme.smallRadius
                // 尺寸与圆角
                color: clearRecentMa.pressed ? "#7a2020" : Theme.bgButtonDanger
                // 颜色：按下深红 > 危险色
                border.color: Theme.borderDanger; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "清空"
                    // 居中文字"清空"
                    color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                    // 白字与小字号
                }
                MouseArea {
                    // 点击区
                    id: clearRecentMa; anchors.fill: parent
                    // 填满按钮
                    enabled: main.recentList.length > 0
                    // 列表非空时才可点
                    onClicked: main.circuitRef.clearRecentFiles()
                    // 点击清空最近文件列表
                }
            }
        }

        Text {
            // 空列表占位提示
            visible: main.recentList.length === 0
            // 仅在列表为空时显示
            width: recentPopup.width; height: 60
            // 宽度等于弹窗宽，高度 60
            horizontalAlignment: Text.AlignHCenter
            // 水平居中
            verticalAlignment: Text.AlignVCenter
            // 垂直居中
            text: "暂无最近打开的文件"
            // 占位文案
            color: Theme.textMuted
            // 弱化文字色
            font.pixelSize: Theme.fsNormal
            // 常规字号
        }

        Repeater {
            // 按最近文件列表生成条目
            model: main.recentList
            // 数据模型：最近文件列表
            delegate: Rectangle {
                // 单条最近文件
                width: recentPopup.width; height: 44
                // 条目宽等于弹窗宽，高 44
                color: itemMa.pressed ? Theme.bgButtonHover : "transparent"
                // 颜色：按下高亮 > 透明

                Column {
                    // 文件信息列：名称 + 时间/路径
                    anchors.left: parent.left; anchors.leftMargin: 12
                    // 左侧对齐并留 12px 边距
                    anchors.right: delMaBox.left; anchors.rightMargin: 8
                    // 右侧止于删除按钮左侧并留 8px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    spacing: 2
                    // 两行之间间距
                    Text {
                        // 文件显示名
                        text: modelData.name
                        // 显示文件名称
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
                        // 时间与完整路径
                        text: modelData.time + "  " + modelData.path
                        // 拼接时间与路径
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
                    // 单条删除按钮
                    id: delMaBox
                    // 按钮 id，供信息列右侧对齐引用
                    anchors.right: parent.right; anchors.rightMargin: 8
                    // 右侧对齐并留 8px 边距
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    width: 24; height: 24; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: delMaMouse.pressed ? "#7a2020" : Theme.bgButtonDanger
                    // 颜色：按下深红 > 危险色
                    Text {
                        // 删除图标
                        anchors.centerIn: parent; text: "×"
                        // 居中显示 ×
                        color: Theme.textWhite; font.pixelSize: 14
                        // 白字与字号
                    }
                    MouseArea {
                        // 点击区
                        id: delMaMouse; anchors.fill: parent
                        // 填满按钮
                        onClicked: main.circuitRef.removeRecentFile(modelData.path)
                        // 点击移除该最近文件记录
                    }
                }

                MouseArea {
                    // 条目点击区：点击打开该文件
                    id: itemMa
                    // 点击区 id
                    anchors.fill: parent
                    // 填满条目
                    anchors.rightMargin: 40
                    // 右侧留出删除按钮区域，避免误触
                    onClicked: {
                        // 点击处理
                        // ★ 所有外部引用在闭包创建前捕获到局部变量
                        // （loadFromFile 触发 recentFilesChanged 时，Repeater 会重建本委托，
                        //  之后闭包里的外部标识符（main）会失效 → ReferenceError）
                        var savedPath = modelData.path
                        // 提前捕获路径，避免委托重建后失效
                        var savedName = modelData.name
                        // 提前捕获名称
                        var mainRef = main
                        // 提前捕获主窗口引用
                        var circRef = main.circuitRef
                        // 提前捕获电路接口引用
                        console.log("[QMLTRACE] RecentPopup.onClicked savedPath=", savedPath,
                                    "currentFilePath(before)=", mainRef.currentFilePath)
                        // 调试日志：打印目标路径与当前路径
                        recentPopup.close()
                        // 关闭弹窗

                        mainRef.doOpenFile(function() {
                            // 走未保存确认流程后执行打开
                            console.log("[QMLTRACE] Recent action: ENTER, currentFilePath(before)=",
                                        mainRef.currentFilePath, "target=", savedPath)
                            // 调试日志：进入打开动作
                            if (circRef.loadFromFile(savedPath)) {
                                // 加载成功
                                mainRef.currentFilePath = savedPath
                                // 更新当前文件路径
                                console.log("[QMLTRACE] Recent action: currentFilePath set to",
                                            mainRef.currentFilePath)
                                // 调试日志：路径已更新
                                mainRef.dirty = false
                                // 清除脏标记
                                mainRef.selectedCompId = ""
                                // 清空单选元件
                                mainRef.selectedComp = null
                                // 清空选中元件引用
                                mainRef.selectedIds = []
                                // 清空多选
                                mainRef.selectedSubId = ""
                                // 清空选中子电路
                                mainRef.resetView()
                                // 重置视图缩放与偏移
                                mainRef.statusText = "已打开：" + savedName
                                // 状态栏提示已打开
                            } else {
                                // 加载失败
                                mainRef.statusText = "打开失败：" + savedPath
                                // 状态栏提示失败
                            }
                        })
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
        // 底部留白
    }
}