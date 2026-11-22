import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 与非门的共享界面定义：注册后复用，实例状态由绘制参数 comp 提供。
// 支持输入数量和门延迟编辑；继承按输入数增高的尺寸，输出小圆表示取反。
// 绘制只负责外观，返回 false 交回通用端口与名称绘制流程。
ComponentDefinition {
    typeId: "nand"
    displayName: "与非"
    category: "复合逻辑门"
    categoryOrder: 2
    libraryOrder: 0
    multiInput: true
    gateDelay: true
    function label(comp, circuit) { return "NAND" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 开始路径
        ctx.beginPath()
        // 半圆半径
        var r = h / 2
        // 左上到上边中点
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x + w / 2, sp.y)
        // 右半圆
        ctx.arc(sp.x + w / 2, sp.y + r, r, -Math.PI / 2, Math.PI / 2, false)
        // 回到左下并闭合
        ctx.lineTo(sp.x, sp.y + h); ctx.closePath(); ctx.fill(); ctx.stroke()
        // 与非门：额外画输出小圆
        if (t === "nand") {
            // 开始路径
            ctx.beginPath()
            // 输出小圆
            ctx.arc(sp.x + w + 5 * viewScale, sp.y + h / 2, 5 * viewScale, 0, Math.PI * 2)
            // 填充与描边
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    // 与非符号已绘制，简化模式跳过中心标签。
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
