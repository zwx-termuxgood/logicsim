import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "not"
    displayName: "非门"
    category: "基本逻辑门"
    categoryOrder: 1
    libraryOrder: 2
    gateDelay: true
    function size(comp) { return { w: 80, h: 60 } }
    function label(comp, circuit) { return "NOT" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 开始路径
        ctx.beginPath()
        // 三角形左边上下
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x, sp.y + h)
        // 右边尖点
        ctx.lineTo(sp.x + w * 0.85, sp.y + h / 2); ctx.closePath(); ctx.fill(); ctx.stroke()
        // 开始路径
        ctx.beginPath()
        // 输出小圆
        ctx.arc(sp.x + w * 0.85 + 6 * viewScale, sp.y + h / 2, 6 * viewScale, 0, Math.PI * 2)
        // 填充与描边
        ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
    // 分线器/集线器：圆角矩形
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
