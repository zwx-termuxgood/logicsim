import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "const"
    displayName: "常量"
    category: "输入输出"
    categoryOrder: 0
    libraryOrder: 1
    inputLike: true
    displayBase: true
    function size(comp) { return { w: Math.max(60, (comp.bitWidth || 1) * 22 + 8), h: 40 } }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充色
        ctx.fillStyle = "#2a2a3a"; ctx.strokeStyle = isSel ? "#ffffff" : "#7c8cff"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // LED：圆形
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        // 取位列表
        var bits = comp.inputBits || []
        // 二进制：逐位画小方块
        if (base === "bin") {
            // 逐位绘制
            for (var i = 0; i < bw; ++i) {
                // 该位方块左上角 x
                var gx = sp.x + (4 + i * 22) * viewScale
                // 该位方块左上角 y
                var gy = sp.y + 4 * viewScale
                // 该位方块宽
                var gw = 20 * viewScale
                // 该位方块高
                var gh = h - 8 * viewScale
                // 该位值
                var v = bits[i] || false
                // 高电平绿，低电平深色
                ctx.fillStyle = v ? "#4caf50" : "#1a1a1a"
                // 方块描边
                ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                // 画圆角方块
                Geometry.roundRect(ctx, gx, gy, gw, gh, 3 * viewScale); ctx.fill(); ctx.stroke()
                // 文字颜色
                ctx.fillStyle = v ? "#ffffff" : "#888888"
                // 等宽字体
                ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                // 居中
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                // 画 0/1
                ctx.fillText(v ? "1" : "0", gx + gw / 2, gy + gh / 2)
            }
        // 非二进制：整串格式化后居中显示
        } else {
            // 取格式化字符串
            var s = circuit.getInputAsString(comp.id)
            // 文字颜色
            ctx.fillStyle = "#dddddd"
            // 等宽字体
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制字符串
            ctx.fillText(s, sp.x + w / 2, sp.y + h / 2)
        }
    // 输出端口：按显示进制画值
    
        return false
    }
}
