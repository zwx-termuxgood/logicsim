import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Dialog 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Dialog {
    // 未保存确认对话框：执行打开/新建等操作前，询问是否保存当前修改
    id: unsavedDialog
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 dirty、currentFilePath、circuitRef 等

    // 用户点"继续"后要执行的动作（打开新文件 / 清空 / …）
    property var pendingAction: null
    // 待执行的回调：用户在"不保存"或"保存成功"后执行

    anchors.centerIn: parent
    // 居中显示在父元素内
    modal: true
    // 模态：阻止用户操作背后的界面
    title: "未保存"
    // 对话框标题
    standardButtons: Dialog.NoButton
    // 不显示标准按钮，改用自定义按钮
    padding: 20
    // 内容内边距
    width: 360
    // 对话框宽度

    background: Rectangle {
        // 自定义对话框背景
        color: Theme.bgDialog
        // 背景色取主题对话框背景色
        border.color: Theme.borderDialog
        // 边框颜色取主题对话框边框色
        border.width: 1
        // 边框宽度
        radius: Theme.dialogRadius
        // 圆角半径取主题对话框圆角
    }

    contentItem: ColumnLayout {
        // 内容区：纵向布局
        spacing: 12
        // 子项间距

        Text {
            // 提示文字
            text: "当前电路已修改，是否保存？"
            // 询问文案
            color: Theme.textStrong
            // 强调文字色
            font.pixelSize: Theme.fsLarge
            // 较大字号
            Layout.fillWidth: true
            // 占满宽度
            wrapMode: Text.Wrap
            // 超宽自动换行
        }

        RowLayout {
            // 按钮行：水平布局
            Layout.fillWidth: true
            // 占满宽度
            Item { Layout.fillWidth: true }
            // 左侧弹性占位，把按钮推到右侧

            Rectangle {
                // 取消按钮
                Layout.preferredWidth: 70; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: b1.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderDialog; border.width: 1
                // 边框
                Text { anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal }
                // 居中文字"取消"
                MouseArea { id: b1; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: unsavedDialog.close() }
                    // 点击关闭对话框，放弃本次操作
            }
            Rectangle {
                // 不保存按钮
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: b2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                // 颜色：按下高亮 > 危险色
                border.color: Theme.borderDangerHi; border.width: 1
                // 危险色边框
                Text { anchors.centerIn: parent; text: "不保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                // 居中文字"不保存"
                MouseArea { id: b2; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: {
                        // 点击处理
                        unsavedDialog.close()
                        // 先关闭对话框
                        unsavedDialog.main.dirty = false
                        // 清除脏标记，放弃修改
                        var act = unsavedDialog.pendingAction
                        // 取出待执行回调
                        unsavedDialog.pendingAction = null
                        // 清空回调，避免重复执行
                        if (act) act()
                        // 有回调则执行
                    } }
            }
            Rectangle {
                // 保存按钮
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: b3.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                // 颜色：按下高亮 > 成功色
                border.color: Theme.borderSuccess; border.width: 1
                // 成功色边框
                Text { anchors.centerIn: parent; text: "保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                // 居中文字"保存"
                MouseArea { id: b3; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: {
                        // 点击处理
                        unsavedDialog.close()
                        // 先关闭对话框
                        // 当前路径已知：直接保存，成功后执行 pendingAction
                        // 未知路径：弹另存为对话框，把 pendingAction 交给 saveDialog 处理
                        var act = unsavedDialog.pendingAction
                        // 取出待执行回调
                        unsavedDialog.pendingAction = null
                        // 清空回调，避免重复执行

                        if (unsavedDialog.main.currentFilePath !== "") {
                            // 已有文件路径：直接保存
                            if (unsavedDialog.main.circuitRef.saveToFile(
                                        unsavedDialog.main.currentFilePath)) {
                                // 保存成功
                                unsavedDialog.main.dirty = false
                                // 清除脏标记
                                if (act) act()
                                // 执行待执行回调
                            } else {
                                // 保存失败
                                unsavedDialog.main.statusText = "保存失败"
                                // 状态栏提示失败
                            }
                        } else {
                            // 让 saveDialog 保存成功后执行 act
                            unsavedDialog.main.pendingAfterSave = act
                            // 把回调交给另存为对话框
                            unsavedDialog.main.saveDialogRef.open()
                            // 打开另存为对话框
                        }
                    } }
            }
        }
    }
}