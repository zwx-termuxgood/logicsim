import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Dialog 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Dialog {
    // 退出确认对话框：无未保存修改时，退出前再确认一次
    id: exitConfirm
    // 元素 id，供外部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：用于调用 quitApp()

    anchors.centerIn: parent
    // 居中显示在父元素内
    modal: true
    // 模态：阻止用户操作背后的界面
    title: "退出"
    // 对话框标题
    standardButtons: Dialog.NoButton
    // 不显示标准按钮，改用自定义按钮
    padding: 20
    // 内容内边距
    width: 320
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
            text: "确定要退出 LogicSim 吗？"
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
                color: ec1.pressed ? Theme.bgButtonHover : Theme.bgButton
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
                MouseArea { id: ec1; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: exitConfirm.close() }
                    // 点击关闭对话框，不退出
            }
            Rectangle {
                // 退出按钮
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: ec2.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                // 颜色：按下高亮 > 成功色
                border.color: Theme.borderSuccess; border.width: 1
                // 成功色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "退出"
                    // 居中文字"退出"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    // 白字与字号
                }
                MouseArea { id: ec2; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: main.quitApp() }
                    // 点击直接退出应用
            }
        }
    }
}