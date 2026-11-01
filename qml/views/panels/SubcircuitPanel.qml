import QtQuick
// 引入 QtQuick 基础模块：Column/Text/Row/Rectangle 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：TextField 输入框在此
import QtQuick.Layouts
// 引入 Layouts 模块：本文件按需使用（保持与其它面板一致的依赖）
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Column {
    // 子电路属性面板：选中子电路时显示名称、端口信息与操作按钮
    id: subcircuitPanel
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 subList、circuitRef 等

    spacing: 5
    // 子项间距
    width: parent ? parent.width : 200
    // 宽度跟随父元素，缺省 200

    // 定义"按 id 查子电路信息"函数
    function findSubInfo(subId) {
        // main 为空返回 null
        if (!main) return null
        // 子电路列表为空返回 null
        if (!main.subList) return null
        // 遍历子电路列表
        for (var i = 0; i < main.subList.length; ++i) {
            // 命中则返回该项
            if (main.subList[i].id === subId) return main.subList[i]
        }
        // 未找到返回 null
        return null
    }

    readonly property string selSubId: main ? (main.selectedSubId || "") : ""
    // 当前选中子电路 id
    readonly property bool viewOnly: main ? main.circuitViewOnly : false
    // 是否只读浏览

    Rectangle {
        // 面板标题条
        width: parent.width; height: 24; radius: Theme.smallRadius
        // 宽度铺满，固定高度与圆角
        color: Theme.bgDialogHeader
        // 标题条背景色
        Text {
            // 标题文字
            anchors.left: parent.left; anchors.leftMargin: 8
            // 左侧对齐并留 8px 边距
            anchors.verticalCenter: parent.verticalCenter
            // 垂直居中
            text: "子电路"
            // 标题内容
            color: Theme.accentPurple
            // 紫色强调
            font.pixelSize: Theme.fsSmall; font.bold: true
            // 小字号加粗
        }
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
            width: subcircuitPanel.width - 42; height: 26
            // 宽度自适应，固定高度
            color: Theme.bgInput
            // 输入框背景色
            border.color: Theme.borderStrong; border.width: 1
            // 边框
            radius: Theme.smallRadius
            // 圆角
            TextField {
                // 名称输入框
                id: subNameField
                // 控件 id
                anchors.fill: parent
                // 填满外框
                anchors.leftMargin: 5; anchors.rightMargin: 5
                // 左右内边距
                color: Theme.text; font.pixelSize: Theme.fsSmall
                // 文字颜色与小字号
                background: null
                // 不画自带背景
                selectByMouse: true
                // 支持鼠标选择
                readOnly: subcircuitPanel.viewOnly
                // 只读模式不可编辑
                text: {
                    // 显示当前子电路名称
                    var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
                    // 查子电路信息
                    return info ? info.name : ""
                    // 有则显示名称，否则空串
                }
                onEditingFinished: {
                    // 编辑结束处理
                    if (!main) return
                    // main 为空则忽略
                    if (subcircuitPanel.selSubId === "" || subcircuitPanel.viewOnly) return
                    // 无选中或只读则忽略
                    if (text.trim().length === 0) return
                    // 名称为空则忽略
                    main.circuitRef.setSubcircuitName(subcircuitPanel.selSubId, text.trim())
                    // 提交新名称
                }
            }
        }
    }

    Text {
        // 子电路信息：ID 与端口数
        text: {
            // 计算显示文本
            var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
            // 查子电路信息
            if (!info) return ""
            // 无信息返回空串
            return "ID：" + info.id + "\n输入端口：" + info.inCount
                 + "    输出端口：" + info.outCount
            // 拼接 id 与输入/输出端口数
        }
        color: Theme.textGray
        // 次要文字色
        font.pixelSize: Theme.fsSmall
        // 小字号
        wrapMode: Text.Wrap
        // 超宽自动换行
        width: parent.width
        // 宽度铺满
    }

    Rectangle {
        // 重命名按钮
        width: parent.width; height: 30; radius: Theme.smallRadius
        // 宽度铺满，尺寸与圆角
        visible: !subcircuitPanel.viewOnly
        // 只读模式隐藏
        color: renameBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        // 颜色：按下高亮 > 普通
        border.color: Theme.borderStrong; border.width: 1
        // 边框
        Text {
            // 按钮文字
            anchors.centerIn: parent; text: "重命名…"
            // 居中文字
            color: Theme.text; font.pixelSize: Theme.fsSmall
            // 文字颜色与小字号
        }
        MouseArea {
            // 点击区
            id: renameBtn; anchors.fill: parent
            // 填满按钮
            onClicked: {
                // 点击处理
                if (!main) return
                // main 为空则忽略
                var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
                // 查子电路信息
                main.openRenameSubDialog(subcircuitPanel.selSubId, info ? info.name : "")
                // 打开重命名对话框并预填当前名称
            }
        }
    }

    Rectangle {
        // 设为主电路按钮
        width: parent.width; height: 30; radius: Theme.smallRadius
        // 宽度铺满，尺寸与圆角
        visible: !subcircuitPanel.viewOnly
        // 只读模式隐藏
        color: setRootBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        // 颜色：按下高亮 > 普通
        border.color: Theme.borderStrong; border.width: 1
        // 边框
        Text {
            // 按钮文字
            anchors.centerIn: parent; text: "设为主电路"
            // 居中文字
            color: Theme.text; font.pixelSize: Theme.fsSmall
            // 文字颜色与小字号
        }
        MouseArea {
            // 点击区
            id: setRootBtn; anchors.fill: parent
            // 填满按钮
            onClicked: {
                // 点击处理
                if (!main) return
                // main 为空则忽略
                main.confirmDialogRef.title = "设为主电路"
                // 设置确认框标题
                main.confirmDialogRef.message =
                    "确定把该子电路设为主电路？\n当前主电路内容将被移动到原位置。"
                // 设置确认框消息
                main.confirmDialogRef.action = function() {
                    // 设置确认后的动作
                    if (!main) return
                    // main 为空则忽略
                    main.circuitRef.setSubcircuitAsRoot(subcircuitPanel.selSubId)
                    // 把子电路与根上下文互换
                    main.selectedSubId = ""
                    // 清空选中子电路
                }
                main.confirmDialogRef.open()
                // 打开确认框
            }
        }
    }

    Rectangle {
        // 进入浏览按钮
        width: parent.width; height: 30; radius: Theme.smallRadius
        // 宽度铺满，尺寸与圆角
        color: enterBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        // 颜色：按下高亮 > 普通
        border.color: Theme.borderStrong; border.width: 1
        // 边框
        Text {
            // 按钮文字
            anchors.centerIn: parent; text: "进入浏览"
            // 居中文字
            color: Theme.text; font.pixelSize: Theme.fsSmall
            // 文字颜色与小字号
        }
        MouseArea {
            // 点击区
            id: enterBtn; anchors.fill: parent
            // 填满按钮
            onClicked: {
                // 点击处理
                if (!main) return
                // main 为空则忽略
                main.circuitRef.enterSubcircuit(subcircuitPanel.selSubId)
                // 进入该子电路只读浏览
            }
        }
    }

    Rectangle {
        // 删除子电路按钮
        width: parent.width; height: 30; radius: Theme.smallRadius
        // 宽度铺满，尺寸与圆角
        visible: !subcircuitPanel.viewOnly
        // 只读模式隐藏
        color: delSubBtn.pressed ? "#6e2020" : Theme.bgButtonDanger
        // 颜色：按下深红 > 危险色
        border.color: Theme.borderDanger; border.width: 1
        // 危险色边框
        Text {
            // 按钮文字
            anchors.centerIn: parent; text: "删除子电路"
            // 居中文字
            color: Theme.textWhite; font.pixelSize: Theme.fsSmall
            // 白字与小字号
        }
        MouseArea {
            // 点击区
            id: delSubBtn; anchors.fill: parent
            // 填满按钮
            onClicked: {
                // 点击处理
                if (!main) return
                // main 为空则忽略
                var sid = subcircuitPanel.selSubId
                // 先记录待删除子电路 id
                main.confirmDialogRef.title = "删除子电路"
                // 设置确认框标题
                main.confirmDialogRef.message = "确定删除该子电路及其所有实例？"
                // 设置确认框消息
                main.confirmDialogRef.action = function() {
                    // 设置确认后的动作
                    if (!main) return
                    // main 为空则忽略
                    main.circuitRef.deleteSubcircuit(sid)
                    // 删除该子电路
                    main.selectedSubId = ""
                    // 清空选中子电路
                }
                main.confirmDialogRef.open()
                // 打开确认框
            }
        }
    }

    Rectangle { width: parent.width; height: 1; color: Theme.border }
    // 底部细分隔线
}