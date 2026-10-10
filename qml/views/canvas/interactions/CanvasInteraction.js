.pragma library

// ============================================================
// CanvasView 的鼠标交互逻辑
// 所有函数接收 (main, cv, event)，其中：
//   main = Main.qml 的 root 对象
//   cv   = CanvasView 的根 Item，用于访问 compSize / worldToScreen
//          / screenToWorld / portPos / bitIndexAt / requestPaint
//          / panning / draggingComp / dragCompId / lastMouseX / lastMouseY
// ============================================================

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
            main.statusText = "⚠ 无法放置：会造成子电路循环引用"
        } else {
            main.selectedCompId = newId
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

    if (editable && main.mode === "edit") {
        for (var i2 = comps.length - 1; i2 >= 0; i2--) {
            var c2 = comps[i2]
            var outs = main.outPortsById[c2.id] || []
            for (var k = 0; k < outs.length; k++) {
                var p = outs[k]
                var pp = cv.worldToScreen(c2.x + p.x, c2.y + p.y)
                var dx = mouse.x - pp.x, dy = mouse.y - pp.y
                if (dx*dx + dy*dy < 900) {
                    main.wiringFromComp = c2.id
                    main.wiringFromPort = k
                    main.wiringStartIsOutput = true
                    main.wiring = true
                    main.wireEndX = mouse.x
                    main.wireEndY = mouse.y
                    cv.requestPaint()
                    return
                }
            }
        }
        for (var i3 = comps.length - 1; i3 >= 0; i3--) {
            var c3 = comps[i3]
            var ins = main.inPortsById[c3.id] || []
            for (var k3 = 0; k3 < ins.length; k3++) {
                var p3 = ins[k3]
                var pp3 = cv.worldToScreen(c3.x + p3.x, c3.y + p3.y)
                var dx3 = mouse.x - pp3.x, dy3 = mouse.y - pp3.y
                if (dx3*dx3 + dy3*dy3 < 900) {
                    main.wiringFromComp = c3.id
                    main.wiringFromPort = k3
                    main.wiringStartIsOutput = false
                    main.wiring = true
                    main.wireEndX = mouse.x
                    main.wireEndY = mouse.y
                    cv.requestPaint()
                    return
                }
            }
        }
    }

    for (var i4 = comps.length - 1; i4 >= 0; i4--) {
        var c4 = comps[i4]
        var size4 = cv.compSize(c4)
        if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
            main.selectedCompId = c4.id
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

    main.selectedCompId = ""
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
        var comps = main.cachedComps
        var hitTo = false

        if (main.wiringStartIsOutput) {
            for (var i = comps.length - 1; i >= 0 && !hitTo; i--) {
                var c = comps[i]
                var ins = main.inPortsById[c.id] || []
                for (var k = 0; k < ins.length; k++) {
                    var p = ins[k]
                    var pp = cv.worldToScreen(c.x + p.x, c.y + p.y)
                    var dx = mouse.x - pp.x, dy = mouse.y - pp.y
                    if (dx*dx + dy*dy < 900) {
                        if (!(c.id === main.wiringFromComp && k === main.wiringFromPort)) {
                            main.circuitRef.addWire(main.wiringFromComp,
                                                    main.wiringFromPort, c.id, k)
                            main.statusText = "已连线"
                        }
                        hitTo = true
                        break
                    }
                }
            }
        } else {
            for (var i4b = comps.length - 1; i4b >= 0 && !hitTo; i4b--) {
                var c4b = comps[i4b]
                var outs = main.outPortsById[c4b.id] || []
                for (var k4b = 0; k4b < outs.length; k4b++) {
                    var p4b = outs[k4b]
                    var pp4b = cv.worldToScreen(c4b.x + p4b.x, c4b.y + p4b.y)
                    var dx4b = mouse.x - pp4b.x, dy4b = mouse.y - pp4b.y
                    if (dx4b*dx4b + dy4b*dy4b < 900) {
                        if (!(c4b.id === main.wiringFromComp && k4b === main.wiringFromPort)) {
                            main.circuitRef.addWire(c4b.id, k4b,
                                                    main.wiringFromComp,
                                                    main.wiringFromPort)
                            main.statusText = "已连线"
                        }
                        hitTo = true
                        break
                    }
                }
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
