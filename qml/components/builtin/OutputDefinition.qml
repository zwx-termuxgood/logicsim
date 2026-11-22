import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "output"
    displayName: "输出端口"
    category: "输入输出"
    categoryOrder: 0
    libraryOrder: 2
    displayBase: true
    function size(comp) { return { w: Math.max(60, (comp.bitWidth || 1) * 22 + 8), h: 40 } }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#ff9800"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // 子电路实例：圆角矩形
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        // 输出值字符串
        var ovStr = ""
        // 有输入端口值则取第一个
        if (comp.inputPortValues && comp.inputPortValues.length > 0)
            ovStr = comp.inputPortValues[0] || ""
        // 二进制：逐位画小方块
        if (base === "bin") {
            // 逐位绘制
            for (var i2 = 0; i2 < bw; ++i2) {
                // 该位方块左上角 x
                var gx2 = sp.x + (4 + i2 * 22) * viewScale
                // 该位方块左上角 y
                var gy2 = sp.y + 4 * viewScale
                // 该位方块宽
                var gw2 = 20 * viewScale
                // 该位方块高
                var gh2 = h - 8 * viewScale
                // 该位字符，缺失按 '0'
                var ch = (i2 < ovStr.length) ? ovStr.charAt(i2) : '0'
                // 按位状态选填充色
                if (ch === '1')      ctx.fillStyle = "#4caf50"
                else if (ch === 'Z') ctx.fillStyle = "#ffb74d"
                else if (ch === 'E') ctx.fillStyle = "#e05252"
                else                 ctx.fillStyle = "#1a1a1a"
                // 方块描边
                ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                // 画圆角方块
                Geometry.roundRect(ctx, gx2, gy2, gw2, gh2, 3 * viewScale); ctx.fill(); ctx.stroke()
                // 文字颜色
                ctx.fillStyle = "#ffffff"
                // 等宽字体
                ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                // 居中
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                // 画位字符
                ctx.fillText(ch, gx2 + gw2 / 2, gy2 + gh2 / 2)
            }
        // 非二进制：整串格式化后居中显示
        } else {
            // 格式化输出值
            var dispStr = circuit.formatValue(ovStr, base, bw)
            // 文字颜色
            ctx.fillStyle = "#dddddd"
            // 等宽字体
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制字符串
            ctx.fillText(dispStr, sp.x + w / 2, sp.y + h / 2)
        }
    // LED：按显示进制画值
    
        return false
    }
}
