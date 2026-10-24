.pragma library

function findPortAt(main, cv, sx, sy) {
    var comps = main.cachedComps
    var best = null
    var bestDist = 900
    for (var i = 0; i < comps.length; i++) {
        var c = comps[i]
        var outs = main.outPortsById[c.id] || []
        for (var k = 0; k < outs.length; k++) {
            var p = outs[k]
            var pp = cv.worldToScreen(c.x + p.x, c.y + p.y)
            var dx = sx - pp.x, dy = sy - pp.y
            var d = dx*dx + dy*dy
            if (d < bestDist) {
                bestDist = d
                best = { compId: c.id, portIdx: k, isOutput: true }
            }
        }
        var ins = main.inPortsById[c.id] || []
        for (var k2 = 0; k2 < ins.length; k2++) {
            var p2 = ins[k2]
            var pp2 = cv.worldToScreen(c.x + p2.x, c.y + p2.y)
            var dx2 = sx - pp2.x, dy2 = sy - pp2.y
            var d2 = dx2*dx2 + dy2*dy2
            if (d2 < bestDist) {
                bestDist = d2
                best = { compId: c.id, portIdx: k2, isOutput: false }
            }
        }
    }
    return best
}

function resolveEffectiveWireType(main) {
    var wm = main.circuitRef.wireMode
    if (wm === "ortho") return "ortho"
    if (wm === "bezier") return "bezier"
    var cwt = main.circuitRef.currentWireType
    return (cwt === "ortho") ? "ortho" : "bezier"
}

function snapWorldPoint(main, cv, mouse) {
    var port = findPortAt(main, cv, mouse.x, mouse.y)
    if (port) {
        var ep = cv.portPos(port.compId, port.portIdx, port.isOutput)
        return { x: ep.x, y: ep.y, port: port }
    }
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    return {
        x: Math.round(wp.x / 10) * 10,
        y: Math.round(wp.y / 10) * 10,
        port: null
    }
}

function snapToWire(main, wireId, wx, wy) {
    var w = main.circuitRef.getWire(wireId)
    if (!w || !w.path || w.path.length < 2) return null
    var path = w.path
    var bestX = 0, bestY = 0, bestD = 1e18
    var bestIsHorizontal = false
    for (var i = 0; i + 1 < path.length; i++) {
        var p0 = path[i], p1 = path[i + 1]
        var x1 = p0.x, y1 = p0.y, x2 = p1.x, y2 = p1.y
        var dx = x2 - x1, dy = y2 - y1
        var len2 = dx*dx + dy*dy
        var t = 0
        if (len2 > 1e-9) {
            t = ((wx - x1) * dx + (wy - y1) * dy) / len2
            if (t < 0) t = 0; else if (t > 1) t = 1
        }
        var cx = x1 + t * dx, cy = y1 + t * dy
        var d = (wx - cx) * (wx - cx) + (wy - cy) * (wy - cy)
        if (d < bestD) {
            bestD = d
            bestX = cx
            bestY = cy
            bestIsHorizontal = Math.abs(y1 - y2) < 0.5
        }
    }
    if (bestIsHorizontal) {
        var snappedX = Math.round(bestX / 10) * 10
        var lo = Math.min(path[0].x, path[path.length-1].x)
        var hi = Math.max(path[0].x, path[path.length-1].x)
        if (snappedX < lo) snappedX = lo
        if (snappedX > hi) snappedX = hi
        return { x: snappedX, y: bestY }
    } else {
        var snappedY = Math.round(bestY / 10) * 10
        var lo2 = Math.min(path[0].y, path[path.length-1].y)
        var hi2 = Math.max(path[0].y, path[path.length-1].y)
        if (snappedY < lo2) snappedY = lo2
        if (snappedY > hi2) snappedY = hi2
        return { x: bestX, y: snappedY }
    }
}

function updateWiringDirection(main, sx, sy, wx, wy) {
    var H = 1, V = 2
    var sameX = Math.abs(wx - sx) < 0.5
    var sameY = Math.abs(wy - sy) < 0.5
    var dir = main.wiringDirection
    if (dir === 0) {
        if (!sameX) dir = H
        else if (!sameY) dir = V
    } else if (dir === H && sameX) {
        if (sameY) dir = 0
        else dir = V
    } else if (dir === V && sameY) {
        if (sameX) dir = 0
        else dir = H
    }
    main.wiringDirection = dir
}

// ★ 容差从 0.5 放宽到 2.0：端点在段内部更容易命中（尤其缩放后）
function pointOnSegInterior(px, py, x1, y1, x2, y2) {
    var tol = 2.0
    if (Math.abs(y1 - y2) < 0.5) {
        if (Math.abs(py - y1) > 0.5) return false
        var lo = Math.min(x1, x2)
        var hi = Math.max(x1, x2)
        return px > lo + tol && px < hi - tol
    }
    if (Math.abs(x1 - x2) < 0.5) {
        if (Math.abs(px - x1) > 0.5) return false
        var lo2 = Math.min(y1, y2)
        var hi2 = Math.max(y1, y2)
        return py > lo2 + tol && py < hi2 - tol
    }
    return false
}

// 起点是否在某 wire 的端点附近（起点在端点 → 缩短候选）
function findShortenTargetAt(main, wx, wy) {
    var wids = []
    for (var i = 0; i < main.cachedWires.length; i++) {
        var w = main.cachedWires[i]
        if ((w.wireType || "bezier") !== "ortho") continue
        if (!w.path || w.path.length < 2) continue
        var p0 = w.path[0]
        var pn = w.path[w.path.length - 1]
        if ((Math.abs(p0.x - wx) < 3 && Math.abs(p0.y - wy) < 3) ||
            (Math.abs(pn.x - wx) < 3 && Math.abs(pn.y - wy) < 3)) {
            wids.push(w.id)
        }
    }
    return wids
}

function startWiringFromPort(main, cv, onPort) {
    var startPt = cv.portPos(onPort.compId, onPort.portIdx, onPort.isOutput)
    main.wiringFromComp = onPort.compId
    main.wiringFromPort = onPort.portIdx
    main.wiringStartIsOutput = onPort.isOutput
    main.wiring = true
    main.wiringStartX = startPt.x
    main.wiringStartY = startPt.y
    var ss = cv.worldToScreen(startPt.x, startPt.y)
    main.wireEndX = ss.x
    main.wireEndY = ss.y
    main.wiringDirection = 0
    main.selectedWireId = ""
}

function startWiringFromWire(main, cv, snapPt) {
    main.wiringFromComp = ""
    main.wiringFromPort = -1
    main.wiringStartIsOutput = true
    main.wiring = true
    main.wiringStartX = snapPt.x
    main.wiringStartY = snapPt.y
    var ss = cv.worldToScreen(snapPt.x, snapPt.y)
    main.wireEndX = ss.x
    main.wireEndY = ss.y
    main.wiringDirection = 0
    main.selectedWireId = ""
}

function onPressed(main, cv, mouse) {
    cv.lastMouseX = mouse.x; cv.lastMouseY = mouse.y
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    var editable = !main.circuitViewOnly

    if (editable && main.mode === "edit" && main.placingType !== "") {
        var subId = main.placingType === "sub" ? main.placingSubId : ""
        var newId = main.circuitRef.addComponent(main.placingType, wp.x, wp.y,
                                                 main.placingBitWidth,
                                                 main.placingInputCount,
                                                 subId)
        if (newId === "") main.statusText = "⚠ 无法放置"
        else {
            main.selectedCompId = newId
            main.selectedWireId = ""
            main.refreshSel()
        }
        main.placingType = ""
        main.placingSubId = ""
        return
    }

    var comps = main.cachedComps

    // select 模式
    if (main.mode === "select") {
        var wid = main.circuitRef.wireAtPoint(wp.x, wp.y, 15 / Math.max(0.1, main.viewScale))
        if (wid && wid !== "") {
            main.selectedWireId = wid
            main.selectedIds = []
            main.selectedCompId = ""
            main.statusText = "已选中连线（Delete 删除，Ctrl+C 复制）"
            cv.requestPaint()
            return
        }
        for (var i = comps.length - 1; i >= 0; i--) {
            var c = comps[i]
            var size = cv.compSize(c)
            if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                if (main.selectedIds.indexOf(c.id) < 0) main.selectedIds = [c.id]
                main.selectedWireId = ""
                main.circuitRef.breakUndoMerge()
                main.draggingSelection = true
                main.lastDragWorldX = wp.x
                main.lastDragWorldY = wp.y
                cv.requestPaint()
                return
            }
        }
        main.selectedIds = []
        main.selectedWireId = ""
        main.boxSelecting = true
        main.boxStartX = mouse.x; main.boxStartY = mouse.y
        main.boxCurrentX = mouse.x; main.boxCurrentY = mouse.y
        cv.requestPaint()
        return
    }

    // ★ 编辑模式：无论起点是什么，先探测缩短候选
    var shortenCandidates = []
    if (editable && main.mode === "edit") {
        shortenCandidates = findShortenTargetAt(main, wp.x, wp.y)
    }

    // 端口优先
    if (editable && main.mode === "edit") {
        var onPort = findPortAt(main, cv, mouse.x, mouse.y)
        if (onPort) {
            startWiringFromPort(main, cv, onPort)
            main.shortenWireCandidates = shortenCandidates
            main.statusText = "从端口引出（拖回原线内部可缩短）"
            cv.requestPaint()
            return
        }

        // 从线中间：T 型接入
        var wireId = main.circuitRef.wireAtPoint(wp.x, wp.y, 15 / Math.max(0.1, main.viewScale))
        if (wireId && wireId !== "") {
            var effType = resolveEffectiveWireType(main)
            if (effType === "ortho") {
                var snapPt = snapToWire(main, wireId, wp.x, wp.y)
                if (snapPt) {
                    startWiringFromWire(main, cv, snapPt)
                    // ★ 修复：保留缩短候选（用户可能想从这条线上缩短回原线内部）
                    main.shortenWireCandidates = shortenCandidates
                    main.statusText = "从线引出（T 型接入，拖回线上方可缩短）"
                    cv.requestPaint()
                    return
                }
            } else {
                main.statusText = "⚠ 贝塞尔线只能从端口引出"
            }
        }
    }

    // 元件
    for (var i4 = comps.length - 1; i4 >= 0; i4--) {
        var c4 = comps[i4]
        var size4 = cv.compSize(c4)
        if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
            main.selectedCompId = c4.id
            main.selectedWireId = ""
            main.refreshSel()

            if (editable && main.mode === "control") {
                if (c4.type === "input" || c4.type === "const") {
                    var base = c4.displayBase || "bin"
                    if (base === "bin") {
                        var bitIdx = cv.bitIndexAt(c4, wp.x)
                        var bits = c4.inputBits || []
                        main.circuitRef.setBitValue(c4.id, bitIdx, !(bits[bitIdx] || false))
                    }
                } else if (c4.type === "clock") {
                    main.circuitRef.toggleClock(c4.id)
                }
            } else if (editable && main.mode === "edit") {
                main.circuitRef.breakUndoMerge()
                cv.draggingComp = true
                cv.dragCompId = c4.id
            }
            return
        }
    }

    // 空白
    main.selectedCompId = ""
    main.selectedWireId = ""
    main.refreshSel()
    main.shortenWireCandidates = []
    cv.panning = true
}

function onPositionChanged(main, cv, mouse) {
    if (main.boxSelecting) {
        main.boxCurrentX = mouse.x; main.boxCurrentY = mouse.y
        return
    }

    if (main.draggingSelection) {
        var wp = cv.screenToWorld(mouse.x, mouse.y)
        var dwx = wp.x - main.lastDragWorldX
        var dwy = wp.y - main.lastDragWorldY
        if (Math.abs(dwx) > 0.001 || Math.abs(dwy) > 0.001) {
            main.circuitRef.moveSelection(main.selectedIds, dwx, dwy)
            main.lastDragWorldX = wp.x
            main.lastDragWorldY = wp.y
        }
        return
    }

    if (main.wiring) {
        var sp = snapWorldPoint(main, cv, mouse)
        var sx = main.wiringStartX, sy = main.wiringStartY
        updateWiringDirection(main, sx, sy, sp.x, sp.y)
        var es = cv.worldToScreen(sp.x, sp.y)
        main.wireEndX = es.x
        main.wireEndY = es.y
        cv.requestPaint()
        return
    }

    if (cv.draggingComp && cv.dragCompId !== "" && !main.circuitViewOnly) {
        var wp3 = cv.screenToWorld(mouse.x, mouse.y)
        main.circuitRef.moveComponent(cv.dragCompId, wp3.x, wp3.y)
        return
    }

    if (cv.panning) {
        main.viewX += mouse.x - cv.lastMouseX
        main.viewY += mouse.y - cv.lastMouseY
        cv.lastMouseX = mouse.x; cv.lastMouseY = mouse.y
        cv.requestPaint()
    }
}

function onReleased(main, cv, mouse) {
    if (main.boxSelecting) {
        var x1 = Math.min(main.boxStartX, mouse.x)
        var y1 = Math.min(main.boxStartY, mouse.y)
        var x2 = Math.max(main.boxStartX, mouse.x)
        var y2 = Math.max(main.boxStartY, mouse.y)
        if (Math.abs(x2 - x1) < 5 && Math.abs(y2 - y1) < 5) {
            main.selectedIds = []
        } else {
            var ids = []
            for (var i = 0; i < main.cachedComps.length; i++) {
                var c = main.cachedComps[i]
                var sp = cv.worldToScreen(c.x, c.y)
                if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2)
                    ids.push(c.id)
            }
            main.selectedIds = ids
            main.statusText = "已选中 " + ids.length + " 个元件"
        }
        main.boxSelecting = false
        cv.requestPaint()
    }

    if (main.wiring) {
        var sp2 = snapWorldPoint(main, cv, mouse)
        var endX = sp2.x
        var endY = sp2.y
        var endPort = sp2.port

        // ★ 缩短优先检查：终点落在候选线的内部 → 缩短
        var cands = main.shortenWireCandidates || []
        var didShorten = false
        for (var ci = 0; ci < cands.length; ci++) {
            var candId = cands[ci]
            var sw = main.circuitRef.getWire(candId)
            if (!sw || !sw.path || sw.path.length < 2) continue
            var inside = false
            for (var k = 0; k + 1 < sw.path.length; k++) {
                if (pointOnSegInterior(endX, endY,
                                       sw.path[k].x, sw.path[k].y,
                                       sw.path[k+1].x, sw.path[k+1].y)) {
                    inside = true
                    break
                }
            }
            if (inside) {
                main.circuitRef.shortenWireEnd(candId, endX, endY)
                main.statusText = "已缩短连线"
                didShorten = true
                break
            }
        }

        if (!didShorten) {
            var effType = resolveEffectiveWireType(main)
            var startX = main.wiringStartX
            var startY = main.wiringStartY

            if (effType === "bezier") {
                if (main.wiringFromComp === "") {
                    main.statusText = "⚠ 贝塞尔线需从端口开始"
                } else if (!endPort) {
                    main.statusText = "⚠ 贝塞尔线需拖到端口"
                } else {
                    var same = (endPort.compId === main.wiringFromComp
                                && endPort.portIdx === main.wiringFromPort
                                && endPort.isOutput === main.wiringStartIsOutput)
                    if (!same) {
                        var startIsOut = main.wiringStartIsOutput
                        var endIsOut = endPort.isOutput
                        if (startIsOut && !endIsOut) {
                            main.circuitRef.addWire(main.wiringFromComp, main.wiringFromPort,
                                                    endPort.compId, endPort.portIdx)
                            main.statusText = "已连线（贝塞尔）"
                        } else if (!startIsOut && endIsOut) {
                            main.circuitRef.addWire(endPort.compId, endPort.portIdx,
                                                    main.wiringFromComp, main.wiringFromPort)
                            main.statusText = "已连线（贝塞尔）"
                        } else {
                            main.statusText = "⚠ 需连接 输出→输入"
                        }
                    }
                }
            } else {
                // 正交：终点若无端口，尝试吸线
                if (!endPort) {
                    var endWireId = main.circuitRef.wireAtPoint(endX, endY, 15 / Math.max(0.1, main.viewScale))
                    if (endWireId && endWireId !== "") {
                        var snapEnd = snapToWire(main, endWireId, endX, endY)
                        if (snapEnd) { endX = snapEnd.x; endY = snapEnd.y }
                    }
                }
                var hasDragged = (Math.abs(endX - startX) > 0.5 ||
                                  Math.abs(endY - startY) > 0.5)
                if (hasDragged) {
                    var dir = main.wiringDirection
                    if (dir === 0) dir = 1
                    main.circuitRef.addOrthoWire(startX, startY, endX, endY, dir)
                    main.statusText = "已连线（正交）"
                }
            }
        }

        main.wiring = false
        main.wiringFromComp = ""
        main.wiringFromPort = -1
        main.wiringDirection = 0
        main.shortenWireCandidates = []
        cv.requestPaint()
    }

    cv.panning = false
    cv.draggingComp = false
    cv.dragCompId = ""
    main.draggingSelection = false
    main.circuitRef.breakUndoMerge()
}

function onCanceled(main, cv) {
    cv.panning = false
    cv.draggingComp = false
    cv.dragCompId = ""
    main.draggingSelection = false
    main.boxSelecting = false
    main.wiring = false
    main.wiringFromComp = ""
    main.wiringFromPort = -1
    main.wiringDirection = 0
    main.shortenWireCandidates = []
    main.circuitRef.breakUndoMerge()
    cv.requestPaint()
}

function onWheel(main, cv, wheel) {
    var oldScale = main.viewScale
    var factor = wheel.angleDelta.y > 0 ? 1.15 : 1/1.15
    var newScale = Math.max(0.15, Math.min(4.0, main.viewScale * factor))
    if (newScale === oldScale) return
    var mx = wheel.x, my = wheel.y
    main.viewX = mx - (mx - main.viewX) * (newScale / oldScale)
    main.viewY = my - (my - main.viewY) * (newScale / oldScale)
    main.viewScale = newScale
    main.triggerRepaint()
}

function onDoubleClicked(main, cv, mouse) {
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    for (var i = main.cachedComps.length - 1; i >= 0; i--) {
        var c = main.cachedComps[i]
        var size = cv.compSize(c)
        if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
            if (c.type === "sub") main.circuitRef.enterSubcircuit(c.subId)
            return
        }
    }
}