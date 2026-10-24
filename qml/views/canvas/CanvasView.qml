import QtQuick
import LogicSim
import "Geometry.js" as Geometry
import "Painter.js" as Painter
import "interactions/CanvasInteraction.js" as CI

Item {
    id: canvasView
    property QtObject main: null
    clip: true

    property real nearestAngle: 0
    property bool hasComponents: false

    property real lastMouseX: 0
    property real lastMouseY: 0
    property bool panning: false
    property bool draggingComp: false
    property string dragCompId: ""

    property var _sizeCache: ({})

    function compSize(comp) {
        var t = comp.type
        var bw = comp.bitWidth || 1
        var rot = comp.rotation || 0
        var ic = comp.inputCount || 2
        var splits = comp.outputSplits ? comp.outputSplits.length : 0
        var subIn = comp.subInputNames ? comp.subInputNames.length : 0
        var subOut = comp.subOutputNames ? comp.subOutputNames.length : 0
        var sig = t + "|" + bw + "|" + rot + "|" + ic + "|" + splits + "|" + subIn + "|" + subOut

        var cached = _sizeCache[comp.id]
        if (cached !== undefined && cached._sig === sig) return cached

        var w0, h0
        if (t === "text") { w0 = 140; h0 = 44 }
        else if (t === "clock") { w0 = 60; h0 = 40 }
        else if (t === "led") { w0 = Math.max(50, bw * 22 + 8); h0 = 40 }
        else if (t === "input" || t === "output" || t === "const")
            { w0 = Math.max(60, bw * 22 + 8); h0 = 40 }
        else if (t === "splitter" || t === "hub") {
            var oc = splits || bw
            w0 = 80; h0 = Math.max(60, oc * 24)
        }
        else if (t === "not") { w0 = 80; h0 = 60 }
        else if (t === "tgate" || t === "ntran" || t === "ptran") { w0 = 80; h0 = 60 }
        else if (t === "sub") {
            w0 = 110; h0 = Math.max(60, Math.max(subIn, subOut) * 24)
        }
        else { w0 = 80; h0 = Math.max(60, ic * 24) }

        var result
        if (rot === 1 || rot === 3) result = { w: h0, h: w0, _sig: sig }
        else                        result = { w: w0, h: h0, _sig: sig }
        _sizeCache[comp.id] = result
        return result
    }

    function clearSizeCache() { _sizeCache = ({}) }

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

    function resolveEffType() {
        var wm = main.circuitRef.wireMode
        if (wm === "ortho") return "ortho"
        if (wm === "bezier") return "bezier"
        var cwt = main.circuitRef.currentWireType
        return (cwt === "ortho") ? "ortho" : "bezier"
    }

    // ★ 修复：颜色传播
    //   递归追上游时不仅检查"U 的末端 == w 的起点"（端点对端点），
    //   也检查"w 的起点落在 U 的任意段内部"（T 型接入）
    function computeWireSources(wires, compById, outPortsById) {
        var cache = {}

        function pointOnSegInterior(px, py, x1, y1, x2, y2) {
            if (Math.abs(y1 - y2) < 0.5) {
                if (Math.abs(py - y1) > 0.5) return false
                var lo = Math.min(x1, x2)
                var hi = Math.max(x1, x2)
                return px > lo + 0.5 && px < hi - 0.5
            }
            if (Math.abs(x1 - x2) < 0.5) {
                if (Math.abs(px - x1) > 0.5) return false
                var lo2 = Math.min(y1, y2)
                var hi2 = Math.max(y1, y2)
                return py > lo2 + 0.5 && py < hi2 - 0.5
            }
            return false
        }

        function pointOnWireInterior(px, py, path) {
            for (var i = 0; i + 1 < path.length; i++) {
                var p0 = path[i], p1 = path[i + 1]
                if (pointOnSegInterior(px, py, p0.x, p0.y, p1.x, p1.y)) return true
            }
            return false
        }

        function getSource(w, depth) {
            if (depth > 20) return "0"
            if (cache[w.id] !== undefined) return cache[w.id]

            var src = "0"

            // 1) fromComp
            if (w.fromComp && compById[w.fromComp]) {
                var fromC = compById[w.fromComp]
                var fp = w.fromPort || 0
                if (fromC.outputPorts && fromC.outputPorts.length > fp) {
                    var sv = fromC.outputPorts[fp]
                    if (sv && sv.length > 0) src = sv
                }
                cache[w.id] = src
                return src
            }

            // 2) 递归找上游 wire
            if (w.path && w.path.length > 0) {
                var startPt = w.path[0]
                for (var i = 0; i < wires.length; i++) {
                    var uw = wires[i]
                    if (uw.id === w.id) continue
                    if ((uw.wireType || "bezier") !== "ortho") continue
                    if (!uw.path || uw.path.length < 2) continue

                    var matched = false
                    // (a) 端点对端点
                    var uwLast = uw.path[uw.path.length - 1]
                    if (Math.abs(uwLast.x - startPt.x) < 0.5 &&
                        Math.abs(uwLast.y - startPt.y) < 0.5) {
                        matched = true
                    }
                    // (b) T 型接入：w 起点落在 U 的任意段内部
                    if (!matched && pointOnWireInterior(startPt.x, startPt.y, uw.path)) {
                        matched = true
                    }

                    if (matched) {
                        var s = getSource(uw, depth + 1)
                        if (s !== "0") { src = s; break }
                    }
                }
            }

            // 3) 精确匹配输出端口
            if (src === "0" && w.path && w.path.length > 0) {
                var sp = w.path[0]
                for (var cid in outPortsById) {
                    var comp = compById[cid]
                    if (!comp) continue
                    var outs = outPortsById[cid] || []
                    for (var k = 0; k < outs.length; k++) {
                        var op = outs[k]
                        var wx = comp.x + op.x
                        var wy = comp.y + op.y
                        if (Math.abs(wx - sp.x) < 0.5 && Math.abs(wy - sp.y) < 0.5) {
                            var sv2 = comp.outputPorts ? comp.outputPorts[k] : ""
                            if (sv2 && sv2.length > 0) { src = sv2; break }
                        }
                    }
                    if (src !== "0") break
                }
            }

            cache[w.id] = src
            return src
        }

        for (var i = 0; i < wires.length; i++) {
            getSource(wires[i], 0)
        }
        return cache
    }

    function computeNearestAngle() {
        var comps = main.cachedComps
        if (comps.length === 0) {
            hasComponents = false
            return
        }
        hasComponents = true
        var cx = (canvasWidth() / 2 - main.viewX) / main.viewScale
        var cy = (canvasHeight() / 2 - main.viewY) / main.viewScale
        var bestDx = 0, bestDy = 0, bestD = 1e18
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            var dx = c.x - cx, dy = c.y - cy
            var d = dx * dx + dy * dy
            if (d < bestD) { bestD = d; bestDx = dx; bestDy = dy }
        }
        nearestAngle = Math.atan2(bestDy, bestDx) * 180 / Math.PI
    }

    function jumpToNearest() {
        var comps = main.cachedComps
        if (comps.length === 0) return
        var cx = (canvasWidth() / 2 - main.viewX) / main.viewScale
        var cy = (canvasHeight() / 2 - main.viewY) / main.viewScale
        var best = null, bestD = 1e18
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            var dx = c.x - cx, dy = c.y - cy
            var d = dx * dx + dy * dy
            if (d < bestD) { bestD = d; best = c }
        }
        if (best) {
            main.viewX = canvasWidth() / 2 - best.x * main.viewScale
            main.viewY = canvasHeight() / 2 - best.y * main.viewScale
            main.triggerRepaint()
        }
    }

    function paintCanvas(ctx, W, H) {
        if (W <= 0 || H <= 0) return

        ctx.clearRect(0, 0, W, H)
        ctx.fillStyle = Theme.bg; ctx.fillRect(0, 0, W, H)

        Painter.paintGrid(ctx, W, H, main.viewScale, main.viewX, main.viewY)

        var simplify = main.viewScale < 0.35

        var wires = main.cachedWires
        var wireSources = computeWireSources(wires, main.compById, main.outPortsById)

        for (var i = 0; i < wires.length; i++) {
            Painter.drawWire(ctx, wires[i], main.compById, main.outPortsById, main.inPortsById,
                             main.viewScale, main.viewX, main.viewY, W, H, simplify,
                             main.selectedWireId, wireSources)
        }

        var dots = main.circuitRef.connectionDots()
        Painter.drawConnectionDots(ctx, dots, main.viewScale, main.viewX, main.viewY)

        if (main.wiring) {
            var startScreen = worldToScreen(main.wiringStartX, main.wiringStartY)
            var effType = resolveEffType()
            if (effType === "bezier") {
                Painter.drawPendingBezier(ctx, startScreen,
                                          main.wireEndX, main.wireEndY, main.viewScale)
            } else {
                Painter.drawPendingOrtho(ctx, startScreen, main.wiringDirection,
                                         main.wireEndX, main.wireEndY)
            }
        }

        var comps = main.cachedComps
        var visibleCount = 0
        for (var j = 0; j < comps.length; j++) {
            var comp = comps[j]
            var size = compSize(comp)
            if (!Geometry.isVisible(main.viewScale, main.viewX, main.viewY, W, H,
                                    comp.x, comp.y, size.w, size.h)) continue
            visibleCount++
            Painter.drawComponent(ctx, comp, compSize,
                                  main.viewScale, main.viewX, main.viewY,
                                  main.outPortsById, main.inPortsById,
                                  main.selectedCompId, main.selectedIds,
                                  simplify, main.circuitRef)
        }
        main.visibleComponentCount = visibleCount
        computeNearestAngle()
    }

    Canvas {
        id: canvasArea
        anchors.fill: parent
        renderStrategy: Canvas.FramebufferObject
        onPaint: canvasView.paintCanvas(getContext("2d"), width, height)
    }

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

    Rectangle {
        id: navIndicator
        visible: main.visibleComponentCount === 0 && hasComponents
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 44
        anchors.bottomMargin: 60
        width: 64; height: 64
        radius: 32
        color: navMa.pressed ? Theme.accent : Theme.bgButton
        border.color: Theme.borderStrong
        border.width: 2
        z: 200

        Text {
            anchors.centerIn: parent
            text: "➤"
            color: Theme.textWhite
            font.pixelSize: 24
            rotation: canvasView.nearestAngle
        }

        MouseArea {
            id: navMa
            anchors.fill: parent
            onClicked: canvasView.jumpToNearest()
        }
    }

    MouseArea {
        id: inputArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        onPressed: function(mouse) { CI.onPressed(main, canvasView, mouse) }
        onPositionChanged: function(mouse) { CI.onPositionChanged(main, canvasView, mouse) }
        onReleased: function(mouse) { CI.onReleased(main, canvasView, mouse) }
        onCanceled: { CI.onCanceled(main, canvasView) }
        onWheel: function(wheel) { CI.onWheel(main, canvasView, wheel) }
        onDoubleClicked: function(mouse) { CI.onDoubleClicked(main, canvasView, mouse) }
    }

    function requestPaint() { canvasArea.requestPaint() }
    function canvasWidth() { return canvasArea.width }
    function canvasHeight() { return canvasArea.height }
}