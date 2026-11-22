import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 与门的共享界面定义：注册时创建，绘制时读取各实例的 comp 快照。
// multiInput、gateDelay 声明属性面板能力；输入数决定继承的尺寸高度。
// 绘制 D 形外壳及标签，逻辑计算由电路模型负责；返回 false 后继续画通用端口与名称。
ComponentDefinition {
    typeId: "and"
    displayName: "与门"
    category: "基本逻辑门"
    categoryOrder: 1
    libraryOrder: 0
    multiInput: true
    gateDelay: true
    function label(comp, circuit) { return "AND" }
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
    // 外壳已绘制，简化模式跳过中心标签。
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
