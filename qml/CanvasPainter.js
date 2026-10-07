.pragma library
.import "Geometry.js" as Geometry

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

function drawWire(ctx, w, compById, outPortsById, inPortsById,
                  viewScale, viewX, viewY, W, H, simplify) {
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

    if ((s1.x < -100 && s2.x < -100) || (s1.x > W+100 && s2.x > W+100)) return
    if ((s1.y < -100 && s2.y < -100) || (s1.y > H+100 && s2.y > H+100)) return

    var cx = Math.max(Math.abs(s2.x - s1.x) / 2, 40 * viewScale)
    var wireVal = false
    if (fromC.outputPorts && fromC.outputPorts.length > w.fromPort) {
        var sv = fromC.outputPorts[w.fromPort]
        if (sv && sv.length > 0) wireVal = (sv.charAt(0) === '1')
    }
    ctx.strokeStyle = wireVal ? "#4caf50" : "#666666"
    ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
    ctx.beginPath()
    ctx.moveTo(s1.x, s1.y)
    ctx.bezierCurveTo(s1.x + cx, s1.y, s2.x - cx, s2.y, s2.x, s2.y)
    ctx.stroke()
}

function drawPendingWire(ctx, sp, wiringStartIsOutput, wireEndX, wireEndY) {
    ctx.strokeStyle = "#ffb74d"; ctx.lineWidth = 2
    ctx.setLineDash([6, 6])
    ctx.beginPath()
    if (wiringStartIsOutput) {
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(wireEndX, wireEndY)
    } else {
        ctx.moveTo(wireEndX, wireEndY); ctx.lineTo(sp.x, sp.y)
    }
    ctx.stroke()
    ctx.setLineDash([])
}

function drawComponent(ctx, comp, compSize, viewScale, viewX, viewY,
                       outPortsById, inPortsById, selectedCompId, selectedIds,
                       simplify, circuit) {
    var t = comp.type
    var size = compSize(comp)
    var sp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                    comp.x - size.w/2, comp.y - size.h/2)
    var w = size.w * viewScale
    var h = size.h * viewScale
    var isSel = (comp.id === selectedCompId)
    var isMultiSel = (selectedIds.indexOf(comp.id) >= 0)

    ctx.save()

    if (isMultiSel) {
        ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 2
        ctx.setLineDash([5, 3])
        ctx.strokeRect(sp.x - 3, sp.y - 3, w + 6, h + 6)
        ctx.setLineDash([])
    }

    if (t === "text") {
        if (!simplify) {
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px sans-serif"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(comp.content || "文本", sp.x + w/2, sp.y + h/2)
        }
        if (isSel) {
            ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 1.5
            ctx.setLineDash([4, 3])
            ctx.strokeRect(sp.x, sp.y, w, h)
            ctx.setLineDash([])
        }
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
            ctx.fillText(cval ? "1" : "0", sp.x + w/2, sp.y + h/2 - 4*viewScale)
            ctx.strokeStyle = cval ? "#4caf50" : "#666666"
            ctx.lineWidth = 1.5 * viewScale
            ctx.beginPath()
            var waveY = sp.y + h - 8*viewScale
            var waveAmp = 4*viewScale
            ctx.moveTo(sp.x + 6*viewScale, waveY + waveAmp)
            ctx.lineTo(sp.x + w*0.3, waveY + waveAmp)
            ctx.lineTo(sp.x + w*0.3, waveY - waveAmp)
            ctx.lineTo(sp.x + w*0.7, waveY - waveAmp)
            ctx.lineTo(sp.x + w*0.7, waveY + waveAmp)
            ctx.lineTo(sp.x + w - 6*viewScale, waveY + waveAmp)
            ctx.stroke()
        }
        ctx.restore()
        return
    }

    if (t === "led") {
        ctx.fillStyle = "#2a2a2a"
        ctx.beginPath()
        ctx.arc(sp.x + w/2, sp.y + h/2, Math.min(w, h)/2, 0, Math.PI*2)
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
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x + w/2, sp.y)
        ctx.arc(sp.x + w/2, sp.y + r, r, -Math.PI/2, Math.PI/2, false)
        ctx.lineTo(sp.x, sp.y + h); ctx.closePath(); ctx.fill(); ctx.stroke()
        if (t === "nand") {
            ctx.beginPath()
            ctx.arc(sp.x + w + 5*viewScale, sp.y + h/2, 5*viewScale, 0, Math.PI*2)
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    } else if (t === "or" || t === "nor" || t === "xor" || t === "xnor") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.beginPath()
        ctx.moveTo(sp.x, sp.y)
        ctx.quadraticCurveTo(sp.x + w*0.6, sp.y, sp.x + w, sp.y + h/2)
        ctx.quadraticCurveTo(sp.x + w*0.6, sp.y + h, sp.x, sp.y + h)
        ctx.quadraticCurveTo(sp.x + w*0.3, sp.y + h/2, sp.x, sp.y)
        ctx.closePath(); ctx.fill(); ctx.stroke()
        if (t === "xor" || t === "xnor") {
            ctx.beginPath()
            ctx.moveTo(sp.x - 6*viewScale, sp.y)
            ctx.quadraticCurveTo(sp.x - 6*viewScale + w*0.3, sp.y + h/2,
                                  sp.x - 6*viewScale, sp.y + h)
            ctx.stroke()
        }
        if (t === "nor" || t === "xnor") {
            ctx.beginPath()
            ctx.arc(sp.x + w + 5*viewScale, sp.y + h/2, 5*viewScale, 0, Math.PI*2)
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        }
    } else if (t === "not") {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
        ctx.lineWidth = isSel ? 3 : 2
        ctx.beginPath()
        ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x, sp.y + h)
        ctx.lineTo(sp.x + w*0.85, sp.y + h/2); ctx.closePath(); ctx.fill(); ctx.stroke()
        ctx.beginPath()
        ctx.arc(sp.x + w*0.85 + 6*viewScale, sp.y + h/2, 6*viewScale, 0, Math.PI*2)
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
    } else {
        ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
        ctx.lineWidth = isSel ? 3 : 2
        Geometry.roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
    }

    if (simplify) { ctx.restore(); return }

    var bw = comp.bitWidth || 1
    var base = comp.displayBase || "bin"

    if (t === "input") {
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
                ctx.fillText(v ? "1" : "0", gx + gw/2, gy + gh/2)
            }
        } else {
            var s = circuit.getInputAsString(comp.id)
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(s, sp.x + w/2, sp.y + h/2)
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
                var v2 = (i2 < ovStr.length && ovStr.charAt(i2) === '1')
                ctx.fillStyle = v2 ? "#4caf50" : "#1a1a1a"
                ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                Geometry.roundRect(ctx, gx2, gy2, gw2, gh2, 3 * viewScale); ctx.fill(); ctx.stroke()
                ctx.fillStyle = v2 ? "#ffffff" : "#888888"
                ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(v2 ? "1" : "0", gx2 + gw2/2, gy2 + gh2/2)
            }
        } else {
            var dispStr = circuit.formatValue(ovStr, base, bw)
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(dispStr, sp.x + w/2, sp.y + h/2)
        }
    } else if (t === "led") {
        var lvStr = ""
        if (comp.inputPortValues && comp.inputPortValues.length > 0)
            lvStr = comp.inputPortValues[0] || ""
        if (base !== "bin") {
            var ldStr = circuit.formatValue(lvStr, base, bw)
            ctx.fillStyle = "#dddddd"
            ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(ldStr, sp.x + w/2, sp.y + h/2)
        } else {
            for (var i3 = 0; i3 < bw; ++i3) {
                var cx2 = sp.x + (14 + i3 * 22) * viewScale
                var cy2 = sp.y + h/2
                var v3 = (i3 < lvStr.length && lvStr.charAt(i3) === '1')
                ctx.fillStyle = v3 ? "#ffd700" : "#2a2a2a"
                ctx.beginPath()
                ctx.arc(cx2, cy2, 8 * viewScale, 0, Math.PI*2)
                ctx.fill()
                ctx.strokeStyle = v3 ? "#ffeb3b" : "#444444"
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
        else if (t === "sub") label = comp.name || "SUB"
        if (label.length > 0) {
            ctx.fillStyle = "#e0e0e0"
            ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            ctx.fillText(label, sp.x + w/2, sp.y + h/2)
        }
    }

    var ins = inPortsById[comp.id] || []
    for (var k = 0; k < ins.length; k++) {
        var pIn = ins[k]
        var psp = Geometry.worldToScreen(viewScale, viewX, viewY,
                                          comp.x + pIn.x, comp.y + pIn.y)
        var portVal = false
        if (comp.inputPortValues && comp.inputPortValues.length > k) {
            var pvStr = comp.inputPortValues[k] || ""
            portVal = (pvStr.length > 0 && pvStr.charAt(0) === '1')
        }
        ctx.fillStyle = portVal ? "#4caf50" : "#999999"
        ctx.beginPath()
        ctx.arc(psp.x, psp.y, 6 * viewScale, 0, Math.PI*2)
        ctx.fill()
    }

    var outs = outPortsById[comp.id] || []
    for (var k2 = 0; k2 < outs.length; k2++) {
        var pOut = outs[k2]
        var psp2 = Geometry.worldToScreen(viewScale, viewX, viewY,
                                           comp.x + pOut.x, comp.y + pOut.y)
        var outVal = false
        if (comp.outputPorts && comp.outputPorts.length > k2) {
            var ovStr2 = comp.outputPorts[k2] || ""
            outVal = (ovStr2.length > 0 && ovStr2.charAt(0) === '1')
        }
        ctx.fillStyle = outVal ? "#4caf50" : "#999999"
        ctx.beginPath()
        ctx.arc(psp2.x, psp2.y, 6 * viewScale, 0, Math.PI*2)
        ctx.fill()
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

    if (comp.name && comp.name.length > 0 && t !== "input" && t !== "output"
            && t !== "led" && t !== "sub") {
        ctx.fillStyle = "#cccccc"
        ctx.font = Math.max(8, Math.round(10 * viewScale)) + "px sans-serif"
        ctx.textAlign = "center"; ctx.textBaseline = "top"
        ctx.fillText(comp.name, sp.x + w/2, sp.y - 4*viewScale)
    }

    ctx.restore()
}