import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

ComponentDefinition {
    typeId: "clock"
    displayName: "时钟"
    category: "控制"
    categoryOrder: 5
    libraryOrder: 0
    function size(comp) { return { w: 60, h: 40 } }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充色
        ctx.fillStyle = "#2a3a2a"
        // 描边色：选中白，否则绿
        ctx.strokeStyle = isSel ? "#ffffff" : "#4caf50"
        // 线宽：选中加粗
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        // 非简化模式画当前值
        if (!simplify) {
            // 当前输出是否为 1
            var cval = (comp.outputPorts && comp.outputPorts[0] === "1")
            // 高电平绿，低电平灰
            ctx.fillStyle = cval ? "#4caf50" : "#555555"
            // 等宽字体
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px monospace"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制 0/1
            ctx.fillText(cval ? "1" : "0", sp.x + w / 2, sp.y + h / 2 - 4 * viewScale)
        }

        return true
    }
}
