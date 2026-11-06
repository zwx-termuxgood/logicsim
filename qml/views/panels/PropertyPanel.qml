import QtQuick
// 引入 QtQuick 基础模块：Flickable/Column/Text/Row/Repeater/Flow/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：TextField/SpinBox/ComboBox 等控件在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它面板一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）
import "props"
// 引入属性子组件目录：LedProps/SplitterProps/TextProps/SubcircuitPanel 等

Flickable {
    // 属性面板：可纵向滚动，按选中对象显示不同属性组
    id: propertyPanel
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问选中状态、电路接口等

    clip: true
    // 裁剪超出部分
    contentWidth: width
    // 内容宽度等于自身宽（无横向滚动）
    contentHeight: propsColumn.height + 12
    // 内容高度等于内容列高度加底部留白
    boundsBehavior: Flickable.StopAtBounds
    // 到边界即停，不回弹

    property alias inputValueField: inputValueField
    // 把输入值输入框暴露给外部
    property alias splitterSplitsInput: splitterProps.splitterSplitsInput
    // 把分段输入框暴露给外部
    property alias textContentField: textProps.textContentField
    // 把文本内容输入框暴露给外部

    // ★ 统一安全访问（null-safe）
    readonly property var selComp: main ? (main.selectedComp || null) : null
    // 当前选中元件（可能为 null）
    readonly property string selCompId: main ? (main.selectedCompId || "") : ""
    // 当前选中元件 id
    readonly property string selCompType: selComp ? (selComp.type || "") : ""
    // 当前选中元件类型
    readonly property string selSubId: main ? (main.selectedSubId || "") : ""
    // 当前选中子电路 id
    readonly property string selWireId: main ? (main.selectedWireId || "") : ""
    // 当前选中连线 id
    readonly property bool viewOnly: main ? main.circuitViewOnly : false
    // 是否只读浏览
    readonly property string engineType: main ? (main.engineType || "") : ""
    // 当前引擎类型
    readonly property int inputCount: selComp ? (selComp.inputCount || 2) : 2
    // 选中元件输入端口数，缺省 2
    readonly property int bitWidth: selComp ? (selComp.bitWidth || 1) : 1
    // 选中元件位宽，缺省 1
    readonly property string displayBase: selComp ? (selComp.displayBase || "bin") : "bin"
    // 选中元件显示进制，缺省二进制
    readonly property string compName: selComp ? (selComp.name || "") : ""
    // 选中元件名称

    // 定义"是否为多输入逻辑门类型"函数
    function isGateType(t) {
        // 与/或/与非/或非/异或/同或
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor"
    }
    // 定义"是否为可设置门延迟的门类元件"函数
    function isGateLike(t) {
        // 多输入门 + 非门 + 晶体管 + 分线/汇线器
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor" ||
               t === "not" || t === "tgate" || t === "ntran" ||
               t === "ptran" || t === "splitter" || t === "hub"
    }
    // 定义"是否为输入源类元件"函数
    function isInputLike(t) {
        // 输入开关与常量
        return t === "input" || t === "const"
    }

    Column {
        // 面板内容列
        id: propsColumn
        // 列 id
        width: propertyPanel.width
        // 宽等于面板宽
        spacing: 5
        // 子项间距
        Item { width: 1; height: 6 }
        // 顶部留白

        Text {
            // 面板标题：多选提示 / 连线属性 / 属性
            visible: (main && (main.mode === "select"
                               || propertyPanel.selComp !== null
                               || propertyPanel.selSubId !== ""
                               || propertyPanel.selWireId !== ""))
            // 选择模式或有选中对象时显示
            x: 8
            // 左侧缩进
            text: main && main.mode === "select"
                  ? ("已选中 " + main.selectedIds.length + " 个元件")
                  : (propertyPanel.selWireId !== "" ? "连线属性" : "属性")
            // 选择模式显示多选数量，否则按选中类型显示标题
            color: Theme.textWeak
            // 弱化文字色
            font.pixelSize: Theme.fsSmall; font.bold: true
            // 小字号加粗
        }

        // 多选操作面板（有 wire 选中时隐藏）
        Column {
            // 多选时的复制/粘贴/删除操作组
            visible: main && main.mode === "select" && propertyPanel.selWireId === ""
            // 仅选择模式且未选中连线时可见
            x: 6; width: parent.width - 12; spacing: 4
            // 左右缩进、宽度与间距

            Rectangle {
                // 复制按钮
                width: parent.width; height: 30; radius: Theme.smallRadius
                // 宽度铺满，尺寸与圆角
                color: copyBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: main && main.selectedIds.length > 0 ? Theme.accent : Theme.borderStrong
                // 边框：有选中用强调色，否则普通
                border.width: 1
                // 边框宽度
                opacity: main && main.selectedIds.length > 0 ? 1 : 0.5
                // 无选中时半透明
                Text {
                    // 按钮文字
                    anchors.centerIn: parent
                    // 居中
                    text: "复制 (" + (main ? main.selectedIds.length : 0) + ")"
                    // 文字带选中数量
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与小字号
                }
                MouseArea {
                    // 点击区
                    id: copyBtn; anchors.fill: parent
                    // 填满按钮
                    enabled: main && main.selectedIds.length > 0
                    // 有选中才可用
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        main.clipboardData = main.circuitRef.copySelection(main.selectedIds)
                        // 复制选中元件到剪贴板
                        main.statusText = "已复制 " + main.selectedIds.length + " 个元件"
                        // 状态栏提示
                    }
                }
            }
            Rectangle {
                // 粘贴按钮
                width: parent.width; height: 30; radius: Theme.smallRadius
                // 宽度铺满，尺寸与圆角
                visible: main && main.clipboardData !== null
                // 有剪贴板内容才可见
                color: pasteBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "粘贴"
                    // 居中文字"粘贴"
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    // 文字颜色与小字号
                }
                MouseArea {
                    // 点击区
                    id: pasteBtn; anchors.fill: parent
                    // 填满按钮
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        var newIds = main.circuitRef.pasteSelection(main.clipboardData, 30, 30)
                        // 粘贴并偏移 30,30
                        main.selectedIds = newIds
                        // 选中新粘贴的元件
                        main.statusText = "已粘贴 " + newIds.length + " 个元件（已选中）"
                        // 状态栏提示
                    }
                }
            }
            Rectangle {
                // 删除选中按钮
                width: parent.width; height: 30; radius: Theme.smallRadius
                // 宽度铺满，尺寸与圆角
                visible: main && main.selectedIds.length > 0
                // 有选中才可见
                color: da.pressed ? "#6e2020" : Theme.bgButtonDanger
                // 颜色：按下深红 > 危险色
                border.color: Theme.borderDanger; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "删除选中"
                    // 居中文字
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                    // 白字与小字号
                }
                MouseArea {
                    // 点击区
                    id: da; anchors.fill: parent
                    // 填满按钮
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        main.circuitRef.removeComponents(main.selectedIds)
                        // 批量删除选中元件
                        main.selectedIds = []
                        // 清空多选
                    }
                }
            }
        }

        // 子电路属性
        SubcircuitPanel {
            // 子电路属性子面板
            id: subcircuitPanel
            // 面板 id
            main: propertyPanel.main
            // 传递主窗口引用
            visible: main && propertyPanel.selSubId !== "" && main.mode !== "select"
            // 选中子电路且非选择模式时可见
            x: 6
            // 左侧缩进
            width: parent.width - 12
            // 宽度略窄于父列
        }

        // 连线属性
        Column {
            // 连线属性组
            id: wirePropsColumn
            // 组 id
            visible: main && propertyPanel.selWireId !== ""
            // 选中连线时可见
            x: 6; width: parent.width - 12; spacing: 5
            // 缩进、宽度与间距

            Text {
                // 组标题
                text: "连线"
                // 标题内容
                color: Theme.textWeak; font.pixelSize: Theme.fsSmall; font.bold: true
                // 弱化文字色、小字号加粗
            }
            Text {
                // 连线 id
                text: "ID：" + propertyPanel.selWireId
                // 显示连线 id
                color: Theme.text; font.pixelSize: Theme.fsSmall
                // 文字颜色与小字号
            }
            Text {
                // 连线类型
                text: "类型：" + (main ? (main.circuitRef.getWireType(propertyPanel.selWireId) || "bezier") : "bezier")
                // 显示线型，缺省 bezier
                color: Theme.text; font.pixelSize: Theme.fsSmall
                // 文字颜色与小字号
            }

            Rectangle {
                // 删除连线按钮
                width: parent.width; height: 30; radius: Theme.smallRadius
                // 宽度铺满，尺寸与圆角
                color: delWireMa.pressed ? "#6e2020" : Theme.bgButtonDanger
                // 颜色：按下深红 > 危险色
                border.color: Theme.borderDanger; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "删除连线"
                    // 居中文字
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                    // 白字与小字号
                }
                MouseArea {
                    // 点击区
                    id: delWireMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        if (propertyPanel.selWireId === "") return
                        // 无选中连线则忽略
                        main.circuitRef.removeWire(propertyPanel.selWireId)
                        // 删除该连线
                        main.selectedWireId = ""
                        // 清空选中连线
                    }
                }
            }
        }

        // 元件属性
        Column {
            // 元件属性组
            id: compPropsColumn
            // 组 id
            visible: main && propertyPanel.selComp !== null
                     && propertyPanel.selSubId === ""
                     && propertyPanel.selWireId === ""
            // 选中元件且未选中子电路/连线时可见
            x: 6; width: parent.width - 12; spacing: 5
            // 缩进、宽度与间距

            Text {
                // 元件 id
                text: "ID：" + (propertyPanel.selComp ? (propertyPanel.selComp.id || "") : "")
                // 显示元件 id
                color: Theme.text; font.pixelSize: Theme.fsSmall
                // 文字颜色与小字号
            }
            Text {
                // 元件类型
                text: "类型：" + propertyPanel.selCompType
                // 显示元件类型
                color: Theme.text; font.pixelSize: Theme.fsSmall
                // 文字颜色与小字号
            }

            Row {
                // 名称编辑行
                spacing: 4
                // 标签与输入框间距
                Text {
                    // 标签
                    text: "名称"; color: Theme.textGray
                    // 标签文字与次要色
                    font.pixelSize: Theme.fsSmall; width: 36
                    // 小字号与固定宽度
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                }
                Rectangle {
                    // 输入框外框
                    width: compPropsColumn.width - 42; height: 26
                    // 宽度自适应，固定高度
                    color: Theme.bgInput
                    // 输入框背景色
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    radius: Theme.smallRadius
                    // 圆角
                    TextField {
                        // 名称输入框
                        anchors.fill: parent
                        // 填满外框
                        anchors.leftMargin: 5; anchors.rightMargin: 5
                        // 左右内边距
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                        // 文字颜色与小字号
                        background: null
                        // 不画自带背景
                        text: propertyPanel.compName
                        // 绑定当前名称
                        selectByMouse: true
                        // 支持鼠标选择
                        readOnly: propertyPanel.viewOnly
                        // 只读模式不可编辑
                        onEditingFinished: {
                            // 编辑结束处理
                            if (!main) return
                            // main 为空则忽略
                            if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                            // 无选中或只读则忽略
                            main.circuitRef.renameComponent(propertyPanel.selCompId, text)
                            // 提交重命名
                        }
                    }
                }
            }

            Column {
                // 方向选择组
                spacing: 4; width: parent.width
                // 间距与宽度
                visible: propertyPanel.selComp !== null && propertyPanel.selCompType !== "text"
                // 选中非文本元件时可见

                Text {
                    // 组标签
                    text: "方向"
                    // 标签内容
                    color: Theme.textGray; font.pixelSize: Theme.fsSmall
                    // 次要色与小字号
                }
                Row {
                    // 四个方向按钮
                    spacing: 6
                    // 按钮间距
                    Repeater {
                        // 按方向数据生成按钮
                        model: [
                            // 方向数据：角度、图标、旋转值
                            { deg: 0,   label: "→", rot: 0 },
                            // 0 度
                            { deg: 90,  label: "↓", rot: 1 },
                            // 90 度
                            { deg: 180, label: "←", rot: 2 },
                            // 180 度
                            { deg: 270, label: "↑", rot: 3 }
                            // 270 度
                        ]
                        delegate: Rectangle {
                            // 单个方向按钮
                            width: 30; height: 30; radius: Theme.smallRadius
                            // 尺寸与圆角
                            property bool active: propertyPanel.selComp
                                ? ((propertyPanel.selComp.rotation || 0) === modelData.rot)
                                : (modelData.rot === 0)
                            // 是否为当前方向
                            color: active ? Theme.bgButtonActive
                                          : (rotMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                            // 颜色：当前方向激活色 > 按下高亮 > 普通
                            border.color: active ? Theme.borderBtnActive : Theme.borderStrong
                            // 边框：当前方向激活边框，否则普通
                            border.width: 1
                            // 边框宽度
                            Text {
                                // 方向图标
                                anchors.centerIn: parent
                                // 居中
                                text: modelData.label
                                // 显示箭头
                                color: active ? Theme.textWhite : Theme.text
                                // 当前方向白字，否则常规
                                font.pixelSize: 16
                                // 字号
                            }
                            MouseArea {
                                // 点击区
                                id: rotMa
                                // 点击区 id
                                anchors.fill: parent
                                // 填满按钮
                                enabled: !propertyPanel.viewOnly
                                // 只读模式禁用
                                onClicked: {
                                    // 点击处理
                                    if (!main) return
                                    // main 为空则忽略
                                    if (propertyPanel.selCompId === "") return
                                    // 无选中元件则忽略
                                    main.circuitRef.setComponentProp(
                                        propertyPanel.selCompId, "rotation", modelData.rot)
                                    // 设置旋转值
                                }
                            }
                        }
                    }
                }
            }

            Row {
                // 位宽编辑行
                spacing: 4
                // 标签与控件间距
                Text {
                    // 标签
                    text: "位宽"; color: Theme.textGray
                    // 标签文字与次要色
                    font.pixelSize: Theme.fsSmall; width: 36
                    // 小字号与固定宽度
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                }
                SpinBox {
                    // 位宽数值输入
                    from: 1; to: 64; editable: true
                    // 取值 1..64，可输入
                    // ★ 宽度 90 → 130：原来 Basic 样式的 contentItem 内边距 + 上下指示器
                    //   会挤占文本区，导致只能显示 1 位甚至显示不上。加宽到 130 后
                    //   文本区 ≥ 70px，可以完整显示 "64" 这类数值。
                    width: 130
                    // 宽度
                    font.pixelSize: Theme.fsSmall
                    // 统一字号，与属性面板其它控件一致
                    enabled: !propertyPanel.viewOnly
                    // 只读模式禁用
                    value: propertyPanel.bitWidth
                    // 绑定当前位宽
                    // ★ 自定义 contentItem：显式设置字体、颜色、左右内边距与对齐方式，
                    //   摆脱 Basic 样式默认 contentItem 的 padding / 颜色不确定问题，
                    //   保证数字始终居中可见。
                    contentItem: TextInput {
                        // 显示 SpinBox 当前值的编辑框
                        text: parent.textFromValue(parent.value, parent.locale)
                        // 由 SpinBox 把 value 转成显示文本
                        font: parent.font
                        // 使用 SpinBox 自己的字体
                        color: Theme.text
                        // 使用主题常规文字色，避免深色背景上看不见
                        horizontalAlignment: Qt.AlignHCenter
                        // 水平居中
                        verticalAlignment: Qt.AlignVCenter
                        // 垂直居中
                        leftPadding: 6; rightPadding: 6
                        // 左右内边距，防止文字贴边
                        selectByMouse: true
                        // 支持鼠标选择
                        readOnly: !parent.editable
                        // 不可编辑时只读
                        validator: parent.validator
                        // 继承 SpinBox 的校验器
                    }
                    onValueChanged: {
                        // 值变化处理
                        if (!main) return
                        // main 为空则忽略
                        if (propertyPanel.viewOnly) return
                        // 只读模式忽略
                        if (propertyPanel.selComp && propertyPanel.selCompId !== "" &&
                            propertyPanel.selComp.bitWidth !== value)
                            // 元件存在且值确实变化
                            main.circuitRef.setComponentProp(propertyPanel.selCompId, "bitWidth", value)
                            // 提交位宽修改
                    }
                }
            }

            Row {
                // 输入端口数编辑行
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isGateType(propertyPanel.selCompType)
                // 仅多输入门类型可见
                spacing: 4
                // 标签与控件间距
                Text {
                    // 标签
                    text: "输入"; color: Theme.textGray
                    // 标签文字与次要色
                    font.pixelSize: Theme.fsSmall; width: 36
                    // 小字号与固定宽度
                    anchors.verticalCenter: parent.verticalCenter
                    // 垂直居中
                }
                SpinBox {
                    // 输入端口数数值输入
                    from: 2; to: 64; editable: true
                    // 取值 2..64，可输入
                    // ★ 宽度 90 → 130：同"位宽"SpinBox，避免显示不全。
                    width: 130
                    // 宽度
                    font.pixelSize: Theme.fsSmall
                    // 统一字号
                    enabled: !propertyPanel.viewOnly
                    // 只读模式禁用
                    value: propertyPanel.inputCount
                    // 绑定当前输入端口数
                    // ★ 自定义 contentItem：同"位宽"SpinBox。
                    contentItem: TextInput {
                        // 显示 SpinBox 当前值的编辑框
                        text: parent.textFromValue(parent.value, parent.locale)
                        // 由 SpinBox 把 value 转成显示文本
                        font: parent.font
                        // 使用 SpinBox 自己的字体
                        color: Theme.text
                        // 使用主题常规文字色
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
                    onValueChanged: {
                        // 值变化处理
                        if (!main) return
                        // main 为空则忽略
                        if (propertyPanel.viewOnly) return
                        // 只读模式忽略
                        if (propertyPanel.selComp && propertyPanel.selCompId !== "" &&
                            propertyPanel.selComp.inputCount !== value)
                            // 元件存在且值确实变化
                            main.circuitRef.setComponentProp(propertyPanel.selCompId, "inputCount", value)
                            // 提交输入端口数修改
                    }
                }
            }

            Column {
                // 门延迟覆盖组（仅事件引擎）
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isGateLike(propertyPanel.selCompType)
                         && propertyPanel.engineType === "event"
                // 选中可设置延迟的门类元件且为事件引擎时可见
                spacing: 4
                // 子项间距
                width: parent.width
                // 宽度铺满

                Text {
                    // 组标签
                    text: "传播延迟（该类型门）"
                    // 标签内容
                    color: Theme.textGray
                    // 次要文字色
                    font.pixelSize: Theme.fsSmall
                    // 小字号
                }
                Row {
                    // 延迟输入行
                    spacing: 6
                    // 输入框与按钮间距
                    Rectangle {
                        // 延迟输入框外框
                        width: 90; height: 26
                        // 尺寸
                        color: Theme.bgInput
                        // 输入框背景色
                        border.color: Theme.borderStrong
                        // 边框色
                        border.width: 1; radius: Theme.smallRadius
                        // 边框宽度与圆角
                        TextField {
                            // 延迟输入框
                            anchors.fill: parent
                            // 填满外框
                            anchors.leftMargin: 5; anchors.rightMargin: 5
                            // 左右内边距
                            color: Theme.text
                            // 文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            background: null
                            // 不画自带背景
                            selectByMouse: true
                            // 支持鼠标选择
                            text: {
                                // 显示当前该类型延迟
                                if (!main || propertyPanel.selComp === null) return ""
                                // 无选中返回空
                                var v = main.circuitRef.getGateTypeDelay(propertyPanel.selCompType)
                                // 查询该类型延迟
                                return String(v)
                                // 转字符串显示
                            }
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                            // 只允许最多 18 位数字
                            onEditingFinished: {
                                // 编辑结束处理
                                if (!main) return
                                // main 为空则忽略
                                if (propertyPanel.viewOnly) return
                                // 只读模式忽略
                                if (propertyPanel.selComp === null) return
                                // 无选中元件则忽略
                                var v = parseInt(text)
                                // 解析整数
                                if (isNaN(v) || v < 1) return
                                // 非法或小于 1 则忽略
                                main.circuitRef.setGateTypeDelay(propertyPanel.selCompType, v)
                                // 设置该类型延迟
                                main.statusText = "已设置 " + propertyPanel.selCompType + " 延迟 = " + v
                                // 状态栏提示
                            }
                        }
                    }
                    Rectangle {
                        // 使用全局延迟按钮
                        width: 80; height: 26; radius: Theme.smallRadius
                        // 尺寸与圆角
                        color: gateResetMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                        // 颜色：按下高亮 > 普通
                        border.color: Theme.borderStrong; border.width: 1
                        // 边框
                        Text {
                            // 按钮文字
                            anchors.centerIn: parent; text: "使用全局"
                            // 居中文字
                            color: Theme.text; font.pixelSize: Theme.fsTiny
                            // 文字颜色与极小字号
                        }
                        MouseArea {
                            // 点击区
                            id: gateResetMa
                            // 点击区 id
                            anchors.fill: parent
                            // 填满按钮
                            onClicked: {
                                // 点击处理
                                if (!main) return
                                // main 为空则忽略
                                if (propertyPanel.selComp === null) return
                                // 无选中元件则忽略
                                main.circuitRef.clearGateTypeDelays()
                                // 清除所有类型延迟覆盖
                                main.statusText = "已清除独立延迟"
                                // 状态栏提示
                            }
                        }
                    }
                }
                Text {
                    // 说明文字
                    text: "0 或空 = 使用全局（" + (main ? main.circuitRef.globalGateDelay : 1) + "）"
                    // 说明当前全局延迟值
                    color: Theme.textFaint
                    // 极弱文字色
                    font.pixelSize: Theme.fsTiny
                    // 极小字号
                    width: parent.width
                    // 宽度铺满
                    wrapMode: Text.Wrap
                    // 超宽自动换行
                }
            }

            ComboBox {
                // 显示进制选择
                id: baseCombo
                // 控件 id，供 Connections 引用
                width: parent.width
                // 宽度铺满
                // ★ 显式设置字号，避免 Basic 样式默认字号过大导致文本被截断
                font.pixelSize: Theme.fsSmall
                // 统一字号
                visible: propertyPanel.selComp !== null
                         && (propertyPanel.isInputLike(propertyPanel.selCompType)
                             || propertyPanel.selCompType === "output"
                             || propertyPanel.selCompType === "led")
                // 输入类/输出/LED 时可见
                enabled: !propertyPanel.viewOnly
                // 只读模式禁用
                // ★ 自定义 contentItem：显式控制左/右内边距和文本颜色，
                //   确保 "无符号十进制" 等较长选项在深色背景上能完整显示。
                contentItem: Text {
                    // 显示当前选中项的文本
                    leftPadding: 8
                    // 左侧内边距
                    rightPadding: baseCombo.indicator ? baseCombo.indicator.width + 8 : 24
                    // 右侧给下拉指示箭头留出空间
                    text: baseCombo.displayText
                    // 使用 ComboBox 的显示文本
                    font: baseCombo.font
                    // 继承 ComboBox 字体
                    color: Theme.text
                    // 主题常规文字色
                    verticalAlignment: Text.AlignVCenter
                    // 垂直居中
                    elide: Text.ElideRight
                    // 过长时右侧省略（正常情况下不应该触发）
                }
                model: {
                    // 按位宽生成可用进制选项
                    var bw = propertyPanel.bitWidth
                    // 当前位宽
                    var arr = ["二进制", "无符号十进制", "有符号十进制", "十六进制"]
                    // 基础四项
                    if (bw >= 32) arr.push("单精度浮点")
                    // 位宽 >= 32 增加 f32
                    if (bw >= 64) arr.push("双精度浮点")
                    // 位宽 >= 64 增加 f64
                    return arr
                    // 返回选项列表
                }
                onActivated: {
                    // 选中项变化处理
                    if (!main) return
                    // main 为空则忽略
                    if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                    // 无选中或只读则忽略
                    var bw = propertyPanel.bitWidth
                    // 当前位宽
                    var map = ["bin", "udec", "sdec", "hex"]
                    // 索引到进制字符串映射
                    if (bw >= 32) map.push("f32")
                    // 位宽 >= 32 增加 f32
                    if (bw >= 64) map.push("f64")
                    // 位宽 >= 64 增加 f64
                    main.circuitRef.setInputBase(propertyPanel.selCompId, map[currentIndex] || "bin")
                    // 提交显示进制
                }
                Connections {
                    // 监听选中元件变化以同步下拉框
                    target: main
                    // 目标为主窗口
                    function onSelectedCompChanged() {
                        // 选中元件变化处理
                        if (!main) return
                        // main 为空则忽略
                        if (!propertyPanel.selComp) return
                        // 无选中元件则忽略
                        var b = propertyPanel.selComp.displayBase || "bin"
                        // 取当前显示进制
                        var map = { "bin":0, "udec":1, "sdec":2, "hex":3, "f32":4, "f64":5 }
                        // 进制到索引映射
                        baseCombo.currentIndex = map[b] !== undefined ? map[b] : 0
                        // 同步下拉框索引
                    }
                }
            }

            LedProps {
                // LED 颜色属性子面板
                main: propertyPanel.main
                // 传递主窗口引用
                width: parent.width
                // 宽度铺满
            }

            Column {
                // 非二进制输入值编辑组
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isInputLike(propertyPanel.selCompType)
                         && propertyPanel.displayBase !== "bin"
                // 输入类元件且非二进制显示时可见
                spacing: 4; width: parent.width
                // 间距与宽度
                Text {
                    // 组标签
                    text: "值"; color: Theme.textGray
                    // 标签文字与次要色
                    font.pixelSize: Theme.fsSmall
                    // 小字号
                }
                Rectangle {
                    // 输入框外框
                    width: parent.width; height: 26
                    // 宽度铺满，固定高度
                    color: Theme.bgInput
                    // 输入框背景色
                    border.color: main && main.inputError.length > 0 ? "#aa4040" : Theme.borderStrong
                    // 有输入错误时红边，否则普通
                    border.width: 1; radius: Theme.smallRadius
                    // 边框宽度与圆角
                    TextField {
                        // 输入值输入框
                        id: inputValueField
                        // 控件 id，供 alias 暴露
                        anchors.fill: parent
                        // 填满外框
                        anchors.leftMargin: 5; anchors.rightMargin: 5
                        // 左右内边距
                        color: Theme.text
                        // 文字色
                        font.pixelSize: Theme.fsSmall; font.family: "monospace"
                        // 小字号等宽字体
                        background: null; selectByMouse: true
                        // 不画自带背景；支持鼠标选择
                        readOnly: propertyPanel.viewOnly
                        // 只读模式不可编辑
                        onEditingFinished: {
                            // 编辑结束处理
                            if (!main) return
                            // main 为空则忽略
                            if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                            // 无选中或只读则忽略
                            main.inputError = main.circuitRef.setInputFromString(
                                                propertyPanel.selCompId, text)
                            // 解析并写入输入值，错误写回 inputError
                        }
                    }
                }
                Text {
                    // 输入错误提示
                    visible: main && main.inputError.length > 0
                    // 有错误才显示
                    text: "⚠ " + (main ? main.inputError : "")
                    // 错误文案
                    color: Theme.accentRed; font.pixelSize: Theme.fsTiny
                    // 强调红与极小字号
                    width: parent.width; wrapMode: Text.Wrap
                    // 宽度铺满并自动换行
                }
            }

            Flow {
                // 二进制逐位开关组
                width: parent.width; spacing: 3
                // 宽度铺满，位块间距
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isInputLike(propertyPanel.selCompType)
                         && propertyPanel.displayBase === "bin"
                // 输入类元件且二进制显示时可见
                Repeater {
                    // 按位宽生成位块
                    model: {
                        // 生成 0..bw-1 的索引列表
                        var bw = propertyPanel.bitWidth
                        // 当前位宽
                        var arr = []
                        // 索引数组
                        for (var i = 0; i < bw; ++i) arr.push(i)
                        // 填充索引
                        return arr
                        // 返回索引列表
                    }
                    delegate: Rectangle {
                        // 单个位块
                        width: 22; height: 22; radius: 3
                        // 尺寸与圆角
                        color: {
                            // 位块背景色
                            if (!propertyPanel.selComp) return Theme.bgButton
                            // 无选中元件用普通按钮色
                            var bits = propertyPanel.selComp.inputBits || []
                            // 取位列表
                            return (bits[index] || false) ? Theme.accentGreen : Theme.bgButton
                            // 该位为 1 用绿色，否则普通
                        }
                        border.color: Theme.borderDialog; border.width: 1
                        // 边框颜色与宽度
                        opacity: propertyPanel.viewOnly ? 0.7 : 1
                        // 只读时半透明
                        Text {
                            // 位值文字
                            anchors.centerIn: parent
                            // 居中
                            text: {
                                // 计算显示 0/1
                                if (!propertyPanel.selComp) return "0"
                                // 无选中元件显示 0
                                var bits = propertyPanel.selComp.inputBits || []
                                // 取位列表
                                return (bits[index] || false) ? "1" : "0"
                                // 该位为 1 显示 1，否则 0
                            }
                            color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                            // 白字与极小字号
                        }
                        MouseArea {
                            // 位块点击区：翻转该位
                            anchors.fill: parent
                            // 填满位块
                            enabled: !propertyPanel.viewOnly
                            // 只读模式禁用
                            onClicked: {
                                // 点击处理
                                if (!main) return
                                // main 为空则忽略
                                if (propertyPanel.selCompId === "") return
                                // 无选中元件则忽略
                                var bits = propertyPanel.selComp.inputBits || []
                                // 取位列表
                                main.circuitRef.setBitValue(
                                    propertyPanel.selCompId, index, !(bits[index] || false))
                                // 翻转该位
                            }
                        }
                    }
                }
            }

            SplitterProps {
                // 分线器/集线器属性子面板
                id: splitterProps
                // 面板 id，供 alias 暴露
                main: propertyPanel.main
                // 传递主窗口引用
                width: parent.width
                // 宽度铺满
            }

            TextProps {
                // 文本元件属性子面板
                id: textProps
                // 面板 id，供 alias 暴露
                main: propertyPanel.main
                // 传递主窗口引用
                width: parent.width
                // 宽度铺满
            }

            Column {
                // 晶体管/传输门端口说明组
                visible: propertyPanel.selComp !== null
                         && (propertyPanel.selCompType === "tgate"
                             || propertyPanel.selCompType === "ntran"
                             || propertyPanel.selCompType === "ptran")
                // 选中晶体管类元件时可见
                spacing: 4; width: parent.width
                // 间距与宽度
                Text {
                    // 端口说明文字
                    text: propertyPanel.selCompType === "tgate"
                          ? "端口：D / EN → Y\nEN=1 时 Y=D；EN=0 时输出高阻 Z"
                          : (propertyPanel.selCompType === "ntran"
                            ? "端口：G(左) / D(右下) / S(右上)\nG=1 时导通 S=D；G=0 时输出高阻 Z"
                            : "端口：G(左) / D(右上) / S(右下)\nG=0 时导通 S=D；G=1 时输出高阻 Z")
                    // 说明文字结束
                    color: Theme.textFaint
                    // 极弱文字色
                    font.pixelSize: Theme.fsTiny
                    // 极小字号
                    width: parent.width; wrapMode: Text.Wrap
                    // 宽度铺满并自动换行
                }
            }

            Rectangle {
                // 删除元件按钮
                width: parent.width; height: 30; radius: Theme.smallRadius
                // 宽度铺满，尺寸与圆角
                visible: !propertyPanel.viewOnly
                // 只读模式隐藏
                color: delMa.pressed ? "#6e2020" : Theme.bgButtonDanger
                // 颜色：按下深红 > 危险色
                border.color: Theme.borderDanger; border.width: 1
                // 危险色边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "删除"
                    // 居中文字"删除"
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                    // 白字与小字号
                }
                MouseArea {
                    // 点击区
                    id: delMa; anchors.fill: parent
                    // 填满按钮
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        main.confirmDelete(propertyPanel.selCompId)
                        // 弹确认框后删除该元件
                    }
                }
            }
        }

        Rectangle {
            // 错误区与属性区之间的分隔线
            visible: main && main.errorList.length > 0
            // 有错误时才显示
            x: 8; width: parent.width - 16; height: 1
            // 左右缩进、宽度与高度
            color: Theme.border
            // 边框色
        }
        Text {
            // 错误区标题
            visible: main && main.errorList.length > 0
            // 有错误时才显示
            x: 8
            // 左侧缩进
            text: "错误 (" + (main ? main.errorList.length : 0) + ")"
            // 标题带错误数量
            color: Theme.accentRed
            // 强调红
            font.pixelSize: Theme.fsSmall; font.bold: true
            // 小字号加粗
        }
        Repeater {
            // 按错误列表生成错误条目
            model: main ? main.errorList : []
            // 数据模型：错误列表
            delegate: Text {
                // 单条错误
                x: 8; width: propsColumn.width - 16
                // 左右缩进与宽度
                text: "⚠ " + modelData.msg
                // 错误文案
                color: Theme.accentRed
                // 强调红
                font.pixelSize: Theme.fsTiny
                // 极小字号
                wrapMode: Text.Wrap
                // 超宽自动换行
            }
        }

        Item { width: 1; height: 20 }
        // 底部留白
    }
}