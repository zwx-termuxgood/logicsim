import QtQuick
import ".."
import "../../views/panels/props"
import "../../views/canvas/Geometry.js" as Geometry

// 子电路实例的共享界面定义：注册后复用；category 为空，由专用子电路面板提供实例入口。
// comp.subId 关联模型中的定义，接口名称数组决定高度；这里不展开或执行子电路。
// label 组合实例名与模型子电路名，showName=false 避免上方再画一遍名称。
// paint 返回 false 继续绘制端口；paintPortNames 用模型偏移与世界到屏幕换算标注接口名。
ComponentDefinition {
    typeId: "sub"
    displayName: "子电路"
    category: ""
    categoryOrder: 6
    libraryOrder: 0
    showName: false
    function size(comp) { return { w: 110, h: Math.max(60, Math.max((comp.subInputNames || []).length, (comp.subOutputNames || []).length) * 24) } }
    function label(comp, circuit) { var label = ""
        {
            // 子电路显示名
            var subName = ""
            // 有接口则查询
            if (circuit && circuit.getSubcircuitName)
                subName = circuit.getSubcircuitName(comp.subId) || ""
            // 自定义名与子电路名不同则都显示
            if (comp.name && comp.name.length > 0 && comp.name !== subName)
                label = comp.name + " [" + subName + "]"
            // 否则只显示一个
            else
                label = subName || comp.name || "SUB"
        }

        return label
    }
    function paintPortNames(ctx, comp, ins, outs, viewScale, viewX, viewY) {
        var t = typeId

    if (t === "sub") {
        // 输入端口名列表
        var subIns = comp.subInputNames || []
        // 输出端口名列表
        var subOuts = comp.subOutputNames || []
        // 端口名字体
        ctx.font = Math.max(7, Math.round(9 * viewScale)) + "px sans-serif"
        // 端口名颜色
        ctx.fillStyle = "#a371f7"
        // 输入端口名画在端口左侧
        ctx.textAlign = "right"; ctx.textBaseline = "middle"
        // 逐个输入端口
        for (var a = 0; a < ins.length && a < subIns.length; ++a) {
            // 端口屏幕坐标
            var ppA = Geometry.worldToScreen(viewScale, viewX, viewY,
                                              comp.x + ins[a].x, comp.y + ins[a].y)
            // 绘制端口名
            ctx.fillText(subIns[a], ppA.x - 10 * viewScale, ppA.y)
        }
        // 输出端口名画在端口右侧
        ctx.textAlign = "left"
        // 逐个输出端口
        for (var b = 0; b < outs.length && b < subOuts.length; ++b) {
            // 端口屏幕坐标
            var ppB = Geometry.worldToScreen(viewScale, viewX, viewY,
                                              comp.x + outs[b].x, comp.y + outs[b].y)
            // 绘制端口名
            ctx.fillText(subOuts[b], ppB.x + 10 * viewScale, ppB.y)
        }
    }


    }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) {
        var t = typeId

        // 填充与描边色
        ctx.fillStyle = "#1a2433"; ctx.strokeStyle = isSel ? "#ffffff" : "#a371f7"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 6 * viewScale); ctx.fill(); ctx.stroke()
    // 子电路实例外壳已绘制，下面绘制组合后的显示名称。
    
        if (simplify) return false
        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        var text = label(comp, circuit)
        if (text.length > 0) { ctx.fillStyle = "#e0e0e0"; ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(text, sp.x + w / 2, sp.y + h / 2 + labelYOffset * viewScale) }

        return false
    }
}
