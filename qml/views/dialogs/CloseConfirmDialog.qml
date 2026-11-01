import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Dialog 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Dialog {
    // 关闭确认对话框：主窗口有未保存修改时询问是否保存
    id: closeConfirm
    // 元素 id，供外部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 dirty、currentFilePath、circuitRef 等

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
    width: 340
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
            text: "电路已修改，是否保存？"
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
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: cc1.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderDialog; border.width: 1
                // 边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "取消"
                    // 居中文字"取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                    // 文字颜色与字号
                }
                MouseArea { id: cc1; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: closeConfirm.close() }
                    // 点击关闭对话框（不退出）
            }
            Rectangle {
                // 不保存按钮
                Layout.preferredWidth: 100; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: cc2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                // 颜色：按下高亮 > 危险色
                border.color: Theme.borderDangerHi; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "不保存"
                    // 居中文字"不保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    // 白字与字号
                }
                MouseArea { id: cc2; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: {
                        // 点击处理
                        main.dirty = false
                        // 清除脏标记
                        main.quitApp()
                        // 直接退出应用
                    } }
            }
            Rectangle {
                // 保存按钮
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: cc3.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                // 颜色：按下高亮 > 成功色
                border.color: Theme.borderSuccess; border.width: 1
                // 成功色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "保存"
                    // 居中文字"保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    // 白字与字号
                }
                MouseArea { id: cc3; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: {
                        // 点击处理
                        closeConfirm.close()
                        // 先关闭对话框
                        main.saveBeforeExit = true
                        // 标记退出前保存流程进行中
                        if (main.currentFilePath !== "") {
                            // 已有文件路径：直接保存
                            if (main.circuitRef.saveToFile(main.currentFilePath)) {
                                // 保存成功
                                main.dirty = false
                                // 清除脏标记
                                main.saveBeforeExit = false
                                // 清除退出前保存标记
                                main.quitApp()
                                // 退出应用
                            } else {
                                // 保存失败
                                main.saveBeforeExit = false
                                // 清除退出前保存标记
                                main.saveDialogRef.open()
                                // 打开另存为对话框
                            }
                        } else {
                            // 无文件路径：打开另存为对话框
                            main.saveDialogRef.open()
                        }
                    } }
            }
        }
    }
}