import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 集线器的共享界面定义：注册后复用，用于多段信号汇合的外观与属性入口。
// outputSplits 的段数决定高度，缺省回退位宽；实际端口和信号拼接由模型提供。
// 复用 SplitterProps 编辑分段，editorField 指定同步文本框；gateDelay 开放延迟编辑。
// 绘制返回 false，端口与自定义名称继续由公共流程绘制。
ComponentDefinition {
    typeId: "hub"
    displayName: "集线器"
    category: "工具"
    categoryOrder: 4
    libraryOrder: 1
    gateDelay: true
    propertyEditor: Component { SplitterProps {} }
    editorField: "splitterSplitsInput"
    function size(comp) { return { w: 80, h: Math.max(60, ((comp.outputSplits || []).length || comp.bitWidth || 1) * 24) } }
    function label(comp, circuit) { return "集线器" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // 集线器外壳已绘制，简化模式省略中心文字。
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
