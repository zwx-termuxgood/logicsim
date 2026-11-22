import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "led"
    displayName: "LED"
    category: "输入输出"
    categoryOrder: 0
    libraryOrder: 3
    displayBase: true
    propertyEditor: Component { LedProps {} }
    function size(comp) { return { w: Math.max(50, (comp.bitWidth || 1) * 22 + 8), h: 40 } }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 底色
        ctx.fillStyle = "#2a2a2a"
        // 开始路径
        ctx.beginPath()
        // 圆，半径取宽高较小者一半
        ctx.arc(sp.x + w / 2, sp.y + h / 2, Math.min(w, h) / 2, 0, Math.PI * 2)
        // 填充
        ctx.fill()
        // 描边色
        ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 描边
        ctx.stroke()
    // 输入开关：圆角矩形
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        // LED 输入值字符串
        var lvStr = ""
        // 有输入端口值则取第一个
        if (comp.inputPortValues && comp.inputPortValues.length > 0)
            lvStr = comp.inputPortValues[0] || ""
        // LED 颜色，缺省金色
        var ledColor = comp.color || "#ffd700"
        // 非二进制：整串格式化后居中显示
        if (base !== "bin") {
            // 格式化 LED 值
            var ldStr = circuit.formatValue(lvStr, base, bw)
            // 文字用 LED 颜色
            ctx.fillStyle = ledColor
            // 等宽字体
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制字符串
            ctx.fillText(ldStr, sp.x + w / 2, sp.y + h / 2)
        // 二进制：逐位画圆点
        } else {
            // 逐位绘制
            for (var i3 = 0; i3 < bw; ++i3) {
                // 圆点中心 x
                var cx3 = sp.x + (14 + i3 * 22) * viewScale
                // 圆点中心 y
                var cy3 = sp.y + h / 2
                // 该位字符，缺失按 '0'
                var ch3 = (i3 < lvStr.length) ? lvStr.charAt(i3) : '0'
                // 按位状态选填充色
                if (ch3 === '1')      ctx.fillStyle = ledColor
                else if (ch3 === 'Z') ctx.fillStyle = "#ffb74d"
                else if (ch3 === 'E') ctx.fillStyle = "#e05252"
                else                  ctx.fillStyle = "#2a2a2a"
                // 开始路径
                ctx.beginPath(); ctx.arc(cx3, cy3, 8 * viewScale, 0, Math.PI * 2); ctx.fill()
                // 按位状态选描边色
                ctx.strokeStyle = (ch3 === '1') ? ledColor
                                : (ch3 === 'E') ? "#e05252"
                                : (ch3 === 'Z') ? "#ffb74d"
                                : "#444444"
                // 描边线宽
                ctx.lineWidth = 1.5 * viewScale
                // 描边
                ctx.stroke()
            }
        }
    // 其它元件：画类型标签
    
        return false
    }
}
