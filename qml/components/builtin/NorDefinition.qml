import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 或非门的共享界面定义：注册时创建，按传入的 comp 快照绘制不同实例。
// 支持输入数与门延迟编辑；弧形外壳右侧小圆表示输出取反。
// 尺寸沿用基类规则，绘制返回 false 后由公共流程补充端口和名称。
ComponentDefinition {
    typeId: "nor"
    displayName: "或非"
    category: "复合逻辑门"
    categoryOrder: 2
    libraryOrder: 1
    multiInput: true
    gateDelay: true
    function label(comp, circuit) { return "NOR" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 开始路径
        ctx.beginPath()
        // 左侧起点
        ctx.moveTo(sp.x, sp.y)
        // 上弧到右中
        ctx.quadraticCurveTo(sp.x + w * 0.6, sp.y, sp.x + w, sp.y + h / 2)
        // 下弧回到左侧
        ctx.quadraticCurveTo(sp.x + w * 0.6, sp.y + h, sp.x, sp.y + h)
        // 内弧闭合
        ctx.quadraticCurveTo(sp.x + w * 0.3, sp.y + h / 2, sp.x, sp.y)
        // 填充与描边
        ctx.closePath(); ctx.fill(); ctx.stroke()
        // 异或/同或：额外左侧弧线
        if (t === "xor" || t === "xnor") {
            // 开始路径
            ctx.beginPath()
            // 移到左侧弧线上端
            ctx.moveTo(sp.x - 6 * viewScale, sp.y)
            // 画弧到下端
            ctx.quadraticCurveTo(sp.x - 6 * viewScale + w * 0.3, sp.y + h / 2,
                                  sp.x - 6 * viewScale, sp.y + h)
            // 描边
            ctx.stroke()
        }
        // 或非/同或：额外画输出小圆
        if (t === "nor" || t === "xnor") {
            // 开始路径
            ctx.beginPath()
            // 输出小圆
            ctx.arc(sp.x + w + 5 * viewScale, sp.y + h / 2, 5 * viewScale, 0, Math.PI * 2)
            // 填充与描边
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    // 或非符号已绘制，简化模式跳过类型标签。
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
