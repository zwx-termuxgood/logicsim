import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Row/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：SpinBox 等控件在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它栏组件一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：Circuit 类型及其属性/方法

Rectangle {
    // 时钟工具条根元素：一个矩形条
    id: clockBar
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：用于访问 circuitRef、状态属性等

    anchors.top: parent.top
    // 顶部对齐父元素
    anchors.left: parent.left
    // 左侧对齐父元素
    anchors.right: parent.right
    // 右侧对齐父元素
    height: Theme.clockBarH
    // 高度取主题定义的时钟栏高度
    color: Theme.bgBar
    // 背景色取主题栏背景色
    border.color: Theme.border
    // 边框颜色取主题边框色
    border.width: 1
    // 边框宽度 1px

    Flickable {
        // 可横向滑动的容器：窄屏时工具条可左右滚动
        id: clockFlick
        // 容器 id
        anchors.fill: parent
        // 填满根矩形
        contentWidth: clockRow.implicitWidth + 16
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
            // 工具条内容行
            id: clockRow
            // 行 id
            x: 8
            // 左侧留白 8px
            y: 0
            // 顶部对齐
            height: parent.height
            // 行高等于容器高度
            spacing: 6
            // 子项间距 6px

            Row {
                // 第一组：仿真控制（仅事件引擎显示）
                visible: main.engineType === "event"
                // 仅事件驱动引擎时显示该组
                spacing: 6
                // 组内间距
                anchors.verticalCenter: parent.verticalCenter
                // 组相对行垂直居中

                Rectangle {
                    // 开始/暂停仿真按钮
                    width: 96; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: simStartMa.pressed ? Theme.bgButtonRunHi
                           : (main.simulationRunning ? Theme.bgButtonRun : Theme.bgButton)
                    // 颜色：按下高亮 > 运行中运行色 > 普通按钮色
                    border.color: main.simulationRunning ? Theme.accentGreen : Theme.borderStrong
                    // 边框：运行中强调绿，否则普通强边框
                    border.width: 1
                    // 边框宽度
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent
                        // 居中
                        text: main.simulationRunning ? "⏸ 暂停仿真" : "▶ 开始仿真"
                        // 文案随运行状态切换
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                        // 文字颜色与字号
                    }
                    MouseArea {
                        // 按钮点击区
                        id: simStartMa
                        // 点击区 id
                        anchors.fill: parent
                        // 填满按钮
                        onClicked: {
                            // 点击处理
                            if (main.simulationRunning) main.circuitRef.pauseSimulation()
                            // 运行中则暂停仿真
                            else                        main.circuitRef.startSimulation()
                            // 否则启动仿真
                        }
                    }
                }

                Rectangle {
                    // 单步仿真按钮
                    width: 60; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: simStepMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    Text { anchors.centerIn: parent; text: "单步"
                        color: Theme.text; font.pixelSize: Theme.fsSmall }
                    // 居中文字"单步"
                    MouseArea {
                        // 点击区
                        id: simStepMa; anchors.fill: parent
                        // 填满按钮
                        onClicked: main.circuitRef.simulateStep()
                        // 点击触发单步仿真
                    }
                }

                Rectangle {
                    // 复位仿真按钮
                    width: 60; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: simResetMa.pressed ? Theme.bgButtonResetHi : Theme.bgButtonReset
                    // 颜色：按下高亮 > 复位色
                    border.color: Theme.borderDanger; border.width: 1
                    // 危险色边框
                    Text { anchors.centerIn: parent; text: "复位"
                        color: Theme.textWhite; font.pixelSize: Theme.fsSmall }
                    // 居中白字"复位"
                    MouseArea {
                        // 点击区
                        id: simResetMa; anchors.fill: parent
                        // 填满按钮
                        onClicked: main.circuitRef.resetSimulation()
                        // 点击复位仿真
                    }
                }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }
                // 竖直分隔线

                // ★ 仿真时间：由 showSimTime 开关控制显隐
                Text {
                    // 仿真时间显示
                    visible: main.circuitRef.showSimTime
                    // 仅在开关打开时显示
                    text: "仿真时间：" + main.circuitRef.simTime
                    // 文案拼接当前仿真时间
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与字号
                    anchors.verticalCenter: parent.verticalCenter
                    // 相对行垂直居中
                }

                Rectangle {
                    // 仿真时间后的分隔线
                    visible: main.circuitRef.showSimTime
                    // 与仿真时间同显隐
                    width: 1; height: 22; color: Theme.borderStrong
                    // 竖直分隔线
                }
            }

            Row {
                // 第二组：时钟控制与频率
                spacing: 6
                // 组内间距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中

                Rectangle {
                    // 时钟启动/停止按钮
                    width: 80; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: playBtn.pressed ? Theme.bgButtonRunHi
                           : (main.clockRunning ? Theme.bgButtonRun : Theme.bgButton)
                    // 颜色：按下高亮 > 运行中运行色 > 普通
                    border.color: main.clockRunning ? Theme.accentGreen : Theme.borderStrong
                    // 边框：运行中强调绿
                    border.width: 1
                    // 边框宽度
                    opacity: (main.engineType === "event" && !main.simulationRunning && !main.clockRunning) ? 0.5 : 1
                    // 事件引擎未运行仿真且时钟未跑时置半透明表示不可用
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent
                        // 居中
                        text: main.clockRunning ? "⏸ 停止" : "▶ 时钟"
                        // 文案随时钟状态切换
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                        // 文字颜色与字号
                    }
                    MouseArea {
                        // 点击区
                        id: playBtn; anchors.fill: parent
                        // 填满按钮
                        // ★ 时钟在跑时总能点（为了停）；否则要求「事件引擎必须仿真中」才能启动
                        enabled: main.clockRunning
                                 || !(main.engineType === "event" && !main.simulationRunning)
                        // 启用条件：时钟在跑（可停），或非「事件引擎且仿真未运行」的情况
                        onClicked: {
                            // 点击处理
                            if (main.clockRunning) main.circuitRef.stopClock()
                            // 运行中则停止时钟
                            else                    main.circuitRef.startClock()
                            // 否则启动时钟
                        }
                    }
                }

                Rectangle {
                    // 时钟拍按钮：手动推进一拍
                    width: 64; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: stepBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    Text { anchors.centerIn: parent; text: "时钟拍"
                        color: Theme.text; font.pixelSize: Theme.fsSmall }
                    // 居中文字"时钟拍"
                    MouseArea { id: stepBtn; anchors.fill: parent
                        // 点击区填满按钮
                        onClicked: main.circuitRef.singleStep() }
                        // 点击触发单拍
                }

                Rectangle {
                    // 复位输入按钮
                    width: 68; height: 26; radius: Theme.smallRadius
                    // 尺寸与圆角
                    color: resetBtn.pressed ? Theme.bgButtonResetHi : Theme.bgButtonReset
                    // 颜色：按下高亮 > 复位色
                    border.color: Theme.borderDanger; border.width: 1
                    // 危险色边框
                    Text { anchors.centerIn: parent; text: "复位输入"
                        color: Theme.textWhite; font.pixelSize: Theme.fsSmall }
                    // 居中白字"复位输入"
                    MouseArea { id: resetBtn; anchors.fill: parent
                        // 点击区填满按钮
                        onClicked: {
                            // 点击处理
                            main.circuitRef.stopClock()
                            // 先停时钟
                            main.circuitRef.resetAllInputs()
                            // 再复位所有输入
                        }
                    }
                }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }
                // 竖直分隔线

                Text { text: "频率"; color: Theme.textGray; font.pixelSize: Theme.fsSmall
                    // "频率"标签
                    anchors.verticalCenter: parent.verticalCenter }
                    // 垂直居中
                SpinBox {
                    // 频率数值输入框
                    id: freqSpin
                    // 控件 id
                    from: 1; to: 1000; editable: true
                    // 取值 1..1000，可手动输入
                    // ★ 宽度 100 → 150：原来 Basic 样式的 contentItem 内边距 + 上下
                    //   指示器会挤占文本区，显示 "1000" 这类 4 位数字时被裁掉。
                    //   加宽到 150 后文本区 ≥ 90px，可以完整显示 1~1000。
                    width: 150
                    // 宽度
                    font.pixelSize: Theme.fsSmall
                    // 统一字号，与工具条其它文字一致
                    value: main.clockFrequency
                    // 初始绑定当前时钟频率
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                    // ★ 自定义 contentItem：显式设置字体、颜色、左右内边距与居中，
                    //   摆脱 Basic 样式默认 padding 造成数字被裁剪的问题。
                    contentItem: TextInput {
                        // 显示 SpinBox 当前值的编辑框
                        text: parent.textFromValue(parent.value, parent.locale)
                        // 由 SpinBox 把 value 转成显示文本
                        font: parent.font
                        // 使用 SpinBox 自己的字体
                        color: Theme.text
                        // 使用主题常规文字色，避免深色背景上看不清
                        horizontalAlignment: Qt.AlignHCenter
                        // 水平居中
                        verticalAlignment: Qt.AlignVCenter
                        // 垂直居中
                        leftPadding: 6; rightPadding: 6
                        // 左右内边距
                        selectByMouse: true
                        // 支持鼠标选择
                        readOnly: !parent.editable
                        // 不可编辑时只读
                        validator: parent.validator
                        // 继承 SpinBox 的校验器
                    }
                    onValueChanged: main.circuitRef.setClockFrequency(value)
                    // 值变化时同步到电路
                }
                Text { text: "Hz"; color: Theme.textGray; font.pixelSize: Theme.fsSmall
                    // "Hz"单位标签
                    anchors.verticalCenter: parent.verticalCenter }
                    // 垂直居中

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }
                // 竖直分隔线

                Text {
                    // 时钟拍数显示
                    text: "拍数：" + main.clockTickCount
                    // 文案拼接当前拍数
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与字号
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                }

                Item { width: 8; height: 1 }
                // 末尾留白
            }
        }
    }
}