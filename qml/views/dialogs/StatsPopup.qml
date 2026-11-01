import QtQuick
// 引入 QtQuick 基础模块：Rectangle/Text/Column/Repeater 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：Popup 弹窗基类在此
import QtQuick.Layouts
// 引入 Layouts 模块：ColumnLayout/RowLayout 布局在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Popup {
    // 电路统计弹窗：显示各类元件数量（递归含子电路）
    id: statPopup
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 circuitRef.statistics() 等

    anchors.centerIn: parent
    // 居中显示在父元素内
    width: 340
    // 弹窗宽度
    height: 500
    // 弹窗高度
    modal: true
    // 模态：阻止用户操作背后的界面
    focus: true
    // 打开时获取焦点，支持键盘操作
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // 按 Esc 或点击外部关闭

    background: Rectangle {
        // 自定义弹窗背景
        color: Theme.bgDialog
        // 背景色取主题对话框背景色
        border.color: Theme.borderDialog
        // 边框颜色取主题对话框边框色
        border.width: 1
        // 边框宽度
        radius: 8
        // 圆角半径
    }

    contentItem: ColumnLayout {
        // 内容区：纵向布局
        spacing: 0
        // 子项之间无间距

        Rectangle {
            // 标题栏
            Layout.fillWidth: true
            // 占满宽度
            height: 34
            // 标题栏高度
            color: Theme.bgDialogHeader
            // 标题栏背景色
            Text {
                // 标题文字
                anchors.left: parent.left; anchors.leftMargin: 12
                // 左侧对齐并留 12px 边距
                anchors.verticalCenter: parent.verticalCenter
                // 垂直居中
                text: "电路统计（递归含子电路）"
                // 标题内容
                color: Theme.textWhite
                // 白色文字
                font.pixelSize: Theme.fsMedium
                // 中号字号
                font.bold: true
                // 加粗
            }
        }

        Flickable {
            // 统计内容可滚动容器
            Layout.fillWidth: true
            // 占满宽度
            Layout.fillHeight: true
            // 占满剩余高度
            clip: true
            // 裁剪超出部分
            contentWidth: width
            // 内容宽度等于自身宽（无横向滚动）
            contentHeight: statCol.height + 20
            // 内容高度等于内容列高度加底部留白

            Column {
                // 统计条目列
                id: statCol
                // 列 id
                x: 12; y: 6
                // 左上内边距
                width: parent.width - 24
                // 宽度为容器宽减左右内边距
                spacing: 5
                // 条目间距

                Repeater {
                    // 按统计模型生成条目
                    model: statModel
                    // 数据模型：统计条目列表
                    delegate: RowLayout {
                        // 单条统计：左侧名称，右侧数值
                        width: statCol.width
                        // 宽度占满内容列
                        spacing: 10
                        // 名称与数值间距
                        Text {
                            // 条目名称
                            text: model.k
                            // 显示名称
                            color: Theme.textGray
                            // 次要文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            Layout.fillWidth: true
                            // 占满剩余宽度
                        }
                        Text {
                            // 条目数值
                            text: model.v
                            // 显示数值
                            color: Theme.text
                            // 常规文字色
                            font.pixelSize: Theme.fsSmall
                            // 小字号
                            font.bold: true
                            // 加粗突出数值
                        }
                    }
                }
            }
        }
    }

    ListModel { id: statModel }
    // 统计条目数据模型：由 refreshStats 填充

    onOpened: refreshStats()
    // 打开时刷新统计数据

    // 定义"刷新统计数据"函数：查询电路统计并填充模型
    function refreshStats() {
        // 取统计结果
        var s = main.circuitRef.statistics()
        // 清空旧条目
        statModel.clear()
        // 逻辑门总数
        statModel.append({ k: "逻辑门总数", v: s.totalGates })
        // 与门数量
        statModel.append({ k: "  与门 AND", v: s.byType["and"] || 0 })
        // 或门数量
        statModel.append({ k: "  或门 OR", v: s.byType["or"] || 0 })
        // 非门数量
        statModel.append({ k: "  非门 NOT", v: s.byType["not"] || 0 })
        // 与非门数量
        statModel.append({ k: "  与非 NAND", v: s.byType["nand"] || 0 })
        // 或非门数量
        statModel.append({ k: "  或非 NOR", v: s.byType["nor"] || 0 })
        // 异或门数量
        statModel.append({ k: "  异或 XOR", v: s.byType["xor"] || 0 })
        // 同或门数量
        statModel.append({ k: "  同或 XNOR", v: s.byType["xnor"] || 0 })
        // 传输门数量
        statModel.append({ k: "  传输门 TG", v: s.byType["tgate"] || 0 })
        // NMOS 数量
        statModel.append({ k: "  NMOS 管", v: s.byType["ntran"] || 0 })
        // PMOS 数量
        statModel.append({ k: "  PMOS 管", v: s.byType["ptran"] || 0 })
        // 输入元件数量
        statModel.append({ k: "输入元件", v: s.totalInputs })
        // 输出/LED 数量
        statModel.append({ k: "输出 / LED", v: s.totalOutputs })
        // 分线器数量
        statModel.append({ k: "分线器", v: s.totalSplits })
        // 集线器数量
        statModel.append({ k: "集线器", v: s.totalHubs })
        // 时钟数量
        statModel.append({ k: "时钟", v: s.totalClocks })
        // 文字元件数量
        statModel.append({ k: "文字", v: s.totalTexts })
        // 子电路实例数量
        statModel.append({ k: "子电路实例", v: s.totalSubInst })
        // 子电路定义数量
        statModel.append({ k: "子电路定义数", v: s.subCount })
    }
}