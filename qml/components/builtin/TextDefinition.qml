import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 文字注记的共享界面定义：随注册表存活，固定世界尺寸为 140×44。
// TextProps 编辑 comp.content，editorField 指定需同步的输入框；禁止旋转并隐藏额外名称。
// 简化模式省略正文但保留选中框；返回 true 直接结束，不再绘制通用端口与名称。
ComponentDefinition {
    typeId: "text"
    displayName: "文字"
    category: "控制"
    categoryOrder: 5
    libraryOrder: 1
    rotatable: false
    showName: false
    propertyEditor: Component { TextProps {} }
    editorField: "textContentField"
    function size(comp) { return { w: 140, h: 44 } }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 非简化模式才画文字
        if (!simplify) {
            // 文字颜色
            ctx.fillStyle = "#dddddd"
            // 字体
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px sans-serif"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制内容
            ctx.fillText(comp.content || "文本", sp.x + w / 2, sp.y + h / 2)
        }
        // 选中：虚线框
        if (isSel) {
            // 选中颜色
            ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 1.5
            // 虚线模式
            ctx.setLineDash([4, 3])
            // 绘制选中框
            ctx.strokeRect(sp.x, sp.y, w, h)
            // 复位虚线
            ctx.setLineDash([])
        }

        return true
    }
}
