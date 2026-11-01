import QtQuick
// 引入 QtQuick 基础模块：Column/Text/Row/Repeater/Rectangle/MouseArea 等基础元素
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：主题等在此（本文件主要用 Theme）

Column {
    // LED 属性面板：选中 LED 时显示颜色选择
    id: ledProps
    // 元素 id，供外部与内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问 selectedComp、circuitRef 等

    visible: main !== null
             && main.selectedComp !== null
             && main.selectedComp !== undefined
             && main.selectedComp.type === "led"
    // 仅当 main 存在、选中元件存在且类型为 led 时可见
    spacing: 4
    // 子项间距

    readonly property var ledColorChoices: [
        // LED 可选颜色列表：名称 + 颜色值
        { name: "黄", value: "#ffd700" },
        // 黄
        { name: "红", value: "#e05252" },
        // 红
        { name: "绿", value: "#4caf50" },
        // 绿
        { name: "蓝", value: "#58a6ff" },
        // 蓝
        { name: "橙", value: "#ffb74d" },
        // 橙
        { name: "紫", value: "#a371f7" },
        // 紫
        { name: "白", value: "#ffffff" }
        // 白
    ]

    Text {
        // 面板标签
        text: "LED 颜色"
        // 标签内容
        color: Theme.textGray
        // 次要文字色
        font.pixelSize: Theme.fsSmall
        // 小字号
    }

    Row {
        // 颜色色块行
        spacing: 6
        // 色块间距
        Repeater {
            // 按颜色列表生成色块
            model: ledProps.ledColorChoices
            // 数据模型：颜色列表
            delegate: Rectangle {
                // 单个颜色色块
                width: 24; height: 24; radius: 4
                // 尺寸与圆角
                color: modelData.value
                // 色块填充为该颜色
                border.width: {
                    // 边框宽度：当前颜色加粗
                    if (!main || !main.selectedComp) return 1
                    // 无选中元件时默认 1
                    var cur = main.selectedComp.color || "#ffd700"
                    // 当前 LED 颜色，缺省金色
                    return (cur === modelData.value) ? 2 : 1
                    // 是当前颜色则 2px，否则 1px
                }
                border.color: {
                    // 边框颜色：当前颜色用白色突出
                    if (!main || !main.selectedComp) return "#555555"
                    // 无选中元件时用灰色
                    var cur = main.selectedComp.color || "#ffd700"
                    // 当前 LED 颜色，缺省金色
                    return (cur === modelData.value) ? "#ffffff" : "#555555"
                    // 是当前颜色则白边，否则灰边
                }
                MouseArea {
                    // 色块点击区
                    anchors.fill: parent
                    // 填满色块
                    enabled: main ? !main.circuitViewOnly : false
                    // 只读模式下禁用
                    onClicked: {
                        // 点击处理
                        if (!main) return
                        // main 为空则忽略
                        if (main.circuitViewOnly) return
                        // 只读模式忽略
                        if (main.selectedCompId === "") return
                        // 无选中元件忽略
                        main.circuitRef.setComponentProp(
                            main.selectedCompId, "color", modelData.value)
                        // 设置 LED 颜色属性
                    }
                }
            }
        }
    }
}