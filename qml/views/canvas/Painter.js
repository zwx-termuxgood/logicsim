.pragma library
.import "Geometry.js" as Geometry

// ============================================================
// 背景网格
// ============================================================
function paintGrid(ctx, W, H, viewScale, viewX, viewY) {
    var step = 40 * viewScale
    if (step < 8) return
    var ox = viewX % step, oy = viewY % step
    ctx.strokeStyle = "#252525"; ctx.lineWidth = 1
    ctx.beginPath()
    for (var x = ox; x < W; x += step) { ctx.moveTo(x, 0); ctx.lineTo(x, H) }
    for (var y = oy; y < H; y += step) { ctx.moveTo(0, y); ctx.lineTo(W, y) }
    ctx.stroke()
}

function pickWireStyle(srcStr) {
    var hasE = srcStr.indexOf('E') !== -1
    var hasZ = !hasE && srcStr.indexOf('Z') !== -1
    var has1 = !hasE && !hasZ && srcStr.indexOf('1') !== -1
    if (hasE) return { color: "#e05252", dash: [3, 3] }
    if (hasZ) return { color: "#ffb74d", dash: [6, 5] }
    if (has1) return { color: "#4caf50", dash: [] }
    return { color: "#666666", dash: [] }
}

function pickWireColor(ctx, srcStr) {
    var s = pickWireStyle(srcStr)
    ctx.strokeStyle = s.color
    ctx.setLineDash(s.dash)
}

// ============================================================
// drawWire 分派（13 参数：新增 wireSources）
//   wireSources: { wireId -> "0110" } 由 CanvasView 预先计算
// ============================================================
function drawWire(ctx, w, compById, outPortsById, inPortsById,
                  viewScale, viewX, viewY, W, H, simplify, selectedWireId, wireSources) {
    var wt = w.wireType || "bezier"
    var isSel = (w.id === selectedWireId)

    if (wt === "ortho") {
        drawWireOrtho(ctx, w, compById, viewScale, viewX, viewY, W, H, simplify,
                      isSel, wireSources)
    } else {
        drawWireBezier(ctx, w, compById, outPortsById, inPortsById,
                       viewScale, viewX, viewY, W, H, simplify, isSel, wireSources)
    }
}

// ============================================================
// 贝塞尔连线
// ============================================================
function drawWireBezier(ctx, w, compById, outPortsById, inPortsById,
                        viewScale, viewX, viewY, W, H, simplify, isSel, wireSources) {
    var fromC = compById[w.fromComp]
    var toC = compById[w.toComp]
    if (!fromC || !toC) return

    var pOuts = outPortsById[w.fromComp] || []
    var pIns = inPortsById[w.toComp] || []
    if (w.fromPort < 0 || w.fromPort >= pOuts.length) return
    if (w.toPort < 0 || w.toPort >= pIns.length) return

    var p1 = { x: fromC.x + pOuts[w.fromPort].x, y: fromC.y + pOuts[w.fromPort].y }
    var p2 = { x: toC.x + pIns[w.toPort].x, y: toC.y + pIns[w.toPort].y }
    var s1 = Geometry.worldToScreen(viewScale, viewX, viewY, p1.x, p1.y)
    var s2 = Geometry.worldToScreen(viewScale, viewX, viewY, p2.x, p2.y)

    if ((s1.x < -100 && s2.x < -100) || (s1.x > W + 100 && s2.x > W + 100)) return
    if ((s1.y < -100 && s2.y < -100) || (s1.y > H + 100 && s2.y > H + 100)) return

    var cx = Math.max(Math.abs(s2.x - s1.x) / 2, 40 * viewScale)

    // 贝塞尔一定有 fromComp，直接取
    var srcStr = "0"
    if (wireSources && wireSources[w.id] !== undefined) {
        srcStr = wireSources[w.id]
    } else if (fromC.outputPorts && fromC.outputPorts.length > w.fromPort) {
        var sv = fromC.outputPorts[w.fromPort]
        if (sv && sv.length > 0) srcStr = sv
    }

    ctx.beginPath()
    if (isSel) {
        ctx.strokeStyle = "#58a6ff"
        ctx.lineWidth = 4
        ctx.setLineDash([])
    } else {
        var pc = pickWireStyle(srcStr)
        ctx.strokeStyle = pc.color
        ctx.setLineDash(pc.dash)
        ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
    }
    ctx.moveTo(s1.x, s1.y)
    ctx.bezierCurveTo(s1.x + cx, s1.y, s2.x - cx, s2.y, s2.x, s2.y)
    ctx.stroke()
    ctx.setLineDash([])
}

// ============================================================
// 正交连线（path 折线）
//   ★ 修复：srcStr 优先用 wireSources[w.id]（递归追上游）
// ============================================================
function drawWireOrtho(ctx, w, compById, viewScale, viewX, viewY, W, H, simplify,
                       isSel, wireSources) {
    if (!w.path || w.path.length < 2) return

    var vis = false
    for (var i = 0; i < w.path.length; i++) {
        var p = w.path[i]
        var sp = Geometry.worldToScreen(viewScale, viewX, viewY, p.x, p.y)
        if (sp.x >= -50 && sp.x <= W + 50 && sp.y >= -50 && sp.y <= H + 50) {
            vis = true
            break
        }
    }
    if (!vis) return

    // ★ 优先用 wireSources
    var srcStr = "0"
    if (wireSources && wireSources[w.id] !== undefined) {
        srcStr = wireSources[w.id]
    } else if (w.fromComp && compById[w.fromComp]) {
        var fromC = compById[w.fromComp]
        var fromPort = w.fromPort || 0
        if (fromC.outputPorts && fromC.outputPorts.length > fromPort) {
            var sv = fromC.outputPorts[fromPort]
            if (sv && sv.length > 0) srcStr = sv
        }
    }

    ctx.beginPath()
    for (var j = 0; j < w.path.length; j++) {
        var pt = w.path[j]
        var spt = Geometry.worldToScreen(viewScale, viewX, viewY, pt.x, pt.y)
        if (j === 0) ctx.moveTo(spt.x, spt.y)
        else         ctx.lineTo(spt.x, spt.y)
    }

    if (isSel) {
        ctx.strokeStyle = "#58a6ff"
        ctx.lineWidth = 4
        ctx.setLineDash([])
    } else {
        var pc = pickWireStyle(srcStr)
        ctx.strokeStyle = pc.color
        ctx.setLineDash(pc.dash)
        ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
    }
    ctx.stroke()
    ctx.setLineDash([])
}

// ============================================================
// 连接圆点
// ============================================================
function drawConnectionDots(ctx, dots, viewScale, viewX, viewY) {
    for (var i = 0; i < dots.length; i++) {
        var d = dots[i]
        var sp = Geometry.worldToScreen(viewScale, viewX, viewY, d.x, d.y)
        ctx.fillStyle = "#999999"
        ctx.beginPath()
        ctx.arc(sp.x, sp.y, 4 * viewScale, 0, Math.PI * 2)
        ctx.fill()
    }
}

// ============================================================
// 连线预览
// ============================================================
function drawPendingBezier(ctx, startScreen, wireEndX, wireEndY, viewScale) {
    ctx.strokeStyle = "#ffb74d"
    ctx.lineWidth = 2
    ctx.setLineDash([6, 6])
    ctx.beginPath()
    var cx = Math.max(Math.abs(wireEndX - startScreen.x) / 2, 40 * viewScale)
    ctx.moveTo(startScreen.x, startScreen.y)
    ctx.bezierCurveTo(startScreen.x + cx, startScreen.y,
                      wireEndX - cx, wireEndY,
                      wireEndX, wireEndY)
    ctx.stroke()
    ctx.setLineDash([])
}

function drawPendingOrtho(ctx, startScreen, direction, wireEndX, wireEndY) {
    ctx.strokeStyle = "#ffb74d"
    ctx.lineWidth = 2
    ctx.setLineDash([6, 6])
    ctx.beginPath()
    if (direction === 1) {
        ctx.moveTo(startScreen.x, startScreen.y)
        ctx.lineTo(wireEndX, startScreen.y)
        ctx.lineTo(wireEndX, wireEndY)
    } else if (direction === 2) {
        ctx.moveTo(startScreen.x, startScreen.y)
        ctx.lineTo(startScreen.x, wireEndY)
        ctx.lineTo(wireEndX, wireEndY)
    } else {
        ctx.moveTo(startScreen.x, startScreen.y)
        ctx.lineTo(wireEndX, wireEndY)
    }
    ctx.stroke()
    ctx.setLineDash([])
}

function drawPendingWire(ctx, startScreen, direction, wireEndX, wireEndY) {
    drawPendingOrtho(ctx, startScreen, direction, wireEndX, wireEndY)
}

// ============================================================
// 端口颜色
// ============================================================
function classifyPortStr(s) {
    if (!s || s.length === 0) return 0
    if (s.indexOf('E') !== -1) return 3
    if (s.indexOf('Z') !== -1) return 2
    if (s.indexOf('1') !== -1) return 1
    return 0
}

function fillPortColor(ctx, kind) {
    if (kind === 3)      ctx.fillStyle = "#e05252"
    else if (kind === 2) ctx.fillStyle = "#ffb74d"
    else if (kind === 1) ctx.fillStyle = "#4caf50"
    else                 ctx.fillStyle = "#999999"
}

// ============================================================
// 元件绘制（与上一版一致，无改动）
// ============================================================
function drawComponent(ctx, comp, compSize, viewScale, viewX, viewY,
                       outPortsById, inPortsById, selectedCompId, selectedIds,
                       simplify, circuit) {
    var t = comp.type
    var size = compSize(comp)
    var sp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                    comp.x - size.w / 2, comp.y - size.h / 2)
    var w = size.w * viewScale
    var h = size.h * viewScale
    var isSel = (comp.id === selectedCompId)
    var isMultiSel = (selectedIds.indexOf(comp.id) >= 0)

    var rotDeg = (comp.rotation || 0) * 90
    var rotRad = rotDeg * Math.PI / 180
    var cxs = sp.x + w / 2
    var cys = sp.y + h / 2

    ctx.save()

    if (isMultiSel) {
        ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 2
        ctx.setLineDash([5, 3])
        ctx.strokeRect(sp.x - 3, sp.y - 3, w + 6, h + 6)
        ctx.setLineDash([])
    }

    if (rotRad !== 0) {
        ctx.translate(cxs, cys); ctx.rotate(rotRad); ctx.translate(-cxs, -cys)
    }

    if (t === "text") {
        if (!simplify) {
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px sans-serif"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(comp.content || "文本", sp.x + w / 2, sp.y + h / 2)
        }
        if (isSel) {
            ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 1.5
            ctx.setLineDash([4, 3])
            ctx.strokeRect(sp.x, sp.y, w, h)
            ctx.setLineDash([])
        }
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        ctx.restore()
        return
    }

    if (t === "clock") {
        ctx.fillStyle = "#2a3a2a"
        ctx.strokeStyle = isSel ? "#ffffff" : "#4caf50"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        if (!simplify) {
            var cval = (comp.outputPorts && comp.outputPorts[0] === "1")
            ctx.fillStyle = cval ? "#4caf50" : "#555555"
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(cval ? "1" : "0", sp.x + w / 2, sp.y + h / 2 - 4 * viewScale)
        }
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        ctx.restore()
        return
    }

    if (t === "const") {
        ctx.fillStyle = "#2a2a3a"; ctx.strokeStyle = isSel ? "#ffffff" : "#7c8cff"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    } else if (t === "led") {
        ctx.fillStyle = "#2a2a2a"
        ctx.beginPath()
        ctx.arc(sp.x + w / 2, sp.y + h / 2, Math.min(w, h) / 2, 0, Math.PI * 2)
        ctx.fill()
        ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.stroke()
    } else if (t === "input") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#666666"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    } else if (t === "and" || t === "nand") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.beginPath()
        var r = h / 2
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x + w / 2, sp.y)
        ctx.arc(sp.x + w / 2, sp.y + r, r, -Math.PI / 2, Math.PI / 2, false)
        ctx.lineTo(sp.x, sp.y + h); ctx.closePath(); ctx.fill(); ctx.stroke()
        if (t === "nand") {
            ctx.beginPath()
            ctx.arc(sp.x + w + 5 * viewScale, sp.y + h / 2, 5 * viewScale, 0, Math.PI * 2)
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    } else if (t === "or" || t === "nor" || t === "xor" || t === "xnor") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.beginPath()
        ctx.moveTo(sp.x, sp.y)
        ctx.quadraticCurveTo(sp.x + w * 0.6, sp.y, sp.x + w, sp.y + h / 2)
        ctx.quadraticCurveTo(sp.x + w * 0.6, sp.y + h, sp.x, sp.y + h)
        ctx.quadraticCurveTo(sp.x + w * 0.3, sp.y + h / 2, sp.x, sp.y)
        ctx.closePath(); ctx.fill(); ctx.stroke()
        if (t === "xor" || t === "xnor") {
            ctx.beginPath()
            ctx.moveTo(sp.x - 6 * viewScale, sp.y)
            ctx.quadraticCurveTo(sp.x - 6 * viewScale + w * 0.3, sp.y + h / 2,
                                  sp.x - 6 * viewScale, sp.y + h)
            ctx.stroke()
        }
        if (t === "nor" || t === "xnor") {
            ctx.beginPath()
            ctx.arc(sp.x + w + 5 * viewScale, sp.y + h / 2, 5 * viewScale, 0, Math.PI * 2)
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    } else if (t === "not") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.beginPath()
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x, sp.y + h)
        ctx.lineTo(sp.x + w * 0.85, sp.y + h / 2); ctx.closePath(); ctx.fill(); ctx.stroke()
        ctx.beginPath()
        ctx.arc(sp.x + w * 0.85 + 6 * viewScale, sp.y + h / 2, 6 * viewScale, 0, Math.PI * 2)
        ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
    } else if (t === "splitter" || t === "hub") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    } else if (t === "output") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#ff9800"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    } else if (t === "sub") {
        ctx.fillStyle = "#1a2433"; ctx.strokeStyle = isSel ? "#ffffff" : "#a371f7"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 6 * viewScale); ctx.fill(); ctx.stroke()
    } else if (t === "ntran" || t === "ptran") {
        ctx.fillStyle = "#1f1a2e"
        ctx.strokeStyle = isSel ? "#ffffff" : "#9c27b0"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        if (!simplify) {
            var cx2 = sp.x + w * 0.5
            var cy2 = sp.y + h / 2
            ctx.strokeStyle = isSel ? "#ffffff" : "#b388ff"
            ctx.lineWidth = Math.max(1.5, 2 * viewScale)
            ctx.beginPath(); ctx.moveTo(cx2, sp.y + h * 0.2); ctx.lineTo(cx2, sp.y + h * 0.8); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx2 - 8 * viewScale, sp.y + h * 0.2); ctx.lineTo(cx2 - 8 * viewScale, sp.y + h * 0.8); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx2 - 8 * viewScale, sp.y + h * 0.5); ctx.lineTo(sp.x + 4 * viewScale, sp.y + h * 0.5); ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx2, sp.y + h * 0.25); ctx.lineTo(sp.x + w, sp.y + h * 0.25)
            ctx.moveTo(cx2, sp.y + h * 0.75); ctx.lineTo(sp.x + w, sp.y + h * 0.75)
            ctx.stroke()
            if (t === "ptran") {
                ctx.beginPath()
                ctx.arc(cx2 - 8 * viewScale - 4 * viewScale, cy2, 4 * viewScale, 0, Math.PI * 2)
                ctx.stroke()
            }
        }
    } else if (t === "tgate") {
        ctx.fillStyle = "#1f1a2e"
        ctx.strokeStyle = isSel ? "#ffffff" : "#9c27b0"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        if (!simplify) {
            var chanL = sp.x + w * 0.32
            var chanR = sp.x + w * 0.68
            var pY = sp.y + h * 0.30
            var nY = sp.y + h * 0.70
            var gateX = sp.x + w * 0.24
            var gateHalf = h * 0.10
            var dotR = 2.2 * viewScale
            ctx.strokeStyle = isSel ? "#ffffff" : "#b388ff"
            ctx.lineWidth = Math.max(1.2, 1.6 * viewScale)
            ctx.beginPath(); ctx.moveTo(chanL, pY); ctx.lineTo(chanL, nY); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(chanR, pY); ctx.lineTo(chanR, nY); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(chanL, pY); ctx.lineTo(chanR, pY); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(gateX, pY - gateHalf); ctx.lineTo(gateX, pY + gateHalf); ctx.stroke()
            ctx.beginPath(); ctx.arc(gateX - dotR * 1.6, pY, dotR, 0, Math.PI * 2); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(chanL, nY); ctx.lineTo(chanR, nY); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(gateX, nY - gateHalf); ctx.lineTo(gateX, nY + gateHalf); ctx.stroke()
        }
    } else {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    }

    if (simplify) {
        if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }
        ctx.restore(); return
    }

    var bw = comp.bitWidth || 1
    var base = comp.displayBase || "bin"

    if (t === "input" || t === "const") {
        var bits = comp.inputBits || []
        if (base === "bin") {
            for (var i = 0; i < bw; ++i) {
                var gx = sp.x + (4 + i * 22) * viewScale
                var gy = sp.y + 4 * viewScale
                var gw = 20 * viewScale
                var gh = h - 8 * viewScale
                var v = bits[i] || false
                ctx.fillStyle = v ? "#4caf50" : "#1a1a1a"
                ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                Geometry.roundRect(ctx, gx, gy, gw, gh, 3 * viewScale); ctx.fill(); ctx.stroke()
                ctx.fillStyle = v ? "#ffffff" : "#888888"
                ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(v ? "1" : "0", gx + gw / 2, gy + gh / 2)
            }
        } else {
            var s = circuit.getInputAsString(comp.id)
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(s, sp.x + w / 2, sp.y + h / 2)
        }
    } else if (t === "output") {
        var ovStr = ""
        if (comp.inputPortValues && comp.inputPortValues.length > 0)
            ovStr = comp.inputPortValues[0] || ""
        if (base === "bin") {
            for (var i2 = 0; i2 < bw; ++i2) {
                var gx2 = sp.x + (4 + i2 * 22) * viewScale
                var gy2 = sp.y + 4 * viewScale
                var gw2 = 20 * viewScale
                var gh2 = h - 8 * viewScale
                var ch = (i2 < ovStr.length) ? ovStr.charAt(i2) : '0'
                if (ch === '1')      ctx.fillStyle = "#4caf50"
                else if (ch === 'Z') ctx.fillStyle = "#ffb74d"
                else if (ch === 'E') ctx.fillStyle = "#e05252"
                else                 ctx.fillStyle = "#1a1a1a"
                ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                Geometry.roundRect(ctx, gx2, gy2, gw2, gh2, 3 * viewScale); ctx.fill(); ctx.stroke()
                ctx.fillStyle = "#ffffff"
                ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(ch, gx2 + gw2 / 2, gy2 + gh2 / 2)
            }
        } else {
            var dispStr = circuit.formatValue(ovStr, base, bw)
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(dispStr, sp.x + w / 2, sp.y + h / 2)
        }
    } else if (t === "led") {
        var lvStr = ""
        if (comp.inputPortValues && comp.inputPortValues.length > 0)
            lvStr = comp.inputPortValues[0] || ""
        var ledColor = comp.color || "#ffd700"
        if (base !== "bin") {
            var ldStr = circuit.formatValue(lvStr, base, bw)
            ctx.fillStyle = ledColor
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(ldStr, sp.x + w / 2, sp.y + h / 2)
        } else {
            for (var i3 = 0; i3 < bw; ++i3) {
                var cx3 = sp.x + (14 + i3 * 22) * viewScale
                var cy3 = sp.y + h / 2
                var ch3 = (i3 < lvStr.length) ? lvStr.charAt(i3) : '0'
                if (ch3 === '1')      ctx.fillStyle = ledColor
                else if (ch3 === 'Z') ctx.fillStyle = "#ffb74d"
                else if (ch3 === 'E') ctx.fillStyle = "#e05252"
                else                  ctx.fillStyle = "#2a2a2a"
                ctx.beginPath(); ctx.arc(cx3, cy3, 8 * viewScale, 0, Math.PI * 2); ctx.fill()
                ctx.strokeStyle = (ch3 === '1') ? ledColor
                                : (ch3 === 'E') ? "#e05252"
                                : (ch3 === 'Z') ? "#ffb74d"
                                : "#444444"
                ctx.lineWidth = 1.5 * viewScale
                ctx.stroke()
            }
        }
    } else {
        var label = ""
        if (t === "and") label = "AND"
        else if (t === "or") label = "OR"
        else if (t === "not") label = "NOT"
        else if (t === "nand") label = "NAND"
        else if (t === "nor") label = "NOR"
        else if (t === "xor") label = "XOR"
        else if (t === "xnor") label = "XNOR"
        else if (t === "splitter") label = "分线器"
        else if (t === "hub") label = "集线器"
        else if (t === "const") label = "常量"
        else if (t === "sub") {
            var subName = ""
            if (circuit && circuit.getSubcircuitName)
                subName = circuit.getSubcircuitName(comp.subId) || ""
            if (comp.name && comp.name.length > 0 && comp.name !== subName)
                label = comp.name + " [" + subName + "]"
            else
                label = subName || comp.name || "SUB"
        }
        else if (t === "tgate") label = "传输门"
        else if (t === "ntran") label = "NMOS"
        else if (t === "ptran") label = "PMOS"
        if (label.length > 0) {
            ctx.fillStyle = "#e0e0e0"
            ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(label, sp.x + w / 2, sp.y + h / 2 + (t === "tgate" || t === "ntran" || t === "ptran" ? 14 * viewScale : 0))
        }
    }

    if (rotRad !== 0) { ctx.translate(cxs, cys); ctx.rotate(-rotRad); ctx.translate(-cxs, -cys) }

    var ins = inPortsById[comp.id] || []
    for (var k = 0; k < ins.length; k++) {
        var pIn = ins[k]
        var psp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                          comp.x + pIn.x, comp.y + pIn.y)
        var pvStr = ""
        if (comp.inputPortValues && comp.inputPortValues.length > k)
            pvStr = comp.inputPortValues[k] || ""
        fillPortColor(ctx, classifyPortStr(pvStr))
        ctx.beginPath(); ctx.arc(psp.x, psp.y, 6 * viewScale, 0, Math.PI * 2); ctx.fill()

        if (!simplify && (t === "ntran" || t === "ptran")) {
            ctx.fillStyle = "#b388ff"
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            if (k === 0) { ctx.textAlign = "right"; ctx.textBaseline = "middle"; ctx.fillText("G", psp.x - 10 * viewScale, psp.y) }
            else if (k === 1) { ctx.textAlign = "left"; ctx.textBaseline = "middle"; ctx.fillText("S", psp.x + 10 * viewScale, psp.y) }
        } else if (!simplify && t === "tgate") {
            ctx.fillStyle = "#b388ff"
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            ctx.textAlign = "right"; ctx.textBaseline = "middle"
            if (k === 0) ctx.fillText("D", psp.x - 10 * viewScale, psp.y)
            else if (k === 1) ctx.fillText("EN", psp.x - 10 * viewScale, psp.y)
            else if (k === 2) ctx.fillText("/EN", psp.x - 10 * viewScale, psp.y)
        }
    }

    var outs = outPortsById[comp.id] || []
    for (var k2 = 0; k2 < outs.length; k2++) {
        var pOut = outs[k2]
        var psp2 = Geometry.worldToScreen(viewScale, viewX, viewY,
                                           comp.x + pOut.x, comp.y + pOut.y)
        var ovStr2 = ""
        if (comp.outputPorts && comp.outputPorts.length > k2)
            ovStr2 = comp.outputPorts[k2] || ""
        fillPortColor(ctx, classifyPortStr(ovStr2))
        ctx.beginPath(); ctx.arc(psp2.x, psp2.y, 6 * viewScale, 0, Math.PI * 2); ctx.fill()

        if (!simplify && (t === "ntran" || t === "ptran")) {
            ctx.fillStyle = "#b388ff"
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            ctx.textAlign = "left"; ctx.textBaseline = "middle"
            ctx.fillText("D", psp2.x + 10 * viewScale, psp2.y)
        } else if (!simplify && t === "tgate") {
            ctx.fillStyle = "#b388ff"
            ctx.font = "bold " + Math.max(9, Math.round(11 * viewScale)) + "px sans-serif"
            ctx.textAlign = "left"; ctx.textBaseline = "middle"
            ctx.fillText("S", psp2.x + 10 * viewScale, psp2.y)
        }
    }

    if (t === "sub") {
        var subIns = comp.subInputNames || []
        var subOuts = comp.subOutputNames || []
        ctx.font = Math.max(7, Math.round(9 * viewScale)) + "px sans-serif"
        ctx.fillStyle = "#a371f7"
        ctx.textAlign = "right"; ctx.textBaseline = "middle"
        for (var a = 0; a < ins.length && a < subIns.length; ++a) {
            var ppA = Geometry.worldToScreen(viewScale, viewX, viewY,
                                              comp.x + ins[a].x, comp.y + ins[a].y)
            ctx.fillText(subIns[a], ppA.x - 10 * viewScale, ppA.y)
        }
        ctx.textAlign = "left"
        for (var b = 0; b < outs.length && b < subOuts.length; ++b) {
            var ppB = Geometry.worldToScreen(viewScale, viewX, viewY,
                                              comp.x + outs[b].x, comp.y + outs[b].y)
            ctx.fillText(subOuts[b], ppB.x + 10 * viewScale, ppB.y)
        }
    }

    if (comp.name && comp.name.length > 0 && t !== "sub" && t !== "text") {
        ctx.fillStyle = "#cccccc"
        ctx.font = Math.max(8, Math.round(10 * viewScale)) + "px sans-serif"
        ctx.textAlign = "center"; ctx.textBaseline = "bottom"
        ctx.fillText(comp.name, sp.x + w / 2, sp.y - 4 * viewScale)
    }

    ctx.restore()
}