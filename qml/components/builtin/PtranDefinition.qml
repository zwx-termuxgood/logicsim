import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "ptran"
    displayName: "PMOS 管"
    category: "传输门 / 晶体管"
    categoryOrder: 3
    libraryOrder: 2
    gateDelay: true
    labelYOffset: 14
    portDescription: "端口：G(左) / D(右上) / S(右下)\nG=0 时导通 S=D；G=1 时输出高阻 Z"
    function size(comp) { return { w: 80, h: 60 } }
    function paintInputLabel(ctx, k, psp, viewScale) { var t = "ptran"; var simplify = false
        // 晶体管端口标注：G/S
        if (!simplify && (t === "ntran" || t === "ptran")) {
            // 标注颜色
            ctx.fillStyle = "#b388ff"
            // 标注字体
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            // 栅极标注在左侧
            if (k === 0) { ctx.textAlign = "right"; ctx.textBaseline = "middle"; ctx.fillText("G", psp.x - 10 * viewScale, psp.y) }
            // 源极标注在右侧
            else if (k === 1) { ctx.textAlign = "left"; ctx.textBaseline = "middle"; ctx.fillText("S", psp.x + 10 * viewScale, psp.y) }
        // 传输门端口标注：D/EN//EN
        } else if (!simplify && t === "tgate") {
            // 标注颜色
            ctx.fillStyle = "#b388ff"
            // 标注字体
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            // 标注在左侧
            ctx.textAlign = "right"; ctx.textBaseline = "middle"
            // 数据端
            if (k === 0) ctx.fillText("D", psp.x - 10 * viewScale, psp.y)
            // 使能端
            else if (k === 1) ctx.fillText("EN", psp.x - 10 * viewScale, psp.y)
            // 反使能端
            else if (k === 2) ctx.fillText("/EN", psp.x - 10 * viewScale, psp.y)
        }
    }
    function paintOutputLabel(ctx, k2, psp2, viewScale) { var t = "ptran"; var simplify = false
        // 晶体管输出端口标注：D
        if (!simplify && (t === "ntran" || t === "ptran")) {
            // 标注颜色
            ctx.fillStyle = "#b388ff"
            // 标注字体
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            // 漏极标注在右侧
            ctx.textAlign = "left"; ctx.textBaseline = "middle"
            // 绘制 D
            ctx.fillText("D", psp2.x + 10 * viewScale, psp2.y)
        // 传输门输出端口标注：S
        } else if (!simplify && t === "tgate") {
            // 标注颜色
            ctx.fillStyle = "#b388ff"
            // 标注字体
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            // 标注在右侧
            ctx.textAlign = "left"; ctx.textBaseline = "middle"
            // 绘制 S
            ctx.fillText("S", psp2.x + 10 * viewScale, psp2.y)
        }
    }
    function label(comp, circuit) { return "PMOS" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充色
        ctx.fillStyle = "#1f1a2e"
        // 描边色
        ctx.strokeStyle = isSel ? "#ffffff" : "#9c27b0"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        // 非简化模式才画符号
        if (!simplify) {
            // 中心 x
            var cx2 = sp.x + w * 0.5
            // 中心 y
            var cy2 = sp.y + h / 2
            // 符号颜色
            ctx.strokeStyle = isSel ? "#ffffff" : "#b388ff"
            // 符号线宽
            ctx.lineWidth = Math.max(1.5, 2 * viewScale)
            // 竖直沟道线
            ctx.beginPath(); ctx.moveTo(cx2, sp.y + h * 0.2); ctx.lineTo(cx2, sp.y + h * 0.8); ctx.stroke()
            // 左侧栅极线
            ctx.beginPath(); ctx.moveTo(cx2 - 8 * viewScale, sp.y + h * 0.2); ctx.lineTo(cx2 - 8 * viewScale, sp.y + h * 0.8); ctx.stroke()
            // 栅极引线
            ctx.beginPath(); ctx.moveTo(cx2 - 8 * viewScale, sp.y + h * 0.5); ctx.lineTo(sp.x + 4 * viewScale, sp.y + h * 0.5); ctx.stroke()
            // 开始路径
            ctx.beginPath()
            // 上漏极引线
            ctx.moveTo(cx2, sp.y + h * 0.25); ctx.lineTo(sp.x + w, sp.y + h * 0.25)
            // 下源极引线
            ctx.moveTo(cx2, sp.y + h * 0.75); ctx.lineTo(sp.x + w, sp.y + h * 0.75)
            // 描边
            ctx.stroke()
            // PMOS：额外画圆圈表示
            if (t === "ptran") {
                // 开始路径
                ctx.beginPath()
                // 栅极小圆
                ctx.arc(cx2 - 8 * viewScale - 4 * viewScale, cy2, 4 * viewScale, 0, Math.PI * 2)
                // 描边
                ctx.stroke()
            }
        }
    // 传输门：圆角矩形 + CMOS 符号
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
