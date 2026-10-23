.pragma library

// ============================================================
// 端口命中检测：返回鼠标下最近的端口
//   { compId, portIdx, isOutput } 或 null
// 任意端口（输入 or 输出）都可作为起点或终点
// ============================================================
function findPortAt(main, cv, sx, sy) {
    var comps = main.cachedComps
    var best = null
    var bestDist = 900   // 30px 半径
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
        if (newId === "") {
            main.statusText = "⚠ 无法放置"
        } else {
            main.selectedCompId = newId
            main.selectedSubId = ""      // ★ 选中元件时清空子电路选中
            main.refreshSel()
        }
        main.placingType = ""
        main.placingSubId = ""
        return
    }

    var comps = main.cachedComps

    if (main.mode === "select") {
        for (var i = comps.length - 1; i >= 0; i--) {
            var c = comps[i]
            var size = cv.compSize(c)
            if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                if (main.selectedIds.indexOf(c.id) < 0) main.selectedIds = [c.id]
                main.circuitRef.breakUndoMerge()
                main.draggingSelection = true
                main.lastDragWorldX = wp.x
                main.lastDragWorldY = wp.y
                cv.requestPaint()
                return
            }
        }
        main.selectedIds = []
        main.boxSelecting = true
        main.boxStartX = mouse.x; main.boxStartY = mouse.y
        main.boxCurrentX = mouse.x; main.boxCurrentY = mouse.y
        cv.requestPaint()
        return
    }

    // 编辑模式：任意端口都能开始拖拽
    if (editable && main.mode === "edit") {
        var port = findPortAt(main, cv, mouse.x, mouse.y)
        if (port) {
            main.wiringFromComp = port.compId
            main.wiringFromPort = port.portIdx
            main.wiringStartIsOutput = port.isOutput
            main.wiring = true
            main.wireEndX = mouse.x
            main.wireEndY = mouse.y
            cv.requestPaint()
            return
        }
    }

    for (var i4 = comps.length - 1; i4 >= 0; i4--) {
        var c4 = comps[i4]
        var size4 = cv.compSize(c4)
        if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
            main.selectedCompId = c4.id
            main.selectedSubId = ""      // ★ 选中元件时清空子电路选中
            main.refreshSel()
            if (editable && main.mode === "control") {
                if (c4.type === "input") {
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

    // ★ 点击空白区域：清空元件选中 + 子电路选中
    main.selectedCompId = ""
    main.selectedSubId = ""
    main.refreshSel()
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
        if (Math.abs(dwx) > 0.01 || Math.abs(dwy) > 0.01) {
            main.circuitRef.moveSelection(main.selectedIds, dwx, dwy)
            main.lastDragWorldX = wp.x
            main.lastDragWorldY = wp.y
        }
        return
    }
    if (main.wiring) {
        main.wireEndX = mouse.x; main.wireEndY = mouse.y
        cv.requestPaint()
        return
    }
    if (cv.draggingComp && cv.dragCompId !== "" && !main.circuitViewOnly) {
        var wp2 = cv.screenToWorld(mouse.x, mouse.y)
        main.circuitRef.moveComponent(cv.dragCompId, wp2.x, wp2.y)
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
            var comps5 = main.cachedComps
            for (var i = 0; i < comps5.length; i++) {
                var c = comps5[i]
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
        var port = findPortAt(main, cv, mouse.x, mouse.y)
        if (port) {
            var startComp = main.wiringFromComp
            var startPort = main.wiringFromPort
            var startIsOut = main.wiringStartIsOutput
            var same = (port.compId === startComp
                        && port.portIdx === startPort
                        && port.isOutput === startIsOut)
            if (!same) {
                if (startIsOut) {
                    main.circuitRef.addWire(startComp, startPort,
                                            port.compId, port.portIdx)
                } else {
                    main.circuitRef.addWire(port.compId, port.portIdx,
                                            startComp, startPort)
                }
                main.statusText = "已连线"
            }
        }
        main.wiring = false
        main.wiringFromComp = ""
        main.wiringFromPort = -1
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
    var comps6 = main.cachedComps
    for (var i = comps6.length - 1; i >= 0; i--) {
        var c = comps6[i]
        var size = cv.compSize(c)
        if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
            if (c.type === "sub") main.circuitRef.enterSubcircuit(c.subId)
            return
        }
    }
}