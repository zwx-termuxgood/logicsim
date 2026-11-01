import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column/Row 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：RadioButton/CheckBox/TextField 等控件在此
import QtQuick.Layouts
// 引入 Layouts 模块：RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Rectangle {
    // 设置页：覆盖在主界面之上的全屏设置面板
    id: settingsPage
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 circuitRef、settingsOpen 等

    color: Theme.bg
    // 页面背景色取主题背景色
    z: 2000
    // 层级很高，覆盖在主界面之上

    property int engineIndex: 0
    // 引擎选择：0 事件驱动，1 迭代
    property int wireModeIndex: 0
    // 连线模式选择：0 贝塞尔，1 正交，2 可切换
    property string gateDelayText: "1"
    // 全局门延迟输入框文本
    property string wireDelayText: "1"
    // 导线延迟输入框文本
    property int speedExp: 3
    // 仿真速度指数（10 的幂）
    property bool autoSpeed: true
    // 是否自动速度
    property bool showSimTime: false
    // 是否显示仿真时间
    property string errorText: ""
    // 校验错误文本，非空时显示

    // 定义"从电路同步设置到界面"函数
    function syncFromCircuit() {
        // 引擎类型映射到索引
        engineIndex = (main.circuitRef.engineType === "event") ? 0 : 1
        // 全局门延迟转字符串
        gateDelayText = String(main.circuitRef.globalGateDelay)
        // 导线延迟转字符串
        wireDelayText = String(main.circuitRef.wireDelay)
        // 自动速度
        autoSpeed = main.circuitRef.autoSpeed
        // 速度指数
        speedExp = main.circuitRef.simulationSpeedExp
        // 显示仿真时间
        showSimTime = main.circuitRef.showSimTime

        // 取连线模式
        var wm = main.circuitRef.wireMode
        // 连线模式映射到索引
        wireModeIndex = (wm === "ortho") ? 1 : (wm === "switchable" ? 2 : 0)
        // 清空错误文本
        errorText = ""
    }

    Component.onCompleted: syncFromCircuit()
    // 组件加载完成时同步一次
    onVisibleChanged: if (visible) syncFromCircuit()
    // 每次显示时重新同步，保证反映最新设置

    Rectangle {
        // 设置页标题栏
        id: titleBar
        // 标题栏 id
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        // 顶部横向铺满
        height: 50; color: Theme.bgPanel
        // 高度与背景色
        border.color: Theme.border; border.width: 1
        // 边框

        RowLayout {
            // 标题栏内容行
            anchors.fill: parent
            // 填满标题栏
            anchors.leftMargin: 12; anchors.rightMargin: 12
            // 左右边距
            spacing: 8
            // 子项间距
            Text {
                // 页面标题
                text: "⚙ 设置"
                // 标题内容
                color: Theme.textStrong
                // 强调文字色
                font.pixelSize: Theme.fsLarge
                // 较大字号
                font.bold: true
                // 加粗
            }
            Item { Layout.fillWidth: true }
            // 弹性占位，把返回按钮推到右侧
            Rectangle {
                // 返回按钮
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                // 尺寸
                radius: Theme.buttonRadius
                // 圆角
                color: backMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "← 返回"
                    // 居中文字"← 返回"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                    // 文字颜色与字号
                }
                MouseArea {
                    // 点击区
                    id: backMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: main.settingsOpen = false
                    // 点击关闭设置页
                }
            }
        }
    }

    Flickable {
        // 设置内容可滚动容器
        anchors.top: titleBar.bottom
        // 顶部对齐标题栏底部
        anchors.left: parent.left
        // 左侧对齐
        anchors.right: parent.right
        // 右侧对齐
        anchors.bottom: parent.bottom
        // 底部对齐
        anchors.margins: 20
        // 四周留 20px 边距
        clip: true
        // 裁剪超出部分
        contentWidth: width
        // 内容宽度等于自身宽（无横向滚动）
        contentHeight: settingsColumn.height + 40
        // 内容高度等于内容列高度加底部留白

        Column {
            // 设置内容列
            id: settingsColumn
            // 列 id
            x: Math.max(0, (parent.width - width) / 2)
            // 水平居中
            width: Math.min(parent.width, 700)
            // 宽度上限 700，超出则居中
            spacing: 20
            // 各设置区块之间间距

            Rectangle {
                // 区块：仿真引擎选择
                width: parent.width; height: 130
                // 宽度铺满，固定高度
                color: Theme.bgPanel
                // 面板背景色
                border.color: Theme.border; border.width: 1; radius: 8
                // 边框与圆角
                Column {
                    // 区块内容列
                    x: 16; y: 12; spacing: 10; width: parent.width - 32
                    // 内边距、间距与宽度
                    Text {
                        // 区块标题
                        text: "仿真引擎"
                        // 标题内容
                        color: Theme.textStrong
                        // 强调文字色
                        font.pixelSize: Theme.fsLarge
                        // 较大字号
                        font.bold: true
                        // 加粗
                    }
                    Text {
                        // 区块说明
                        text: "新引擎支持门延迟、导线延迟、事件驱动、振荡器仿真"
                        // 说明文字
                        color: Theme.textGray
                        // 次要文字色
                        font.pixelSize: Theme.fsSmall
                        // 小字号
                        width: parent.width
                        // 宽度铺满
                        wrapMode: Text.Wrap
                        // 超宽自动换行
                    }
                    Row {
                        // 引擎单选行
                        spacing: 16
                        // 两项间距
                        RadioButton {
                            // 事件驱动引擎选项
                            id: rbEvent
                            // 控件 id
                            text: "事件驱动引擎（新）"
                            // 选项文字
                            checked: settingsPage.engineIndex === 0
                            // 当前选中状态绑定
                            onToggled: if (checked) settingsPage.engineIndex = 0
                            // 选中时更新索引
                            contentItem: Text {
                                // 自定义文字样式
                                text: rbEvent.text
                                // 显示选项文字
                                color: Theme.text
                                // 常规文字色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                leftPadding: rbEvent.indicator.width + 6
                                // 给指示器留出左侧空间
                                verticalAlignment: Text.AlignVCenter
                                // 垂直居中
                            }
                        }
                        RadioButton {
                            // 迭代引擎选项
                            id: rbIter
                            // 控件 id
                            text: "迭代引擎（旧）"
                            // 选项文字
                            checked: settingsPage.engineIndex === 1
                            // 当前选中状态绑定
                            onToggled: if (checked) settingsPage.engineIndex = 1
                            // 选中时更新索引
                            contentItem: Text {
                                // 自定义文字样式
                                text: rbIter.text
                                // 显示选项文字
                                color: Theme.text
                                // 常规文字色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                leftPadding: rbIter.indicator.width + 6
                                // 给指示器留出左侧空间
                                verticalAlignment: Text.AlignVCenter
                                // 垂直居中
                            }
                        }
                    }
                }
            }

            Rectangle {
                // 区块：连线模式选择
                width: parent.width; height: 170
                // 宽度铺满，固定高度
                color: Theme.bgPanel
                // 面板背景色
                border.color: Theme.border; border.width: 1; radius: 8
                // 边框与圆角
                Column {
                    // 区块内容列
                    x: 16; y: 12; spacing: 10; width: parent.width - 32
                    // 内边距、间距与宽度
                    Text {
                        // 区块标题
                        text: "连线模式"
                        // 标题内容
                        color: Theme.textStrong
                        // 强调文字色
                        font.pixelSize: Theme.fsLarge
                        // 较大字号
                        font.bold: true
                        // 加粗
                    }
                    Text {
                        // 区块说明
                        text: "贝塞尔：平滑曲线（默认）；正交：直角折线（拖拽方向决定拐角）；可切换：主界面顶部切换按钮控制。\n旧存档默认贝塞尔，正交线作为独立元件可编辑删除。"
                        // 说明文字（含换行）
                        color: Theme.textGray
                        // 次要文字色
                        font.pixelSize: Theme.fsSmall
                        // 小字号
                        width: parent.width
                        // 宽度铺满
                        wrapMode: Text.Wrap
                        // 超宽自动换行
                    }
                    Row {
                        // 连线模式单选行
                        spacing: 16
                        // 各项间距
                        RadioButton {
                            // 贝塞尔选项
                            id: rbBezier
                            // 控件 id
                            text: "贝塞尔"
                            // 选项文字
                            checked: settingsPage.wireModeIndex === 0
                            // 当前选中状态绑定
                            onToggled: if (checked) settingsPage.wireModeIndex = 0
                            // 选中时更新索引
                            contentItem: Text {
                                // 自定义文字样式
                                text: rbBezier.text; color: Theme.text
                                // 文字与颜色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                leftPadding: rbBezier.indicator.width + 6
                                // 给指示器留出左侧空间
                                verticalAlignment: Text.AlignVCenter
                                // 垂直居中
                            }
                        }
                        RadioButton {
                            // 正交选项
                            id: rbOrtho
                            // 控件 id
                            text: "正交"
                            // 选项文字
                            checked: settingsPage.wireModeIndex === 1
                            // 当前选中状态绑定
                            onToggled: if (checked) settingsPage.wireModeIndex = 1
                            // 选中时更新索引
                            contentItem: Text {
                                // 自定义文字样式
                                text: rbOrtho.text; color: Theme.text
                                // 文字与颜色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                leftPadding: rbOrtho.indicator.width + 6
                                // 给指示器留出左侧空间
                                verticalAlignment: Text.AlignVCenter
                                // 垂直居中
                            }
                        }
                        RadioButton {
                            // 可切换选项
                            id: rbSwitchable
                            // 控件 id
                            text: "可切换"
                            // 选项文字
                            checked: settingsPage.wireModeIndex === 2
                            // 当前选中状态绑定
                            onToggled: if (checked) settingsPage.wireModeIndex = 2
                            // 选中时更新索引
                            contentItem: Text {
                                // 自定义文字样式
                                text: rbSwitchable.text; color: Theme.text
                                // 文字与颜色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                leftPadding: rbSwitchable.indicator.width + 6
                                // 给指示器留出左侧空间
                                verticalAlignment: Text.AlignVCenter
                                // 垂直居中
                            }
                        }
                    }
                }
            }

            Rectangle {
                // 区块：传播延迟（仅事件引擎显示）
                width: parent.width
                // 宽度铺满
                height: settingsPage.engineIndex === 0 ? 210 : 0
                // 事件引擎时高度 210，否则 0
                visible: settingsPage.engineIndex === 0
                // 仅事件引擎时可见
                color: Theme.bgPanel
                // 面板背景色
                border.color: Theme.border; border.width: 1; radius: 8
                // 边框与圆角
                Column {
                    // 区块内容列
                    x: 16; y: 12; spacing: 10; width: parent.width - 32
                    // 内边距、间距与宽度
                    Text {
                        // 区块标题
                        text: "传播延迟"
                        // 标题内容
                        color: Theme.textStrong
                        // 强调文字色
                        font.pixelSize: Theme.fsLarge
                        // 较大字号
                        font.bold: true
                        // 加粗
                    }
                    Text {
                        // 区块说明
                        text: "延迟使用逻辑时间单位（不是真实时间）。\n门延迟作用于所有逻辑门，导线延迟作用于两个门之间的连线（与长度无关）。"
                        // 说明文字（含换行）
                        color: Theme.textGray
                        // 次要文字色
                        font.pixelSize: Theme.fsSmall
                        // 小字号
                        width: parent.width
                        // 宽度铺满
                        wrapMode: Text.Wrap
                        // 超宽自动换行
                    }
                    Row {
                        // 全局门延迟输入行
                        spacing: 8
                        // 子项间距
                        Text {
                            // 标签
                            text: "全局门延迟"
                            // 标签内容
                            color: Theme.text
                            // 文字色
                            font.pixelSize: Theme.fsNormal
                            // 常规字号
                            width: 120
                            // 固定标签宽度
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                        Rectangle {
                            // 输入框外框
                            width: 160; height: 30
                            // 尺寸
                            color: Theme.bgInput
                            // 输入框背景色
                            border.color: Theme.borderStrong
                            // 边框色
                            border.width: 1; radius: Theme.smallRadius
                            // 边框与圆角
                            TextField {
                                // 门延迟输入框
                                id: gateDelayField
                                // 控件 id
                                anchors.fill: parent
                                // 填满外框
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                // 左右内边距
                                color: Theme.text
                                // 文字色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                background: null
                                // 不画自带背景
                                selectByMouse: true
                                // 支持鼠标选择
                                text: settingsPage.gateDelayText
                                // 绑定当前文本
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                                // 只允许最多 18 位数字
                                onTextChanged: settingsPage.gateDelayText = text
                                // 输入变化同步回属性
                            }
                        }
                        Text {
                            // 默认值提示
                            text: "（默认 1）"
                            // 提示文字
                            color: Theme.textFaint
                            // 极弱文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                    }
                    Row {
                        // 导线延迟输入行
                        spacing: 8
                        // 子项间距
                        Text {
                            // 标签
                            text: "导线传播延迟"
                            // 标签内容
                            color: Theme.text
                            // 文字色
                            font.pixelSize: Theme.fsNormal
                            // 常规字号
                            width: 120
                            // 固定标签宽度
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                        Rectangle {
                            // 输入框外框
                            width: 160; height: 30
                            // 尺寸
                            color: Theme.bgInput
                            // 输入框背景色
                            border.color: Theme.borderStrong
                            // 边框色
                            border.width: 1; radius: Theme.smallRadius
                            // 边框与圆角
                            TextField {
                                // 导线延迟输入框
                                id: wireDelayField
                                // 控件 id
                                anchors.fill: parent
                                // 填满外框
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                // 左右内边距
                                color: Theme.text
                                // 文字色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                background: null
                                // 不画自带背景
                                selectByMouse: true
                                // 支持鼠标选择
                                text: settingsPage.wireDelayText
                                // 绑定当前文本
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                                // 只允许最多 18 位数字
                                onTextChanged: settingsPage.wireDelayText = text
                                // 输入变化同步回属性
                            }
                        }
                        Text {
                            // 默认值提示
                            text: "（默认 1）"
                            // 提示文字
                            color: Theme.textFaint
                            // 极弱文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                    }
                }
            }

            Rectangle {
                // 区块：仿真速度（仅事件引擎显示）
                width: parent.width
                // 宽度铺满
                height: settingsPage.engineIndex === 0 ? 250 : 0
                // 事件引擎时高度 250，否则 0
                visible: settingsPage.engineIndex === 0
                // 仅事件引擎时可见
                color: Theme.bgPanel
                // 面板背景色
                border.color: Theme.border; border.width: 1; radius: 8
                // 边框与圆角
                Column {
                    // 区块内容列
                    x: 16; y: 12; spacing: 10; width: parent.width - 32
                    // 内边距、间距与宽度
                    Text {
                        // 区块标题
                        text: "仿真速度"
                        // 标题内容
                        color: Theme.textStrong
                        // 强调文字色
                        font.pixelSize: Theme.fsLarge
                        // 较大字号
                        font.bold: true
                        // 加粗
                    }
                    Text {
                        // 区块说明
                        text: "每帧推进的逻辑时间单位数。数值可以很大。"
                        // 说明文字
                        color: Theme.textGray
                        // 次要文字色
                        font.pixelSize: Theme.fsSmall
                        // 小字号
                        width: parent.width
                        // 宽度铺满
                        wrapMode: Text.Wrap
                        // 超宽自动换行
                    }
                    CheckBox {
                        // 自动速度开关
                        id: autoSpeedBox
                        // 控件 id
                        text: "自动"
                        // 选项文字
                        checked: settingsPage.autoSpeed
                        // 绑定当前状态
                        onToggled: settingsPage.autoSpeed = checked
                        // 切换同步回属性
                        contentItem: Text {
                            // 自定义文字样式
                            text: autoSpeedBox.text
                            // 显示选项文字
                            color: Theme.text
                            // 常规文字色
                            font.pixelSize: Theme.fsNormal
                            // 常规字号
                            leftPadding: autoSpeedBox.indicator.width + 6
                            // 给指示器留出左侧空间
                            verticalAlignment: Text.AlignVCenter
                            // 垂直居中
                        }
                    }
                    Row {
                        // 速度指数输入行
                        spacing: 8
                        // 子项间距
                        enabled: !settingsPage.autoSpeed
                        // 自动模式下禁用
                        opacity: settingsPage.autoSpeed ? 0.5 : 1
                        // 自动模式下置半透明
                        Text {
                            // 公式前缀
                            text: "速度 = 1 × 10^"
                            // 显示公式
                            color: Theme.text
                            // 文字色
                            font.pixelSize: Theme.fsNormal
                            // 常规字号
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                        Rectangle {
                            // 指数输入框外框
                            width: 80; height: 30
                            // 尺寸
                            color: Theme.bgInput
                            // 输入框背景色
                            border.color: Theme.borderStrong
                            // 边框色
                            border.width: 1; radius: Theme.smallRadius
                            // 边框与圆角
                            TextField {
                                // 指数输入框
                                id: speedExpField
                                // 控件 id
                                anchors.fill: parent
                                // 填满外框
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                // 左右内边距
                                color: Theme.text
                                // 文字色
                                font.pixelSize: Theme.fsNormal
                                // 常规字号
                                background: null
                                // 不画自带背景
                                selectByMouse: true
                                // 支持鼠标选择
                                text: String(settingsPage.speedExp)
                                // 绑定当前指数
                                validator: IntValidator { bottom: 0; top: 18 }
                                // 只允许 0..18 的整数
                                onTextChanged: {
                                    // 输入变化处理
                                    var n = parseInt(text)
                                    // 解析整数
                                    if (!isNaN(n)) settingsPage.speedExp = n
                                    // 有效则同步回属性
                                }
                            }
                        }
                        Text {
                            // 单位提示
                            text: "时间单位/秒"
                            // 提示文字
                            color: Theme.textGray
                            // 次要文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                        }
                    }
                    CheckBox {
                        // 显示仿真时间开关
                        id: showSimTimeBox
                        // 控件 id
                        text: "显示仿真时间"
                        // 选项文字
                        checked: settingsPage.showSimTime
                        // 绑定当前状态
                        onToggled: settingsPage.showSimTime = checked
                        // 切换同步回属性
                        contentItem: Text {
                            // 自定义文字样式
                            text: showSimTimeBox.text
                            // 显示选项文字
                            color: Theme.text
                            // 常规文字色
                            font.pixelSize: Theme.fsNormal
                            // 常规字号
                            leftPadding: showSimTimeBox.indicator.width + 6
                            // 给指示器留出左侧空间
                            verticalAlignment: Text.AlignVCenter
                            // 垂直居中
                        }
                    }
                }
            }

            Text {
                // 校验错误提示
                visible: settingsPage.errorText.length > 0
                // 有错误才显示
                text: "⚠ " + settingsPage.errorText
                // 错误文案
                color: Theme.accentRed
                // 强调红
                font.pixelSize: Theme.fsSmall
                // 小字号
                width: parent.width
                // 宽度铺满
                wrapMode: Text.Wrap
                // 超宽自动换行
            }

            Row {
                // 底部操作按钮行
                spacing: 12
                // 按钮间距
                Rectangle {
                    // 应用并返回按钮
                    width: 120; height: 36; radius: Theme.buttonRadius
                    // 尺寸与圆角
                    color: saveSettingsMa.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                    // 颜色：按下高亮 > 成功色
                    border.color: Theme.borderSuccess; border.width: 1
                    // 成功色边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent; text: "应用并返回"
                        // 居中文字
                        color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                        // 白字与字号
                    }
                    MouseArea {
                        // 点击区
                        id: saveSettingsMa; anchors.fill: parent
                        // 填满按钮
                        onClicked: { settingsPage.apply(); main.settingsOpen = false }
                        // 点击应用设置并关闭设置页
                    }
                }
                Rectangle {
                    // 恢复默认按钮
                    width: 120; height: 36; radius: Theme.buttonRadius
                    // 尺寸与圆角
                    color: resetSettingsMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent; text: "恢复默认"
                        // 居中文字
                        color: Theme.text; font.pixelSize: Theme.fsNormal
                        // 文字颜色与字号
                    }
                    MouseArea {
                        // 点击区
                        id: resetSettingsMa; anchors.fill: parent
                        // 填满按钮
                        onClicked: settingsPage.resetToDefaults()
                        // 点击恢复默认值
                    }
                }
            }
        }
    }

    // 定义"应用设置"函数：校验并写回电路
    function apply() {
        // 清空错误
        errorText = ""

        // 引擎索引映射到字符串
        var newEngine = (engineIndex === 0) ? "event" : "iter"
        // 与当前不同才设置
        if (newEngine !== main.circuitRef.engineType) {
            // 切换引擎
            main.circuitRef.engineType = newEngine
        }

        // 仅事件引擎时应用其专属设置
        if (engineIndex === 0) {
            // 解析门延迟
            var gd = parseInt(gateDelayText)
            // 非法或小于 1 则报错返回
            if (isNaN(gd) || gd < 1) { errorText = "门延迟必须 >= 1"; return }
            // 写回门延迟
            main.circuitRef.globalGateDelay = gd

            // 解析导线延迟
            var wd = parseInt(wireDelayText)
            // 非法或小于 1 则报错返回
            if (isNaN(wd) || wd < 1) { errorText = "导线延迟必须 >= 1"; return }
            // 写回导线延迟
            main.circuitRef.wireDelay = wd

            // 写回自动速度
            main.circuitRef.autoSpeed = autoSpeed
            // 非自动模式才写速度指数
            if (!autoSpeed) {
                // 写回速度指数
                main.circuitRef.simulationSpeedExp = speedExp
            }
        }

        // 写回显示仿真时间开关
        main.circuitRef.showSimTime = showSimTime

        // 连线模式索引映射到字符串
        var wm = (wireModeIndex === 1) ? "ortho"
               : (wireModeIndex === 2 ? "switchable" : "bezier")
        // 与当前不同才设置
        if (wm !== main.circuitRef.wireMode) {
            // 写回连线模式
            main.circuitRef.wireMode = wm
        }

        // 状态栏提示
        main.statusText = "设置已应用"
    }

    // 定义"恢复默认值"函数
    function resetToDefaults() {
        // 门延迟默认 1
        gateDelayText = "1"
        // 导线延迟默认 1
        wireDelayText = "1"
        // 速度指数默认 3
        speedExp = 3
        // 自动速度默认开
        autoSpeed = true
        // 显示仿真时间默认关
        showSimTime = false
        // 连线模式默认贝塞尔
        wireModeIndex = 0
    }
}