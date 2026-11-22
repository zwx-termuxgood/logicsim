import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

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
    // 输出端口：圆角矩形
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
