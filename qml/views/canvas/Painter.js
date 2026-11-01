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
// drawWire 分派（13 参数：新增 wireSources）
//   wireSources: { wireId -> "0110" } 由 CanvasView 预先计算
// ============================================================
// 定义"绘制连线"分派函数：按线型转调具体绘制函数
function drawWire(ctx, w, compById, outPortsById, inPortsById,
                  viewScale, viewX, viewY, W, H, simplify, selectedWireId, wireSources) {
    // 取线型，缺省回退 bezier
    var wt = w.wireType || "bezier"
    // 是否被选中
    var isSel = (w.id === selectedWireId)

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
// 元件绘制（与上一版一致，无改动）
// ============================================================
// 定义"绘制单个元件"函数
function drawComponent(ctx, comp, compSize, viewScale, viewX, viewY,
                       outPortsById, inPortsById, selectedCompId, selectedIds,
                       simplify, circuit) {
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

    // 文本元件：只画文字与选中框
    if (t === "text") {
        // 非简化模式才画文字
        if (!simplify) {
            // 文字颜色
            ctx.fillStyle = "#dddddd"
            // 字体
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px sans-serif"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 绘制内容
            ctx.fillText(comp.content || "文本", sp.x + w / 2, sp.y + h / 2)
        }
        // 选中：虚线框
        if (isSel) {
            // 选中颜色
            ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 1.5
            // 虚线模式
            ctx.setLineDash([4, 3])
            // 绘制选中框
            ctx.strokeRect(sp.x, sp.y, w, h)
            // 复位虚线
            ctx.setLineDash([])
        }
        // 撤销旋转
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        // 恢复上下文
        ctx.restore()
        // 文本元件绘制结束
        return
    }

    // 时钟元件：圆角矩形 + 当前值
    if (t === "clock") {
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
        // 撤销旋转
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        // 恢复上下文
        ctx.restore()
        // 时钟元件绘制结束
        return
    }

    // 常量元件：圆角矩形
    if (t === "const") {
        // 填充色
        ctx.fillStyle = "#2a2a3a"; ctx.strokeStyle = isSel ? "#ffffff" : "#7c8cff"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // LED：圆形
    } else if (t === "led") {
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
    } else if (t === "input") {
        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#666666"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // 与门/与非门：D 形
    } else if (t === "and" || t === "nand") {
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
    // 或/或非/异或/同或门：弧形
    } else if (t === "or" || t === "nor" || t === "xor" || t === "xnor") {
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
    // 非门：三角形 + 输出小圆
    } else if (t === "not") {
        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 开始路径
        ctx.beginPath()
        // 三角形左边上下
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x, sp.y + h)
        // 右边尖点
        ctx.lineTo(sp.x + w * 0.85, sp.y + h / 2); ctx.closePath(); ctx.fill(); ctx.stroke()
        // 开始路径
        ctx.beginPath()
        // 输出小圆
        ctx.arc(sp.x + w * 0.85 + 6 * viewScale, sp.y + h / 2, 6 * viewScale, 0, Math.PI * 2)
        // 填充与描边
        ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
    // 分线器/集线器：圆角矩形
    } else if (t === "splitter" || t === "hub") {
        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // 输出端口：圆角矩形
    } else if (t === "output") {
        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#ff9800"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    // 子电路实例：圆角矩形
    } else if (t === "sub") {
        // 填充与描边色
        ctx.fillStyle = "#1a2433"; ctx.strokeStyle = isSel ? "#ffffff" : "#a371f7"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 6 * viewScale); ctx.fill(); ctx.stroke()
    // NMOS/PMOS：圆角矩形 + 晶体管符号
    } else if (t === "ntran" || t === "ptran") {
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
    } else if (t === "tgate") {
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
            // 沟道左界
            var chanL = sp.x + w * 0.32
            // 沟道右界
            var chanR = sp.x + w * 0.68
            // PMOS 行 y
            var pY = sp.y + h * 0.30
            // NMOS 行 y
            var nY = sp.y + h * 0.70
            // 栅极 x
            var gateX = sp.x + w * 0.24
            // 栅极半高
            var gateHalf = h * 0.10
            // 小圆点半径
            var dotR = 2.2 * viewScale
            // 符号颜色
            ctx.strokeStyle = isSel ? "#ffffff" : "#b388ff"
            // 符号线宽
            ctx.lineWidth = Math.max(1.2, 1.6 * viewScale)
            // 左沟道竖线
            ctx.beginPath(); ctx.moveTo(chanL, pY); ctx.lineTo(chanL, nY); ctx.stroke()
            // 右沟道竖线
            ctx.beginPath(); ctx.moveTo(chanR, pY); ctx.lineTo(chanR, nY); ctx.stroke()
            // 上横线
            ctx.beginPath(); ctx.moveTo(chanL, pY); ctx.lineTo(chanR, pY); ctx.stroke()
            // 上栅极竖线
            ctx.beginPath(); ctx.moveTo(gateX, pY - gateHalf); ctx.lineTo(gateX, pY + gateHalf); ctx.stroke()
            // PMOS 栅极小圆
            ctx.beginPath(); ctx.arc(gateX - dotR * 1.6, pY, dotR, 0, Math.PI * 2); ctx.stroke()
            // 下横线
            ctx.beginPath(); ctx.moveTo(chanL, nY); ctx.lineTo(chanR, nY); ctx.stroke()
            // 下栅极竖线
            ctx.beginPath(); ctx.moveTo(gateX, nY - gateHalf); ctx.lineTo(gateX, nY + gateHalf); ctx.stroke()
        }
    // 其它未知类型：圆角矩形兜底
    } else {
        // 填充与描边色
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        // 线宽
        ctx.lineWidth = isSel ? 3 : 2
        // 圆角矩形
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    }

  // 简化模式：不画文字与端口细节，撤销旋转后直接返回
    if (simplify) {
        // 撤销旋转
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        // 恢复上下文
        ctx.restore(); return
    }

    // 元件位宽
    var bw = comp.bitWidth || 1
    // 元件显示进制
    var base = comp.displayBase || "bin"

    // 输入/常量元件：按显示进制画值
    if (t === "input" || t === "const") {
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
    } else if (t === "output") {
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
    } else if (t === "led") {
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
    } else {
        // 标签文本
        var label = ""
        // 与门
        if (t === "and") label = "AND"
        // 或门
        else if (t === "or") label = "OR"
        // 非门
        else if (t === "not") label = "NOT"
        // 与非门
        else if (t === "nand") label = "NAND"
        // 或非门
        else if (t === "nor") label = "NOR"
        // 异或门
        else if (t === "xor") label = "XOR"
        // 同或门
        else if (t === "xnor") label = "XNOR"
        // 分线器
        else if (t === "splitter") label = "分线器"
        // 集线器
        else if (t === "hub") label = "集线器"
        // 常量
        else if (t === "const") label = "常量"
        // 子电路实例：名称 + 子电路名
        else if (t === "sub") {
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
        // 传输门
        else if (t === "tgate") label = "传输门"
        // NMOS
        else if (t === "ntran") label = "NMOS"
        // PMOS
        else if (t === "ptran") label = "PMOS"
        // 有标签才画
        if (label.length > 0) {
            // 标签颜色
            ctx.fillStyle = "#e0e0e0"
            // 字体
            ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"
            // 居中
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            // 晶体管类标签下移，避免挡住符号
            ctx.fillText(label, sp.x + w / 2, sp.y + h / 2 + (t === "tgate" || t === "ntran" || t === "ptran" ? 14 * viewScale : 0))
        }
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

    // 子电路实例：画端口名
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

    // 非子电路/非文本且有自定义名称：在元件上方画名称
    if (comp.name && comp.name.length > 0 && t !== "sub" && t !== "text") {
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