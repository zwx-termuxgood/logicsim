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

    // ---- 鼠标交互跨事件保存的临时状态 ----
    property real lastMouseX: 0
    property real lastMouseY: 0
    property bool panning: false
    property bool draggingComp: false
    property string dragCompId: ""

    function compSize(comp) {
        var t = comp.type
        var bw = comp.bitWidth || 1
        var rot = comp.rotation || 0
        var w0, h0
        if (t === "text") { w0 = 140; h0 = 44 }
        else if (t === "clock") { w0 = 60; h0 = 40 }
        else if (t === "led") { w0 = Math.max(50, bw * 22 + 8); h0 = 40 }
        else if (t === "input" || t === "output" || t === "const")
            { w0 = Math.max(60, bw * 22 + 8); h0 = 40 }
        else if (t === "splitter" || t === "hub") {
            var oc = (comp.outputSplits && comp.outputSplits.length) || bw
            w0 = 80; h0 = Math.max(60, oc * 24)
        }
        else if (t === "not") { w0 = 80; h0 = 60 }
        else if (t === "tgate" || t === "ntran" || t === "ptran") { w0 = 80; h0 = 60 }
        else if (t === "sub") {
            var ic = (comp.subInputNames || []).length
            var oc2 = (comp.subOutputNames || []).length
            w0 = 110; h0 = Math.max(60, Math.max(ic, oc2) * 24)
        }
        else {
            var ic2 = comp.inputCount || 2
            w0 = 80; h0 = Math.max(60, ic2 * 24)
        }
        if (rot === 1 || rot === 3) return { w: h0, h: w0 }
        return { w: w0, h: h0 }
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
