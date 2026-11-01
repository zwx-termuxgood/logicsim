import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Dialog 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Dialog {
    // 通用确认对话框：显示标题与消息，确定后执行外部传入的动作
    id: confirmDialog
    // 元素 id，供外部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用（本文件按需使用）

    property string message: ""
    // 对话框正文消息，由调用方设置
    property var action: null
    // 点击"确定"后要执行的回调，由调用方设置

    anchors.centerIn: parent
    // 居中显示在父元素内
    modal: true
    // 模态：阻止用户操作背后的界面
    standardButtons: Dialog.NoButton
    // 不显示标准按钮，改用自定义按钮
    width: 320
    // 对话框宽度
    padding: 20
    // 内容内边距

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
            // 标题文本
            text: confirmDialog.title
            // 显示对话框标题
            color: Theme.textStrong
            // 强调文字色
            font.pixelSize: Theme.fsLarge
            // 较大字号
            font.bold: true
            // 加粗
            Layout.fillWidth: true
            // 占满宽度
            wrapMode: Text.Wrap
            // 超宽自动换行
        }
        Text {
            // 消息文本
            text: confirmDialog.message
            // 显示外部设置的消息
            color: Theme.textGray
            // 次要文字色
            font.pixelSize: Theme.fsNormal
            // 常规字号
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
                Layout.preferredWidth: 70; Layout.preferredHeight: 30
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: cd1.pressed ? Theme.bgButtonHover : Theme.bgButton
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
                MouseArea { id: cd1; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: confirmDialog.close() }
                    // 点击直接关闭，不执行动作
            }
            Rectangle {
                // 确定按钮
                Layout.preferredWidth: 80; Layout.preferredHeight: 30
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: cd2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                // 颜色：按下高亮 > 危险色（确认类操作多为危险操作）
                border.color: Theme.borderDangerHi; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "确定"
                    // 居中文字"确定"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    // 白字与字号
                }
                MouseArea { id: cd2; anchors.fill: parent
                    // 点击区填满按钮
                    onClicked: {
                        // 点击处理
                        if (confirmDialog.action) confirmDialog.action()
                        // 有回调则先执行回调
                        confirmDialog.close()
                        // 再关闭对话框
                    } }
            }
        }
    }
}