import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/MouseArea/Column/Item 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Menu/MenuItem/MenuSeparator/Dialog/TextField 等在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Rectangle {
    // 左侧面板：元件库（编辑模式）+ 属性面板
    id: leftPanel
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问模式、选中状态、电路接口等

    property alias inputValueField: propertyPanel.inputValueField
    // 把属性面板的输入值输入框暴露给外部
    property alias splitterSplitsInput: propertyPanel.splitterSplitsInput
    // 把属性面板的分段输入框暴露给外部
    property alias textContentField: propertyPanel.textContentField
    // 把属性面板的文本内容输入框暴露给外部

    property bool paletteCollapsed: false
    // 元件库是否收起
    property var groupExpanded: ({})
    // 各元件分组的展开状态：key 为分组名，false 表示收起

    anchors.left: parent.left
    // 左侧对齐父元素
    anchors.bottom: parent.bottom
    // 底部对齐父元素
    width: Theme.leftPanelW
    // 宽度取主题定义的左侧面板宽度
    color: Theme.bgPanel
    // 背景色取主题面板背景色
    border.color: Theme.border
    // 边框颜色取主题边框色
    border.width: 1
    // 边框宽度

    readonly property bool showPalette: main.mode === "edit" && !main.circuitViewOnly
    // 是否显示元件库：编辑模式且非只读浏览

    // ★ 属性面板可见性：select 模式 / 选中元件 / 选中子电路 / 选中线 都显示
    readonly property bool hasPropsContent:
        main.mode === "select"
        || main.selectedComp !== null
        || main.selectedSubId !== ""
        || main.selectedWireId !== ""
    // 是否显示属性面板：选择模式或有选中对象

    // 定义"按 id 查子电路信息"函数
    function findSubInfo(subId) {
        // 遍历子电路列表
        for (var i = 0; i < main.subList.length; ++i) {
            // 命中则返回该项
            if (main.subList[i].id === subId) return main.subList[i]
        }
        // 未找到返回 null
        return null
    }

    // 定义"判断分组是否展开"函数
    function isGroupExpanded(groupName) {
        // 状态未显式设为 false 时默认展开
        return groupExpanded[groupName] !== false
    }

    // 定义"切换分组展开状态"函数
    function toggleGroup(groupName) {
        // 复制当前状态到新对象（触发属性变更通知）
        var g = {}
        for (var k in groupExpanded) g[k] = groupExpanded[k]
        // 翻转该分组的展开状态
        g[groupName] = !isGroupExpanded(groupName)
        // 写回
        groupExpanded = g
    }

    readonly property var elementGroups: [
        // 元件库分组数据：每组含名称与元件项列表
        { name: "输入输出", items: [
            // 输入输出组
            { type: "input", name: "输入开关" },
            // 输入开关
            { type: "const", name: "常量" },
            // 常量
            { type: "output", name: "输出端口" },
            // 输出端口
            { type: "led", name: "LED" }
            // LED
        ]},
        { name: "基本逻辑门", items: [
            // 基本逻辑门组
            { type: "and", name: "与门" },
            // 与门
            { type: "or", name: "或门" },
            // 或门
            { type: "not", name: "非门" }
            // 非门
        ]},
        { name: "复合逻辑门", items: [
            // 复合逻辑门组
            { type: "nand", name: "与非" },
            // 与非门
            { type: "nor", name: "或非" },
            // 或非门
            { type: "xor", name: "异或" },
            // 异或门
            { type: "xnor", name: "同或" }
            // 同或门
        ]},
        { name: "传输门 / 晶体管", items: [
            // 传输门/晶体管组
            { type: "tgate", name: "传输门" },
            // 传输门
            { type: "ntran", name: "NMOS 管" },
            // NMOS
            { type: "ptran", name: "PMOS 管" }
            // PMOS
        ]},
        { name: "工具", items: [
            // 工具组
            { type: "splitter", name: "分线器" },
            // 分线器
            { type: "hub", name: "集线器" }
            // 集线器
        ]},
        { name: "控制", items: [
            // 控制组
            { type: "clock", name: "时钟" },
            // 时钟
            { type: "text", name: "文字" }
            // 文字
        ]}
    ]

    ColumnLayout {
        // 面板纵向布局
        anchors.fill: parent
        // 填满面板
        spacing: 0
        // 无间距

        Rectangle {
            // 顶部收起按钮条
            Layout.fillWidth: true
            // 占满宽度
            Layout.preferredHeight: 24
            // 高度
            color: Theme.bgBar
            // 背景色
            Rectangle {
                // 收起按钮
                anchors.right: parent.right
                // 右侧对齐
                anchors.rightMargin: 4
                // 右边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                width: 60; height: 20
                // 尺寸
                radius: Theme.smallRadius
                // 圆角
                color: leftCollapseMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent; text: "◀ 收起"
                    // 居中文字
                    color: Theme.text; font.pixelSize: Theme.fsTiny
                    // 文字颜色与小字号
                }
                MouseArea {
                    // 点击区
                    id: leftCollapseMa
                    // 点击区 id
                    anchors.fill: parent
                    // 填满按钮
                    onClicked: main.leftCollapsed = true
                    // 点击收起左侧面板
                }
            }
        }

        Rectangle {
            // 元件库标题条
            Layout.fillWidth: true
            // 占满宽度
            Layout.preferredHeight: 22
            // 高度
            visible: leftPanel.showPalette
            // 仅编辑模式可见
            color: Theme.bgBar
            // 背景色

            Text {
                // 标题文字
                anchors.left: parent.left; anchors.leftMargin: 8
                // 左侧对齐并留 8px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                text: "元件库"
                // 标题内容
                color: Theme.textWeak
                // 弱化文字色
                font.pixelSize: Theme.fsSmall
                // 小字号
                font.bold: true
                // 加粗
            }

            Rectangle {
                // 元件库展开/收起按钮
                anchors.right: parent.right; anchors.rightMargin: 4
                // 右侧对齐并留 4px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                width: 52; height: 18
                // 尺寸
                radius: Theme.smallRadius
                // 圆角
                color: paletteToggleMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                // 颜色：按下高亮 > 普通
                border.color: Theme.borderStrong; border.width: 1
                // 边框
                Text {
                    // 按钮文字
                    anchors.centerIn: parent
                    // 居中
                    text: leftPanel.paletteCollapsed ? "▸ 展开" : "▾ 收起"
                    // 文案随收起状态切换
                    color: Theme.text; font.pixelSize: Theme.fsTiny
                    // 文字颜色与小字号
                }
                MouseArea {
                    // 点击区
                    id: paletteToggleMa
                    // 点击区 id
                    anchors.fill: parent
                    // 填满按钮
                    onClicked: leftPanel.paletteCollapsed = !leftPanel.paletteCollapsed
                    // 点击切换收起状态
                }
            }
        }

        Flickable {
            // 元件库可滚动容器
            id: paletteFlick
            // 容器 id
            Layout.fillWidth: true
            // 占满宽度
            Layout.fillHeight: true
            // 占满剩余高度
            Layout.preferredHeight: 1
            // 首选高度 1，避免空时撑开
            visible: leftPanel.showPalette && !leftPanel.paletteCollapsed
            // 编辑模式且未收起时可见
            clip: true
            // 裁剪超出部分
            contentWidth: width
            // 内容宽度等于自身宽
            contentHeight: paletteColumn.height + 12
            // 内容高度等于列高加底部留白
            boundsBehavior: Flickable.StopAtBounds
            // 到边界即停，不回弹

            Column {
                // 元件库内容列
                id: paletteColumn
                // 列 id
                width: paletteFlick.width
                // 宽等于容器宽
                spacing: 3
                // 子项间距
                Item { width: 1; height: 4 }
                // 顶部留白

                Repeater {
                    // 按分组数据生成分组
                    model: leftPanel.elementGroups
                    // 数据模型：元件分组
                    delegate: Column {
                        // 单个分组
                        id: groupColumn
                        // 分组 id
                        width: paletteColumn.width
                        // 宽等于内容列宽
                        spacing: 2
                        // 组内间距

                        property string groupName: modelData.name
                        // 分组名
                        property var    groupItems: modelData.items
                        // 分组内元件项
                        property bool   expanded: leftPanel.isGroupExpanded(groupName)
                        // 该分组是否展开

                        Rectangle {
                            // 分组标题行
                            width: groupColumn.width
                            // 宽等于分组宽
                            height: 22
                            // 高度
                            color: groupHeaderMa.pressed ? Theme.bgButtonHover : Theme.bgDialog
                            // 颜色：按下高亮 > 对话框背景色

                            Text {
                                // 分组标题文字
                                anchors.left: parent.left; anchors.leftMargin: 8
                                // 左侧对齐并留 8px 边距
                                anchors.verticalCenter: parent.verticalCenter
                                // 垂直居中
                                text: (groupColumn.expanded ? "▾ " : "▸ ") + groupColumn.groupName
                                // 前置展开/收起箭头 + 分组名
                                color: Theme.textWeak
                                // 弱化文字色
                                font.pixelSize: Theme.fsSmall
                                // 小字号
                                font.bold: true
                                // 加粗
                            }

                            MouseArea {
                                // 标题点击区：切换展开
                                id: groupHeaderMa
                                // 点击区 id
                                anchors.fill: parent
                                // 填满标题行
                                onClicked: leftPanel.toggleGroup(groupColumn.groupName)
                                // 点击切换该分组展开状态
                            }
                        }

                        Repeater {
                            // 按分组内元件项生成条目
                            model: groupColumn.expanded ? groupColumn.groupItems : []
                            // 收起时为空列表，不生成条目
                            delegate: Rectangle {
                                // 单个元件条目
                                width: groupColumn.width - 10
                                // 宽度略窄于分组
                                x: 5
                                // 左侧缩进
                                height: 28
                                // 条目高度
                                radius: Theme.smallRadius
                                // 圆角
                                color: (main.placingType === modelData.type && main.placingSubId === "")
                                       ? Theme.bgButtonHover
                                       : (itemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                                // 颜色：正在放置该项高亮 > 按下高亮 > 普通
                                border.color: (main.placingType === modelData.type && main.placingSubId === "")
                                              ? Theme.borderBtnActive : Theme.borderWeak
                                // 边框：正在放置该项用激活边框，否则弱边框
                                border.width: 1
                                // 边框宽度
                                Text {
                                    // 条目文字
                                    anchors.left: parent.left; anchors.leftMargin: 10
                                    // 左侧对齐并留 10px 边距
                                    anchors.verticalCenter: parent.verticalCenter
                                    // 垂直居中
                                    text: modelData.name
                                    // 显示元件名
                                    color: Theme.text; font.pixelSize: Theme.fsNormal
                                    // 文字颜色与字号
                                }
                                MouseArea {
                                    // 条目点击区
                                    id: itemMa; anchors.fill: parent
                                    // 填满条目
                                    onClicked: {
                                        // 点击处理
                                        if (main.placingType === modelData.type && main.placingSubId === "") {
                                            // 再次点击同一项：取消放置
                                            main.placingType = ""
                                        } else {
                                            // 选择该项进入放置态
                                            main.placingType = modelData.type
                                            // 记录待放置类型
                                            main.placingSubId = ""
                                            // 清空待放置子电路
                                            main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                            // 状态栏提示
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    // 子电路分组标题行
                    width: parent.width; height: 22; color: Theme.bgDialog
                    // 宽铺满，固定高度，对话框背景色
                    Text {
                        // 标题文字
                        anchors.left: parent.left; anchors.leftMargin: 8
                        // 左侧对齐并留 8px 边距
                        anchors.verticalCenter: parent.verticalCenter
                        // 垂直居中
                        text: (leftPanel.isGroupExpanded("__sub__") ? "▾ " : "▸ ")
                              + "子电路 (" + main.subList.length + ")"
                        // 展开箭头 + 名称 + 数量
                        color: Theme.accentPurple
                        // 紫色强调
                        font.pixelSize: Theme.fsSmall; font.bold: true
                        // 小字号加粗
                    }
                    MouseArea {
                        // 标题点击区：切换展开
                        anchors.fill: parent
                        // 填满标题行
                        onClicked: leftPanel.toggleGroup("__sub__")
                        // 点击切换子电路分组展开状态
                    }
                }
                Rectangle {
                    // 新建子电路按钮
                    visible: leftPanel.isGroupExpanded("__sub__")
                    // 分组展开时可见
                    width: parent.width - 10; x: 5; height: 28; radius: Theme.smallRadius
                    // 宽度略窄，左缩进，尺寸与圆角
                    color: newSubMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderSub; border.width: 1
                    // 子电路紫色边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent; text: "+ 新建子电路"
                        // 居中文字
                        color: Theme.accentPurple; font.pixelSize: Theme.fsSmall
                        // 紫色强调与小字号
                    }
                    MouseArea { id: newSubMa; anchors.fill: parent
                        // 点击区填满按钮
                        onClicked: main.subNameDialogRef.open() }
                        // 点击打开新建子电路对话框
                }

                Repeater {
                    // 按子电路列表生成条目
                    model: leftPanel.isGroupExpanded("__sub__") ? main.subList : []
                    // 收起时为空列表
                    delegate: Rectangle {
                        // 单个子电路条目
                        width: parent.width - 10; x: 5; height: 32
                        // 宽度略窄，左缩进，条目高度
                        radius: Theme.smallRadius
                        // 圆角
                        color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                               ? Theme.bgButtonHover
                               : (subItemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                        // 颜色：正在放置该子电路高亮 > 按下高亮 > 普通
                        border.color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                                      ? Theme.accentPurple : Theme.borderWeak
                        // 边框：正在放置该子电路用紫色，否则弱边框
                        border.width: 1
                        // 边框宽度

                        Text {
                            // 子电路名称与端口数
                            anchors.left: parent.left; anchors.leftMargin: 10
                            // 左侧对齐并留 10px 边距
                            anchors.right: moreBtn.left; anchors.rightMargin: 4
                            // 右侧止于更多按钮左侧并留 4px 边距
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                            text: modelData.name + " [" + modelData.inCount + "/" + modelData.outCount + "]"
                            // 名称 + 输入/输出端口数
                            color: Theme.text; font.pixelSize: Theme.fsSmall
                            // 文字颜色与小字号
                            elide: Text.ElideRight
                            // 过长时右侧省略
                        }

                        MouseArea {
                            // 条目主体点击区（左键选中放置，右键/长按弹菜单）
                            id: subItemMa
                            // 点击区 id
                            anchors.left: parent.left
                            // 左侧对齐
                            anchors.top: parent.top
                            // 顶部对齐
                            anchors.bottom: parent.bottom
                            // 底部对齐
                            anchors.right: moreBtn.left
                            // 右侧止于更多按钮
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            // 接受左键与右键

                            onPressAndHold: {
                                // 长按：弹出上下文菜单
                                subCtxMenu.subId = modelData.id
                                // 记录子电路 id
                                subCtxMenu.subName = modelData.name
                                // 记录子电路名
                                subCtxMenu.popup()
                                // 弹出菜单
                            }
                            onClicked: function(mouse) {
                                // 点击处理
                                if (mouse.button === Qt.LeftButton) {
                                    // 左键：选中并进入放置态
                                    main.selectedSubId = modelData.id
                                    // 选中该子电路
                                    main.placingType = "sub"
                                    // 进入子电路放置态
                                    main.placingSubId = modelData.id
                                    // 记录待放置子电路
                                    main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                    // 状态栏提示
                                } else if (mouse.button === Qt.RightButton) {
                                    // 右键：弹出上下文菜单
                                    subCtxMenu.subId = modelData.id
                                    // 记录子电路 id
                                    subCtxMenu.subName = modelData.name
                                    // 记录子电路名
                                    subCtxMenu.popup()
                                    // 弹出菜单
                                }
                            }
                        }

                        Rectangle {
                            // 更多操作按钮（⋯）
                            id: moreBtn
                            // 按钮 id，供条目文字与主体点击区右侧对齐引用
                            width: 28; height: parent.height
                            // 尺寸
                            anchors.right: parent.right
                            // 右侧对齐
                            anchors.verticalCenter: parent.verticalCenter
                            // 垂直居中
                            color: moreMa.pressed ? Theme.bgButtonHover : "transparent"
                            // 颜色：按下高亮 > 透明
                            radius: Theme.smallRadius
                            // 圆角
                            Text {
                                // 按钮文字
                                anchors.centerIn: parent
                                // 居中
                                text: "⋯"
                                // 省略号图标
                                color: Theme.textGray
                                // 次要文字色
                                font.pixelSize: 16
                                // 字号
                            }
                            MouseArea {
                                // 点击区
                                id: moreMa; anchors.fill: parent
                                // 填满按钮
                                onClicked: {
                                    // 点击处理
                                    subCtxMenu.subId = modelData.id
                                    // 记录子电路 id
                                    subCtxMenu.subName = modelData.name
                                    // 记录子电路名
                                    subCtxMenu.popup()
                                    // 弹出上下文菜单
                                }
                            }
                        }
                    }
                }
                Item { width: 1; height: 8 }
                // 底部留白
            }
        }

        Rectangle {
            // 元件库与属性面板之间的分隔线
            Layout.fillWidth: true
            // 占满宽度
            Layout.preferredHeight: 1
            // 高度 1px
            color: Theme.border
            // 边框色
            visible: leftPanel.showPalette
                     && !leftPanel.paletteCollapsed
                     && leftPanel.hasPropsContent
            // 元件库可见且有属性内容时显示
        }

        PropertyPanel {
            // 属性面板
            id: propertyPanel
            // 面板 id
            main: leftPanel.main
            // 传递主窗口引用
            Layout.fillWidth: true
            // 占满宽度
            Layout.fillHeight: true
            // 占满剩余高度
            Layout.preferredHeight: 1
            // 首选高度 1，避免空时撑开
            visible: leftPanel.hasPropsContent
            // 有属性内容时可见
        }
    }

    Menu {
        // 子电路上下文菜单
        id: subCtxMenu
        // 菜单 id
        property string subId: ""
        // 当前操作的子电路 id
        property string subName: ""
        // 当前操作的子电路名

        MenuItem {
            // 菜单项：选中并显示属性
            text: "选中（显示属性）"
            // 菜单文字
            onTriggered: {
                // 点击处理
                main.selectedSubId = subCtxMenu.subId
                // 选中该子电路
                main.placingType = "sub"
                // 进入子电路放置态
                main.placingSubId = subCtxMenu.subId
                // 记录待放置子电路
            }
        }
        MenuItem {
            // 菜单项：重命名
            text: "重命名"
            // 菜单文字
            onTriggered: {
                // 点击处理
                renameDialog.subId = subCtxMenu.subId
                // 记录待重命名子电路 id
                renameInput.text = subCtxMenu.subName
                // 预填当前名称
                renameDialog.open()
                // 打开重命名对话框
            }
        }
        MenuItem {
            // 菜单项：设为主电路
            text: "设为主电路"
            // 菜单文字
            onTriggered: main.circuitRef.setSubcircuitAsRoot(subCtxMenu.subId)
            // 点击把该子电路与根上下文互换
        }
        MenuSeparator { }
        // 分隔线
        MenuItem {
            // 菜单项：删除
            text: "删除"
            // 菜单文字
            onTriggered: {
                // 点击处理
                main.confirmDialogRef.title = "删除子电路"
                // 设置确认框标题
                main.confirmDialogRef.message = "确定删除该子电路及其所有实例？"
                // 设置确认框消息
                main.confirmDialogRef.action = function() {
                    // 设置确认后的动作
                    main.circuitRef.deleteSubcircuit(subCtxMenu.subId)
                    // 删除该子电路
                    main.selectedSubId = ""
                    // 清空选中子电路
                }
                main.confirmDialogRef.open()
                // 打开确认框
            }
        }
    }

    // 定义"打开重命名子电路对话框"函数：供外部调用
    function openRenameSubDialog(subId, name) {
        // 记录待重命名子电路 id
        renameDialog.subId = subId
        // 预填当前名称
        renameInput.text = name
        // 打开对话框
        renameDialog.open()
    }

    Dialog {
        // 重命名子电路对话框
        id: renameDialog
        // 对话框 id
        property string subId: ""
        // 待重命名的子电路 id

        anchors.centerIn: parent
        // 居中显示
        modal: true
        // 模态
        title: "重命名子电路"
        // 对话框标题
        standardButtons: Dialog.NoButton
        // 不显示标准按钮
        width: 300
        // 对话框宽度
        padding: 20
        // 内容内边距

        background: Rectangle {
            // 自定义对话框背景
            color: Theme.bgDialog
            // 背景色
            border.color: Theme.borderDialog
            // 边框色
            border.width: 1
            // 边框宽度
            radius: Theme.dialogRadius
            // 圆角
        }

        contentItem: ColumnLayout {
            // 内容区纵向布局
            spacing: 12
            // 子项间距
            Text {
                // 提示文字
                text: "输入新名称"
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
                Layout.preferredHeight: 32
                // 高度
                color: Theme.bgInput
                // 输入框背景色
                border.color: Theme.borderDialog
                // 边框色
                border.width: 1
                // 边框宽度
                radius: Theme.smallRadius
                // 圆角
                TextField {
                    // 名称输入框
                    id: renameInput
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
                    onAccepted: renameDialog.doRename()
                    // 回车确认重命名
                }
            }
            RowLayout {
                // 按钮行
                Layout.fillWidth: true
                // 占满宽度
                Item { Layout.fillWidth: true }
                // 弹性占位，把按钮推到右侧
                Rectangle {
                    // 取消按钮
                    Layout.preferredWidth: 70; Layout.preferredHeight: 30
                    // 尺寸
                    radius: Theme.buttonRadius
                    // 圆角
                    color: cc1r.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderDialog; border.width: 1
                    // 边框
                    Text { anchors.centerIn: parent; text: "取消"
                        color: Theme.text; font.pixelSize: Theme.fsNormal }
                    // 居中文字"取消"
                    MouseArea { id: cc1r; anchors.fill: parent
                        // 点击区填满按钮
                        onClicked: renameDialog.close() }
                        // 点击关闭对话框
                }
                Rectangle {
                    // 确定按钮
                    Layout.preferredWidth: 80; Layout.preferredHeight: 30
                    // 尺寸
                    radius: Theme.buttonRadius
                    // 圆角
                    color: cc2r.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                    // 颜色：按下高亮 > 成功色
                    border.color: Theme.borderSuccess; border.width: 1
                    // 成功色边框
                    Text { anchors.centerIn: parent; text: "确定"
                        color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                    // 居中文字"确定"
                    MouseArea { id: cc2r; anchors.fill: parent
                        // 点击区填满按钮
                        onClicked: renameDialog.doRename() }
                        // 点击执行重命名
                }
            }
        }

        // 定义"执行重命名"函数
        function doRename() {
            // 取去空白后的名称
            var name = renameInput.text.trim()
            // 名称为空则忽略
            if (name.length === 0) return
            // 提交新名称
            main.circuitRef.setSubcircuitName(renameDialog.subId, name)
            // 关闭对话框
            renameDialog.close()
        }
    }
}