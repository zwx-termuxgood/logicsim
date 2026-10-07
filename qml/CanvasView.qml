import QtQuick
import LogicSim
import "Geometry.js" as Geometry
import "CanvasPainter.js" as Painter

Item {
    id: canvasView
    property QtObject main: null
    clip: true

    // ============ 内部工具函数 ============
    function compSize(comp) {
        var t = comp.type
        var bw = comp.bitWidth || 1
        if (t === "text") return { w: 140, h: 44 }
        if (t === "clock") return { w: 60, h: 40 }
        if (t === "led") return { w: Math.max(50, bw * 22 + 8), h: 40 }
        if (t === "input" || t === "output") return { w: Math.max(60, bw * 22 + 8), h: 40 }
        if (t === "splitter" || t === "hub") {
            var oc = (comp.outputSplits && comp.outputSplits.length) || bw
            return { w: 80, h: Math.max(60, oc * 24) }
        }
        if (t === "not") return { w: 80, h: 60 }
        if (t === "tgate" || t === "ntran" || t === "ptran")
            return { w: 80, h: Math.max(60, 2 * 24) }
        if (t === "sub") {
            var ic = (comp.subInputNames || []).length
            var oc2 = (comp.subOutputNames || []).length
            return { w: 110, h: Math.max(60, Math.max(ic, oc2) * 24) }
        }
        var ic2 = comp.inputCount || 2
        return { w: 80, h: Math.max(60, ic2 * 24) }
    }

    function worldToScreen(wx, wy) {
        return Geometry.worldToScreen(main.viewScale, main.viewX, main.viewY, wx, wy)
    }
    function screenToWorld(sx, sy) {
        return Geometry.screenToWorld(main.viewScale, main.viewX, main.viewY, sx, sy)
    }
    function portPos(compId, portIdx, isOutput) {
        var c = main.compById[compId]
        if (!c) return { x: 0, y: 0 }
        var ports = isOutput ? main.outPortsById[compId] : main.inPortsById[compId]
        if (!ports || portIdx < 0 || portIdx >= ports.length) return { x: c.x, y: c.y }
        var p = ports[portIdx]
        return { x: c.x + p.x, y: c.y + p.y }
    }
    function bitIndexAt(comp, worldX) {
        var size = compSize(comp)
        var left = comp.x - size.w / 2 + 4
        var i = Math.floor((worldX - left) / 22)
        var bw = comp.bitWidth || 1
        if (i < 0) i = 0
        if (i >= bw) i = bw - 1
        return i
    }

    // ============ 绘制 ============
    function paintCanvas(ctx, W, H) {
        if (W <= 0 || H <= 0) return

        ctx.clearRect(0, 0, W, H)
        ctx.fillStyle = Theme.bg; ctx.fillRect(0, 0, W, H)

        Painter.paintGrid(ctx, W, H, main.viewScale, main.viewX, main.viewY)

        var simplify = main.viewScale < 0.35

        var wires = main.cachedWires
        for (var i = 0; i < wires.length; i++) {
            Painter.drawWire(ctx, wires[i], main.compById, main.outPortsById, main.inPortsById,
                             main.viewScale, main.viewX, main.viewY, W, H, simplify)
        }

        if (main.wiring) {
            var p = portPos(main.wiringFromComp, main.wiringFromPort, main.wiringStartIsOutput)
            var sp = worldToScreen(p.x, p.y)
            Painter.drawPendingWire(ctx, sp, main.wiringStartIsOutput,
                                    main.wireEndX, main.wireEndY)
        }

        var comps = main.cachedComps
        for (var j = 0; j < comps.length; j++) {
            var comp = comps[j]
            var size = compSize(comp)
            if (!Geometry.isVisible(main.viewScale, main.viewX, main.viewY, W, H,
                                    comp.x, comp.y, size.w, size.h)) continue
            Painter.drawComponent(ctx, comp, compSize,
                                  main.viewScale, main.viewX, main.viewY,
                                  main.outPortsById, main.inPortsById,
                                  main.selectedCompId, main.selectedIds,
                                  simplify, main.circuitRef)
        }
    }

    // ============ 画布 ============
    Canvas {
        id: canvasArea
        anchors.fill: parent
        renderStrategy: Canvas.Immediate
        onPaint: canvasView.paintCanvas(getContext("2d"), width, height)
    }

    // ============ 框选矩形 ============
    Rectangle {
        id: selectionBox
        visible: main.boxSelecting
        x: Math.min(main.boxStartX, main.boxCurrentX)
        y: Math.min(main.boxStartY, main.boxCurrentY)
        width: Math.abs(main.boxCurrentX - main.boxStartX)
        height: Math.abs(main.boxCurrentY - main.boxStartY)
        color: "#2058a6ff"
        border.color: Theme.accent
        border.width: 1
        z: 100
    }

    // ============ 鼠标交互 ============
    MouseArea {
        id: inputArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        property real lastMouseX: 0
        property real lastMouseY: 0
        property bool panning: false
        property bool draggingComp: false
        property string dragCompId: ""

        onPressed: function(mouse) {
            lastMouseX = mouse.x; lastMouseY = mouse.y
            var wp = screenToWorld(mouse.x, mouse.y)
            var editable = !main.circuitViewOnly

            // 放置模式
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

            // 选择模式
            if (main.mode === "select") {
                for (var i = comps.length - 1; i >= 0; i--) {
                    var c = comps[i]
                    var size = compSize(c)
                    if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                        if (main.selectedIds.indexOf(c.id) < 0) main.selectedIds = [c.id]
                        main.circuitRef.breakUndoMerge()
                        main.draggingSelection = true
                        main.lastDragWorldX = wp.x
                        main.lastDragWorldY = wp.y
                        canvasArea.requestPaint()
                        return
                    }
                }
                main.selectedIds = []
                main.boxSelecting = true
                main.boxStartX = mouse.x; main.boxStartY = mouse.y
                main.boxCurrentX = mouse.x; main.boxCurrentY = mouse.y
                canvasArea.requestPaint()
                return
            }

            // 编辑模式：端口检测
            if (editable && main.mode === "edit") {
                for (var i2 = comps.length - 1; i2 >= 0; i2--) {
                    var c2 = comps[i2]
                    var outs = main.outPortsById[c2.id] || []
                    for (var k = 0; k < outs.length; k++) {
                        var p = outs[k]
                        var pp = worldToScreen(c2.x + p.x, c2.y + p.y)
                        var dx = mouse.x - pp.x, dy = mouse.y - pp.y
                        if (dx*dx + dy*dy < 900) {
                            main.wiringFromComp = c2.id
                            main.wiringFromPort = k
                            main.wiringStartIsOutput = true
                            main.wiring = true
                            main.wireEndX = mouse.x
                            main.wireEndY = mouse.y
                            canvasArea.requestPaint()
                            return
                        }
                    }
                }
                for (var i3 = comps.length - 1; i3 >= 0; i3--) {
                    var c3 = comps[i3]
                    var ins = main.inPortsById[c3.id] || []
                    for (var k3 = 0; k3 < ins.length; k3++) {
                        var p3 = ins[k3]
                        var pp3 = worldToScreen(c3.x + p3.x, c3.y + p3.y)
                        var dx3 = mouse.x - pp3.x, dy3 = mouse.y - pp3.y
                        if (dx3*dx3 + dy3*dy3 < 900) {
                            main.wiringFromComp = c3.id
                            main.wiringFromPort = k3
                            main.wiringStartIsOutput = false
                            main.wiring = true
                            main.wireEndX = mouse.x
                            main.wireEndY = mouse.y
                            canvasArea.requestPaint()
                            return
                        }
                    }
                }
            }

            // 元件命中
            for (var i4 = comps.length - 1; i4 >= 0; i4--) {
                var c4 = comps[i4]
                var size4 = compSize(c4)
                if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
                    main.selectedCompId = c4.id
                    main.refreshSel()
                    if (editable && main.mode === "control") {
                        if (c4.type === "input") {
                            var base = c4.displayBase || "bin"
                            if (base === "bin") {
                                var bitIdx = bitIndexAt(c4, wp.x)
                                var bits = c4.inputBits || []
                                main.circuitRef.setBitValue(c4.id, bitIdx, !(bits[bitIdx] || false))
                            }
                        }
                    } else if (editable && main.mode === "edit") {
                        main.circuitRef.breakUndoMerge()
                        draggingComp = true
                        dragCompId = c4.id
                    }
                    return
                }
            }

            main.selectedCompId = ""
            main.refreshSel()
            panning = true
        }

        onPositionChanged: function(mouse) {
            if (main.boxSelecting) {
                main.boxCurrentX = mouse.x; main.boxCurrentY = mouse.y
                return
            }
            if (main.draggingSelection) {
                var wp = screenToWorld(mouse.x, mouse.y)
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
                canvasArea.requestPaint()
                return
            }
            if (draggingComp && dragCompId !== "" && !main.circuitViewOnly) {
                var wp2 = screenToWorld(mouse.x, mouse.y)
                main.circuitRef.moveComponent(dragCompId, wp2.x, wp2.y)
                return
            }
            if (panning) {
                main.viewX += mouse.x - lastMouseX
                main.viewY += mouse.y - lastMouseY
                lastMouseX = mouse.x; lastMouseY = mouse.y
                canvasArea.requestPaint()
            }
        }

        onReleased: function(mouse) {
            // 完成框选
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
                        var sp = worldToScreen(c.x, c.y)
                        if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2)
                            ids.push(c.id)
                    }
                    main.selectedIds = ids
                    main.statusText = "已选中 " + ids.length + " 个元件"
                }
                main.boxSelecting = false
                canvasArea.requestPaint()
            }

            // 完成连线
            if (main.wiring) {
                var comps = main.cachedComps
                var hitTo = false

                if (main.wiringStartIsOutput) {
                    for (var i = comps.length - 1; i >= 0 && !hitTo; i--) {
                        var c = comps[i]
                        var ins = main.inPortsById[c.id] || []
                        for (var k = 0; k < ins.length; k++) {
                            var p = ins[k]
                            var pp = worldToScreen(c.x + p.x, c.y + p.y)
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
                            var pp4b = worldToScreen(c4b.x + p4b.x, c4b.y + p4b.y)
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
                canvasArea.requestPaint()
            }

            panning = false
            draggingComp = false
            dragCompId = ""
            main.draggingSelection = false
            main.circuitRef.breakUndoMerge()
        }

        onCanceled: function() {
            panning = false
            draggingComp = false
            dragCompId = ""
            main.draggingSelection = false
            main.boxSelecting = false
            main.wiring = false
            main.wiringFromComp = ""
            main.wiringFromPort = -1
            main.circuitRef.breakUndoMerge()
            canvasArea.requestPaint()
        }

        onWheel: function(wheel) {
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

        onDoubleClicked: function(mouse) {
            var wp = screenToWorld(mouse.x, mouse.y)
            var comps6 = main.cachedComps
            for (var i = comps6.length - 1; i >= 0; i--) {
                var c = comps6[i]
                var size = compSize(c)
                if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                    if (c.type === "sub") main.circuitRef.enterSubcircuit(c.subId)
                    return
                }
            }
        }
    }

    function requestPaint() { canvasArea.requestPaint() }
    function canvasWidth() { return canvasArea.width }
    function canvasHeight() { return canvasArea.height }
}