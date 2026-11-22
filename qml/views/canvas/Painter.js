.pragma library
// 声明为共享 JS 库：供多个 QML 文件 import 使用，不绑定到某个 QML 实例
.import "Geometry.js" as Geometry
// 引入几何工具库：坐标换算与圆角矩形绘制都依赖它

// ============================================================
// 背景网格
// ============================================================
// 定义"绘制背景网格"函数
function paintGrid(ctx, W, H, viewScale, viewX, viewY) {
    // 网格步长（世界 40px 换算到屏幕）
    var step = 40 * viewScale
    // 步长过小（网格过密）则不画，避免糊成一片
    if (step < 8) return
    // 网格起始偏移：视图偏移对步长取余，保证网格随画布平移
    var ox = viewX % step, oy = viewY % step
    // 网格线颜色与线宽
    ctx.strokeStyle = "#252525"; ctx.lineWidth = 1
    // 开始一条包含所有网格线的大路径，一次 stroke 提高性能
    ctx.beginPath()
    // 逐条竖线
    for (var x = ox; x < W; x += step) { ctx.moveTo(x, 0); ctx.lineTo(x, H) }
    // 逐条横线
    for (var y = oy; y < H; y += step) { ctx.moveTo(0, y); ctx.lineTo(W, y) }
    // 一次性描边
    ctx.stroke()
}

// 定义"根据信号字符串选择连线样式"函数
function pickWireStyle(srcStr) {
    // 是否含错误态 E
    var hasE = srcStr.indexOf('E') !== -1
    // 是否含高阻态 Z（有 E 时不看 Z）
    var hasZ = !hasE && srcStr.indexOf('Z') !== -1
    // 是否含高电平 1（有 E/Z 时不看 1）
    var has1 = !hasE && !hasZ && srcStr.indexOf('1') !== -1
    // 错误态：红色虚线
    if (hasE) return { color: "#e05252", dash: [3, 3] }
    // 高阻态：橙色长虚线
    if (hasZ) return { color: "#ffb74d", dash: [6, 5] }
    // 高电平：绿色实线
    if (has1) return { color: "#4caf50", dash: [] }
    // 全 0：灰色实线
    return { color: "#666666", dash: [] }
}

// 定义"根据信号字符串设置上下文描边样式"函数
function pickWireColor(ctx, srcStr) {
    // 取样式
    var s = pickWireStyle(srcStr)
    // 设置描边颜色
    ctx.strokeStyle = s.color
    // 设置虚线模式
    ctx.setLineDash(s.dash)
}

// ============================================================
// 连线绘制分派：使用模型几何与预计算线源，不改变连接关系
//   wireSources: { wireId -> "0110" } 由 CanvasView 预先计算
// ============================================================
// 定义"绘制连线"分派函数：按线型转调具体绘制函数
function drawWire(ctx, w, compById, outPortsById, inPortsById,
                  viewScale, viewX, viewY, W, H, simplify, selectedWireId, wireSources, selectedIds) {
    // 取线型，缺省回退 bezier
    var wt = w.wireType || "bezier"
    // 是否被选中
    var isSel = (w.id === selectedWireId || (selectedIds && selectedIds.indexOf(w.id) >= 0))

    // ortho 线走正交绘制
    if (wt === "ortho") {
        // 绘制正交折线
        drawWireOrtho(ctx, w, compById, viewScale, viewX, viewY, W, H, simplify,
                      isSel, wireSources)
    } else {
        // 其余走贝塞尔绘制
        drawWireBezier(ctx, w, compById, outPortsById, inPortsById,
                       viewScale, viewX, viewY, W, H, simplify, isSel, wireSources)
    }
}

// ============================================================
// 贝塞尔连线
// ============================================================
// 定义"绘制贝塞尔连线"函数
function drawWireBezier(ctx, w, compById, outPortsById, inPortsById,
                        viewScale, viewX, viewY, W, H, simplify, isSel, wireSources) {
    // 起点元件
    var fromC = compById[w.fromComp]
    // 终点元件
    var toC = compById[w.toComp]
    // 任一端元件不存在则不画
    if (!fromC || !toC) return

    // 起点元件的输出端口几何
    var pOuts = outPortsById[w.fromComp] || []
    // 终点元件的输入端口几何
    var pIns = inPortsById[w.toComp] || []
    // 起点端口索引越界则不画
    if (w.fromPort < 0 || w.fromPort >= pOuts.length) return
    // 终点端口索引越界则不画
    if (w.toPort < 0 || w.toPort >= pIns.length) return

    // 起点世界坐标
    var p1 = { x: fromC.x + pOuts[w.fromPort].x, y: fromC.y + pOuts[w.fromPort].y }
    // 终点世界坐标
    var p2 = { x: toC.x + pIns[w.toPort].x, y: toC.y + pIns[w.toPort].y }
    // 起点屏幕坐标
    var s1 = Geometry.worldToScreen(viewScale, viewX, viewY, p1.x, p1.y)
    // 终点屏幕坐标
    var s2 = Geometry.worldToScreen(viewScale, viewX, viewY, p2.x, p2.y)

    // 两端都在画布左侧外部则不画
    if ((s1.x < -100 && s2.x < -100) || (s1.x > W + 100 && s2.x > W + 100)) return
    // 两端都在画布上下外部则不画
    if ((s1.y < -100 && s2.y < -100) || (s1.y > H + 100 && s2.y > H + 100)) return

    // 贝塞尔控制点水平偏移量，至少 40*缩放
    var cx = Math.max(Math.abs(s2.x - s1.x) / 2, 40 * viewScale)

    // 信号字符串，默认 "0"
    var srcStr = "0"
    // 优先用预先计算的 wireSources
    if (wireSources && wireSources[w.id] !== undefined) {
        srcStr = wireSources[w.id]
    // 否则回退读起点元件输出端口值
    } else if (fromC.outputPorts && fromC.outputPorts.length > w.fromPort) {
        // 取起点输出端口值
        var sv = fromC.outputPorts[w.fromPort]
        // 有效则采用
        if (sv && sv.length > 0) srcStr = sv
    }

    // 开始路径
    ctx.beginPath()
    // 选中态：蓝色高亮实线
    if (isSel) {
        // 选中颜色
        ctx.strokeStyle = "#58a6ff"
        // 线宽加粗
        ctx.lineWidth = 4
        // 实线
        ctx.setLineDash([])
    } else {
        // 按信号选样式
        var pc = pickWireStyle(srcStr)
        // 描边颜色
        ctx.strokeStyle = pc.color
        // 虚线模式
        ctx.setLineDash(pc.dash)
        // 线宽：简化模式 1，否则随缩放
        ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
    }
    // 移到起点
    ctx.moveTo(s1.x, s1.y)
    // 三次贝塞尔曲线到终点
    ctx.bezierCurveTo(s1.x + cx, s1.y, s2.x - cx, s2.y, s2.x, s2.y)
    // 描边
    ctx.stroke()
    // 复位虚线
    ctx.setLineDash([])
}

// ============================================================
// 正交连线（path 折线）
//   ★ 修复：srcStr 优先用 wireSources[w.id]（递归追上游）
// ============================================================
// 定义"绘制正交折线"函数
function drawWireOrtho(ctx, w, compById, viewScale, viewX, viewY, W, H, simplify,
                       isSel, wireSources) {
    // 无效路径不画
    if (!w.path || w.path.length < 2) return

    // 是否有顶点落在可见区域内
    var vis = false
    // 遍历所有顶点
    for (var i = 0; i < w.path.length; i++) {
        // 顶点
        var p = w.path[i]
        // 顶点屏幕坐标
        var sp = Geometry.worldToScreen(viewScale, viewX, viewY, p.x, p.y)
        // 落在画布（外扩 50px）内即可见
        if (sp.x >= -50 && sp.x <= W + 50 && sp.y >= -50 && sp.y <= H + 50) {
            vis = true
            break
        }
    }
    // 不可见则不画
    if (!vis) return

    // 信号字符串，默认 "0"
    var srcStr = "0"
    // ★ 优先用 wireSources
    if (wireSources && wireSources[w.id] !== undefined) {
        srcStr = wireSources[w.id]
    // 否则回退读起点元件输出端口值
    } else if (w.fromComp && compById[w.fromComp]) {
        // 起点元件
        var fromC = compById[w.fromComp]
        // 起点端口索引
        var fromPort = w.fromPort || 0
        // 端口索引有效则取值
        if (fromC.outputPorts && fromC.outputPorts.length > fromPort) {
            var sv = fromC.outputPorts[fromPort]
            if (sv && sv.length > 0) srcStr = sv
        }
    }

    // 开始路径
    ctx.beginPath()
    // 遍历所有顶点连成折线
    for (var j = 0; j < w.path.length; j++) {
        // 顶点
        var pt = w.path[j]
        // 顶点屏幕坐标
        var spt = Geometry.worldToScreen(viewScale, viewX, viewY, pt.x, pt.y)
        // 首顶点移到，其余连线
        if (j === 0) ctx.moveTo(spt.x, spt.y)
        else         ctx.lineTo(spt.x, spt.y)
    }

    // 选中态：蓝色高亮实线
    if (isSel) {
        // 选中颜色
        ctx.strokeStyle = "#58a6ff"
        // 线宽加粗
        ctx.lineWidth = 4
        // 实线
        ctx.setLineDash([])
    } else {
        // 按信号选样式
        var pc = pickWireStyle(srcStr)
        // 描边颜色
        ctx.strokeStyle = pc.color
        // 虚线模式
        ctx.setLineDash(pc.dash)
        // 线宽：简化模式 1，否则随缩放
        ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
    }
    // 描边
    ctx.stroke()
    // 复位虚线
    ctx.setLineDash([])
}

// ============================================================
// 连接圆点
// ============================================================
// 定义"绘制连接圆点"函数
function drawConnectionDots(ctx, dots, viewScale, viewX, viewY) {
    // 遍历每个圆点
    for (var i = 0; i < dots.length; i++) {
        // 圆点
        var d = dots[i]
        // 圆点屏幕坐标
        var sp = Geometry.worldToScreen(viewScale, viewX, viewY, d.x, d.y)
        // 圆点颜色
        ctx.fillStyle = "#999999"
        // 开始路径
        ctx.beginPath()
        // 圆，半径随缩放
        ctx.arc(sp.x, sp.y, 4 * viewScale, 0, Math.PI * 2)
        // 填充
        ctx.fill()
    }
}

// ============================================================
// 连线预览
// ============================================================
// 定义"绘制贝塞尔拉线预览"函数
function drawPendingBezier(ctx, startScreen, wireEndX, wireEndY, viewScale) {
    // 预览颜色
    ctx.strokeStyle = "#ffb74d"
    // 预览线宽
    ctx.lineWidth = 2
    // 预览虚线
    ctx.setLineDash([6, 6])
    // 开始路径
    ctx.beginPath()
    // 控制点偏移
    var cx = Math.max(Math.abs(wireEndX - startScreen.x) / 2, 40 * viewScale)
    // 移到起点
    ctx.moveTo(startScreen.x, startScreen.y)
    // 三次贝塞尔到终点
    ctx.bezierCurveTo(startScreen.x + cx, startScreen.y,
                      wireEndX - cx, wireEndY,
                      wireEndX, wireEndY)
    // 描边
    ctx.stroke()
    // 复位虚线
    ctx.setLineDash([])
}

// 定义"绘制正交拉线预览"函数
function drawPendingOrtho(ctx, startScreen, direction, wireEndX, wireEndY) {
    // 预览颜色
    ctx.strokeStyle = "#ffb74d"
    // 预览线宽
    ctx.lineWidth = 2
    // 预览虚线
    ctx.setLineDash([6, 6])
    // 开始路径
    ctx.beginPath()
    // 横优先：先横后竖
    if (direction === 1) {
        // 移到起点
        ctx.moveTo(startScreen.x, startScreen.y)
        // 水平到终点 x
        ctx.lineTo(wireEndX, startScreen.y)
        // 竖直到终点
        ctx.lineTo(wireEndX, wireEndY)
    // 竖优先：先竖后横
    } else if (direction === 2) {
        // 移到起点
        ctx.moveTo(startScreen.x, startScreen.y)
        // 竖直到终点 y
        ctx.lineTo(startScreen.x, wireEndY)
        // 水平到终点
        ctx.lineTo(wireEndX, wireEndY)
    // 方向未定：直接斜线
    } else {
        // 移到起点
        ctx.moveTo(startScreen.x, startScreen.y)
        // 直线到终点
        ctx.lineTo(wireEndX, wireEndY)
    }
    // 描边
    ctx.stroke()
    // 复位虚线
    ctx.setLineDash([])
}

// 定义"绘制拉线预览"兼容函数：转调正交预览
function drawPendingWire(ctx, startScreen, direction, wireEndX, wireEndY) {
    // 统一按正交预览绘制
    drawPendingOrtho(ctx, startScreen, direction, wireEndX, wireEndY)
}

// ============================================================
// 端口颜色
// ============================================================
// 定义"按信号字符串分类端口状态"函数：返回 0 低/1 高/2 高阻/3 错误
function classifyPortStr(s) {
    // 空串视为低
    if (!s || s.length === 0) return 0
    // 含 E 为错误
    if (s.indexOf('E') !== -1) return 3
    // 含 Z 为高阻
    if (s.indexOf('Z') !== -1) return 2
    // 含 1 为高电平
    if (s.indexOf('1') !== -1) return 1
    // 否则低电平
    return 0
}

// 定义"按端口状态设置填充色"函数
function fillPortColor(ctx, kind) {
    // 错误：红
    if (kind === 3)      ctx.fillStyle = "#e05252"
    // 高阻：橙
    else if (kind === 2) ctx.fillStyle = "#ffb74d"
    // 高电平：绿
    else if (kind === 1) ctx.fillStyle = "#4caf50"
    // 低电平：灰
    else                 ctx.fillStyle = "#999999"
}

// ============================================================
// 元件绘制：定义负责类型外观，公共流程负责旋转、端口和自定义名称
// ============================================================
// 定义"绘制单个元件"函数
// comp 为模型快照，compSize 返回世界尺寸；缩放和平移转换为屏幕位置后交给类型定义。
// 两类端口表保存已考虑旋转的相对几何；端口在撤销外壳旋转之后按这些偏移绘制。
// 函数无返回值，save/restore 限定本次上下文修改；定义返回 true 或 simplify 时提前结束。
function drawComponent(ctx, comp, compSize, viewScale, viewX, viewY,
                       outPortsById, inPortsById, selectedCompId, selectedIds,
                       simplify, circuit, registry) {
    // 元件类型
    var t = comp.type
    // 元件尺寸
    var size = compSize(comp)
    // 元件左上角屏幕坐标
    var sp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                    comp.x - size.w / 2, comp.y - size.h / 2)
    // 屏幕宽
    var w = size.w * viewScale
    // 屏幕高
    var h = size.h * viewScale
    // 是否单选
    var isSel = (comp.id === selectedCompId)
    // 是否在多选集合内
    var isMultiSel = (selectedIds.indexOf(comp.id) >= 0)

    // 旋转角度（度）
    var rotDeg = (comp.rotation || 0) * 90
    // 旋转角度（弧度）
    var rotRad = rotDeg * Math.PI / 180
    // 元件中心屏幕 x
    var cxs = sp.x + w / 2
    // 元件中心屏幕 y
    var cys = sp.y + h / 2

    // 保存上下文状态
    ctx.save()

    // 多选：画虚线框
    if (isMultiSel) {
        // 虚线颜色
        ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 2
        // 虚线模式
        ctx.setLineDash([5, 3])
        // 比包围盒略大的虚线框
        ctx.strokeRect(sp.x - 3, sp.y - 3, w + 6, h + 6)
        // 复位虚线
        ctx.setLineDash([])
    }

    // 有旋转则绕中心旋转上下文
    if (rotRad !== 0) {
        // 移到中心
        ctx.translate(cxs, cys); ctx.rotate(rotRad); ctx.translate(-cxs, -cys)
    }

    var definition = registry.get(comp.type)
    var complete = false
    if (definition) {
        complete = definition.paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit)
    } else {

        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()

    }
    if (complete || simplify) {
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        ctx.restore()
        return
    }

    // 撤销旋转，回到未旋转坐标系画端口
    if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }

    // 取输入端口几何
    var ins = inPortsById[comp.id] || []
    // 逐个输入端口绘制
    for (var k = 0; k < ins.length; k++) {
        // 端口相对偏移
        var pIn = ins[k]
        // 端口屏幕坐标
        var psp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                          comp.x + pIn.x, comp.y + pIn.y)
        // 该端口输入值字符串
        var pvStr = ""
        // 有输入端口值则取对应项
        if (comp.inputPortValues && comp.inputPortValues.length > k)
            pvStr = comp.inputPortValues[k] || ""
        // 按端口状态设置填充色
        fillPortColor(ctx, classifyPortStr(pvStr))
        // 画端口圆点
        ctx.beginPath(); ctx.arc(psp.x, psp.y, 6 * viewScale, 0, Math.PI * 2); ctx.fill()

        if (definition) definition.paintInputLabel(ctx, k, psp, viewScale)

    }

    // 取输出端口几何
    var outs = outPortsById[comp.id] || []
    // 逐个输出端口绘制
    for (var k2 = 0; k2 < outs.length; k2++) {
        // 端口相对偏移
        var pOut = outs[k2]
        // 端口屏幕坐标
        var psp2 = Geometry.worldToScreen(viewScale, viewX, viewY,
                                           comp.x + pOut.x, comp.y + pOut.y)
        // 该端口输出值字符串
        var ovStr2 = ""
        // 有输出端口值则取对应项
        if (comp.outputPorts && comp.outputPorts.length > k2)
            ovStr2 = comp.outputPorts[k2] || ""
        // 按端口状态设置填充色
        fillPortColor(ctx, classifyPortStr(ovStr2))
        // 画端口圆点
        ctx.beginPath(); ctx.arc(psp2.x, psp2.y, 6 * viewScale, 0, Math.PI * 2); ctx.fill()

        if (definition) definition.paintOutputLabel(ctx, k2, psp2, viewScale)

    }

    if (definition) definition.paintPortNames(ctx, comp, ins, outs, viewScale, viewX, viewY)

    // 非子电路/非文本且有自定义名称：在元件上方画名称
    if (comp.name && comp.name.length > 0 && (!definition || definition.showName)) {
        // 名称颜色
        ctx.fillStyle = "#cccccc"
        // 名称字体
        ctx.font = Math.max(8, Math.round(10 * viewScale)) + "px sans-serif"
        // 居中，底部对齐
        ctx.textAlign = "center"; ctx.textBaseline = "bottom"
        // 画在元件上方
        ctx.fillText(comp.name, sp.x + w / 2, sp.y - 4 * viewScale)
    }

    // 恢复上下文
    ctx.restore()
}
