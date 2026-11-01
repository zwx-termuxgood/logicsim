import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Dialog 弹窗基类、TextField 输入框在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Dialog {
    // 新建子电路对话框：输入名称后创建子电路
    id: subNameDialog
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 circuitRef.addSubcircuit() 等

    anchors.centerIn: parent
    // 居中显示在父元素内
    modal: true
    // 模态：阻止用户操作背后的界面
    title: "新建子电路"
    // 对话框标题
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
            // 提示文字
            text: "输入子电路名称"
            // 提示内容
            color: Theme.textStrong
            // 强调文字色
            font.pixelSize: Theme.fsMedium
            // 中号字号
        }

        Rectangle {
            // 输入框外框
            Layout.fillWidth: true
            // 占满宽度
            Layout.preferredHeight: 34
            // 高度
            color: Theme.bgInput
            // 输入框背景色
            border.color: Theme.borderDialog
            // 边框颜色
            border.width: 1
            // 边框宽度
            radius: Theme.buttonRadius
            // 圆角

            TextField {
                // 名称输入框
                id: subNameInput
                // 控件 id，供外部与函数引用
                anchors.fill: parent
                // 填满外框
                anchors.leftMargin: 8; anchors.rightMargin: 8
                // 左右内边距
                color: Theme.text
                // 文字色
                font.pixelSize: Theme.fsNormal
                // 常规字号
                background: null
                // 不画自带背景，使用外框
                selectByMouse: true
                // 支持鼠标选择文本
                onAccepted: subNameDialog.doCreate()
                // 回车确认时创建
            }
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
                color: sn1.pressed ? Theme.bgButtonHover : Theme.bgButton
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
                MouseArea {
                    // 点击区
                    id: sn1; anchors.fill: parent
                    // 填满按钮
                    onClicked: subNameDialog.close()
                    // 点击关闭对话框，不创建
                }
            }
            Rectangle {
                // 创建按钮
                Layout.preferredWidth: 80; Layout.preferredHeight: 30
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: sn2.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                // 颜色：按下高亮 > 成功色
                border.color: Theme.borderSuccess; border.width: 1
                // 成功色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "创建"
                    // 居中文字"创建"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    // 白字与字号
                }
                MouseArea {
                    // 点击区
                    id: sn2; anchors.fill: parent
                    // 填满按钮
                    onClicked: subNameDialog.doCreate()
                    // 点击执行创建
                }
            }
        }
    }

    // 定义"执行创建子电路"函数
    function doCreate() {
        // 名称为空则忽略
        if (subNameInput.text.trim().length === 0) return
        // 用去空白后的名称新建子电路
        main.circuitRef.addSubcircuit(subNameInput.text.trim())
        // 关闭对话框
        close()
    }

    onOpened: {
        // 打开时的初始化
        subNameInput.text = "子电路"
        // 预填默认名称
        subNameInput.forceActiveFocus()
        // 强制获取焦点，方便直接输入
        subNameInput.selectAll()
        // 全选文本，输入即替换
    }
}