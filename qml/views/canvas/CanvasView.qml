import QtQuick
// 引入 QtQuick 基础模块：Item/Canvas/Rectangle/Text/MouseArea 等基础元素
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：Circuit 类型及其属性/方法与主题
import "Geometry.js" as Geometry
// 引入几何工具库：坐标换算、可见性判断、圆角矩形
import "Painter.js" as Painter
// 引入绘制工具库：网格、连线、圆点、元件等所有 Canvas 绘制逻辑
import "interactions/CanvasInteraction.js" as CI
// 引入交互逻辑库：鼠标按下/移动/释放/滚轮/双击的全部处理

Item {
    // 画布视图根元素：一个普通 Item 容器
    id: canvasView
    // 元素 id，供内部引用
    property QtObject main: null
    // 外部注入的主窗口对象引用：访问视图状态、缓存、电路接口
    clip: true
    // 裁剪超出部分，防止绘制/子元素溢出画布区域

    property real nearestAngle: 0
    // 最近元件相对画布中心的方位角（度），驱动导航指示器旋转
    property bool hasComponents: false
    // 是否存在元件：用于决定是否显示导航指示器

    property real lastMouseX: 0
    // 上次鼠标屏幕 x：平移画布时计算增量
    property real lastMouseY: 0
    // 上次鼠标屏幕 y：平移画布时计算增量
    property bool panning: false
    // 是否正在平移画布
    property bool draggingComp: false
    // 是否正在拖动单个元件
    property string dragCompId: ""
    // 正在拖动的元件 id

    property var _sizeCache: ({})
    // 元件尺寸缓存：key 为元件 id，value 为 {w,h,_sig}，避免每帧重复计算

    // 定义"计算元件显示尺寸"函数：按类型/位宽/旋转等估算宽高，带缓存
    function compSize(comp) {
        // 元件类型
        var t = comp.type
        // 位宽
        var bw = comp.bitWidth || 1
        // 旋转值 0..3
        var rot = comp.rotation || 0
        // 输入端口数
        var ic = comp.inputCount || 2
        // 分段数（splitter/hub）
        var splits = comp.outputSplits ? comp.outputSplits.length : 0
        // 子电路输入端口数
        var subIn = comp.subInputNames ? comp.subInputNames.length : 0
        // 子电路输出端口数
        var subOut = comp.subOutputNames ? comp.subOutputNames.length : 0
        // 尺寸签名：任一影响尺寸的字段变化即失效缓存
        var sig = t + "|" + bw + "|" + rot + "|" + ic + "|" + splits + "|" + subIn + "|" + subOut

        // 取缓存
        var cached = _sizeCache[comp.id]
        // 缓存存在且签名一致则直接返回
        if (cached !== undefined && cached._sig === sig) return cached

        // 未旋转时的宽高
        var w0, h0
        // 文本元件
        if (t === "text") { w0 = 140; h0 = 44 }
        // 时钟元件
        else if (t === "clock") { w0 = 60; h0 = 40 }
        // LED：宽度随位宽增长
        else if (t === "led") { w0 = Math.max(50, bw * 22 + 8); h0 = 40 }
        // 输入/输出/常量：宽度随位宽增长
        else if (t === "input" || t === "output" || t === "const")
            { w0 = Math.max(60, bw * 22 + 8); h0 = 40 }
        // 分线器/集线器：高度随分段数增长
        else if (t === "splitter" || t === "hub") {
            // 分段数，缺省用位宽
            var oc = splits || bw
            w0 = 80; h0 = Math.max(60, oc * 24)
        }
        // 非门：固定尺寸
        else if (t === "not") { w0 = 80; h0 = 60 }
        // 晶体管/传输门：固定尺寸
        else if (t === "tgate" || t === "ntran" || t === "ptran") { w0 = 80; h0 = 60 }
        // 子电路实例：高度随端口数增长
        else if (t === "sub") {
            w0 = 110; h0 = Math.max(60, Math.max(subIn, subOut) * 24)
        }
        // 其它门：高度随输入端口数增长
        else { w0 = 80; h0 = Math.max(60, ic * 24) }

        // 结果
        var result
        // 旋转 90/270 度：宽高互换
        if (rot === 1 || rot === 3) result = { w: h0, h: w0, _sig: sig }
        // 旋转 0/180 度：宽高不变
        else                        result = { w: w0, h: h0, _sig: sig }
        // 写入缓存
        _sizeCache[comp.id] = result
        // 返回结果
        return result
    }

    // 定义"清空尺寸缓存"函数：结构变化时调用
    function clearSizeCache() { _sizeCache = ({}) }

    // 定义"世界坐标转屏幕坐标"函数：委托 Geometry 并带入当前视图参数
    function worldToScreen(wx, wy) {
        // 换算并返回屏幕坐标
        return Geometry.worldToScreen(main.viewScale, main.viewX, main.viewY, wx, wy)
    }
    // 定义"屏幕坐标转世界坐标"函数：委托 Geometry 并带入当前视图参数
    function screenToWorld(sx, sy) {
        // 换算并返回世界坐标
        return Geometry.screenToWorld(main.viewScale, main.viewX, main.viewY, sx, sy)
    }
    // 定义"求元件某端口的世界坐标"函数
    function portPos(compId, portIdx, isOutput) {
        // 取元件
        var c = main.compById[compId]
        // 元件不存在返回原点
        if (!c) return { x: 0, y: 0 }
        // 按方向取端口几何列表
        var ports = isOutput ? main.outPortsById[compId] : main.inPortsById[compId]
        // 端口列表无效或索引越界则回退元件中心
        if (!ports || portIdx < 0 || portIdx >= ports.length) return { x: c.x, y: c.y }
        // 取端口相对偏移
        var p = ports[portIdx]
        // 返回端口世界坐标
        return { x: c.x + p.x, y: c.y + p.y }
    }
    // 定义"由世界 x 求输入元件位索引"函数：点击定位到具体位
    function bitIndexAt(comp, worldX) {
        // 元件尺寸
        var size = compSize(comp)
        // 位方块起始左边界
        var left = comp.x - size.w / 2 + 4
        // 按 22px 步长推算索引
        var i = Math.floor((worldX - left) / 22)
        // 位宽
        var bw = comp.bitWidth || 1
        // 下界钳位
        if (i < 0) i = 0
        // 上界钳位
        if (i >= bw) i = bw - 1
        // 返回索引
        return i
    }

    // 定义"综合当前生效线型"函数
    function resolveEffType() {
        // 取全局连线模式
        var wm = main.circuitRef.wireMode
        // ortho 模式强制 ortho
        if (wm === "ortho") return "ortho"
        // bezier 模式强制 bezier
        if (wm === "bezier") return "bezier"
        // switchable 模式用当前线型
        var cwt = main.circuitRef.currentWireType
        // 当前线型为 ortho 则 ortho，否则 bezier
        return (cwt === "ortho") ? "ortho" : "bezier"
    }

    // ★ 修复：颜色传播
    //   递归追上游时不仅检查"U 的末端 == w 的起点"（端点对端点），
    //   也检查"w 的起点落在 U 的任意段内部"（T 型接入）
    // 定义"计算每条连线的信号源字符串"函数：供连线着色使用
    function computeWireSources(wires, compById, outPortsById) {
        // 结果缓存：连线 id → 信号字符串
        var cache = {}

        // 内部函数：点是否严格落在正交线段内部
        function pointOnSegInterior(px, py, x1, y1, x2, y2) {
            // 水平段
            if (Math.abs(y1 - y2) < 0.5) {
                // y 不贴合则不在线上
                if (Math.abs(py - y1) > 0.5) return false
                // 区间下界
                var lo = Math.min(x1, x2)
                // 区间上界
                var hi = Math.max(x1, x2)
                // 严格落在区间内部
                return px > lo + 0.5 && px < hi - 0.5
            }
            // 垂直段
            if (Math.abs(x1 - x2) < 0.5) {
                // x 不贴合则不在线上
                if (Math.abs(px - x1) > 0.5) return false
                // 区间下界
                var lo2 = Math.min(y1, y2)
                // 区间上界
                var hi2 = Math.max(y1, y2)
                // 严格落在区间内部
                return py > lo2 + 0.5 && py < hi2 - 0.5
            }
            // 非正交段
            return false
        }

        // 内部函数：点是否落在折线任意段内部
        function pointOnWireInterior(px, py, path) {
            // 遍历相邻顶点
            for (var i = 0; i + 1 < path.length; i++) {
                // 段起终点
                var p0 = path[i], p1 = path[i + 1]
                // 命中任一段内部即返回 true
                if (pointOnSegInterior(px, py, p0.x, p0.y, p1.x, p1.y)) return true
            }
            // 未命中
            return false
        }

        // 内部函数：递归求某连线的信号源
        function getSource(w, depth) {
            // 递归深度保护
            if (depth > 20) return "0"
            // 命中缓存
            if (cache[w.id] !== undefined) return cache[w.id]

            // 默认低电平
            var src = "0"

            // 1) 起点直接绑定元件端口：取该端口输出值
            if (w.fromComp && compById[w.fromComp]) {
                // 起点元件
                var fromC = compById[w.fromComp]
                // 起点端口索引
                var fp = w.fromPort || 0
                // 端口有效则取值
                if (fromC.outputPorts && fromC.outputPorts.length > fp) {
                    var sv = fromC.outputPorts[fp]
                    if (sv && sv.length > 0) src = sv
                }
                // 写入缓存
                cache[w.id] = src
                // 返回
                return src
            }

            // 2) 无起点元件：递归找上游 wire（端点对端点或 T 型接入）
            if (w.path && w.path.length > 0) {
                // 本线起点
                var startPt = w.path[0]
                // 遍历所有连线找上游
                for (var i = 0; i < wires.length; i++) {
                    // 候选上游线
                    var uw = wires[i]
                    // 跳过自身
                    if (uw.id === w.id) continue
                    // 只考虑 ortho 线
                    if ((uw.wireType || "bezier") !== "ortho") continue
                    // 路径无效跳过
                    if (!uw.path || uw.path.length < 2) continue

                    // 是否匹配上游
                    var matched = false
                    // (a) 端点对端点：上游末顶点等于本线起点
                    var uwLast = uw.path[uw.path.length - 1]
                    if (Math.abs(uwLast.x - startPt.x) < 0.5 &&
                        Math.abs(uwLast.y - startPt.y) < 0.5) {
                        matched = true
                    }
                    // (b) T 型接入：本线起点落在上游线任意段内部
                    if (!matched && pointOnWireInterior(startPt.x, startPt.y, uw.path)) {
                        matched = true
                    }

                    // 匹配则递归取上游信号
                    if (matched) {
                        var s = getSource(uw, depth + 1)
                        if (s !== "0") { src = s; break }
                    }
                }
            }

            // 3) 仍为 0：按坐标精确匹配输出端口
            if (src === "0" && w.path && w.path.length > 0) {
                // 本线起点
                var sp = w.path[0]
                // 遍历所有元件的输出端口
                for (var cid in outPortsById) {
                    // 元件
                    var comp = compById[cid]
                    if (!comp) continue
                    // 输出端口几何
                    var outs = outPortsById[cid] || []
                    // 逐个端口比对坐标
                    for (var k = 0; k < outs.length; k++) {
                        var op = outs[k]
                        var wx = comp.x + op.x
                        var wy = comp.y + op.y
                        if (Math.abs(wx - sp.x) < 0.5 && Math.abs(wy - sp.y) < 0.5) {
                            // 命中则取该端口输出值
                            var sv2 = comp.outputPorts ? comp.outputPorts[k] : ""
                            if (sv2 && sv2.length > 0) { src = sv2; break }
                        }
                    }
                    // 已取到非 0 值则停止
                    if (src !== "0") break
                }
            }

            // 写入缓存并返回
            cache[w.id] = src
            return src
        }

        // 对每条连线求一次信号源，填充缓存
        for (var i = 0; i < wires.length; i++) {
            getSource(wires[i], 0)
        }
        // 返回缓存
        return cache
    }

    // 定义"计算最近元件方位角"函数：驱动导航指示器
    function computeNearestAngle() {
        // 元件列表
        var comps = main.cachedComps
        // 无元件则标记无元件并返回
        if (comps.length === 0) {
            hasComponents = false
            return
        }
        // 有元件
        hasComponents = true
        // 画布中心世界 x
        var cx = (canvasWidth() / 2 - main.viewX) / main.viewScale
        // 画布中心世界 y
        var cy = (canvasHeight() / 2 - main.viewY) / main.viewScale
        // 最近元件的偏移与距离
        var bestDx = 0, bestDy = 0, bestD = 1e18
        // 遍历元件
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            var dx = c.x - cx, dy = c.y - cy
            var d = dx * dx + dy * dy
            // 更近则更新
            if (d < bestD) { bestD = d; bestDx = dx; bestDy = dy }
        }
        // 换算成角度
        nearestAngle = Math.atan2(bestDy, bestDx) * 180 / Math.PI
    }

    // 定义"跳转到最近元件"函数：把视图中心移到最近元件上
    function jumpToNearest() {
        // 元件列表
        var comps = main.cachedComps
        // 无元件则返回
        if (comps.length === 0) return
        // 画布中心世界坐标
        var cx = (canvasWidth() / 2 - main.viewX) / main.viewScale
        var cy = (canvasHeight() / 2 - main.viewY) / main.viewScale
        // 最近元件与距离
        var best = null, bestD = 1e18
        // 遍历找最近
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            var dx = c.x - cx, dy = c.y - cy
            var d = dx * dx + dy * dy
            if (d < bestD) { bestD = d; best = c }
        }
        // 找到则调整视图偏移使其居中
        if (best) {
            main.viewX = canvasWidth() / 2 - best.x * main.viewScale
            main.viewY = canvasHeight() / 2 - best.y * main.viewScale
            // 请求重绘
            main.triggerRepaint()
        }
    }

    // 定义"绘制整个画布"函数：Canvas onPaint 回调
    function paintCanvas(ctx, W, H) {
        // 尺寸无效则不画
        if (W <= 0 || H <= 0) return

        // 清空整块画布
        ctx.clearRect(0, 0, W, H)
        // 填充背景色
        ctx.fillStyle = Theme.bg; ctx.fillRect(0, 0, W, H)

        // 绘制背景网格
        Painter.paintGrid(ctx, W, H, main.viewScale, main.viewX, main.viewY)

        // 是否简化绘制（缩放很小）
        var simplify = main.viewScale < 0.35

        // 连线列表
        var wires = main.cachedWires
        // 预计算每条连线的信号源字符串
        var wireSources = computeWireSources(wires, main.compById, main.outPortsById)

        // 逐条绘制连线
        for (var i = 0; i < wires.length; i++) {
            Painter.drawWire(ctx, wires[i], main.compById, main.outPortsById, main.inPortsById,
                             main.viewScale, main.viewX, main.viewY, W, H, simplify,
                             main.selectedWireId, wireSources)
        }

        // 取连接圆点
        var dots = main.circuitRef.connectionDots()
        // 绘制连接圆点
        Painter.drawConnectionDots(ctx, dots, main.viewScale, main.viewX, main.viewY)

        // 正在拉线则画预览
        if (main.wiring) {
            // 起点屏幕坐标
            var startScreen = worldToScreen(main.wiringStartX, main.wiringStartY)
            // 生效线型
            var effType = resolveEffType()
            // 贝塞尔预览
            if (effType === "bezier") {
                Painter.drawPendingBezier(ctx, startScreen,
                                          main.wireEndX, main.wireEndY, main.viewScale)
            // 正交预览
            } else {
                Painter.drawPendingOrtho(ctx, startScreen, main.wiringDirection,
                                         main.wireEndX, main.wireEndY)
            }
        }

        // 元件列表
        var comps = main.cachedComps
        // 可见元件计数
        var visibleCount = 0
        // 逐个绘制元件
        for (var j = 0; j < comps.length; j++) {
            // 当前元件
            var comp = comps[j]
            // 元件尺寸
            var size = compSize(comp)
            // 不可见则跳过
            if (!Geometry.isVisible(main.viewScale, main.viewX, main.viewY, W, H,
                                    comp.x, comp.y, size.w, size.h)) continue
            // 计数 +1
            visibleCount++
            // 绘制元件
            Painter.drawComponent(ctx, comp, compSize,
                                  main.viewScale, main.viewX, main.viewY,
                                  main.outPortsById, main.inPortsById,
                                  main.selectedCompId, main.selectedIds,
                                  simplify, main.circuitRef)
        }
        // 回写可见元件数
        main.visibleComponentCount = visibleCount
        // 更新导航指示器角度
        computeNearestAngle()
    }

    Canvas {
        // 实际绘制的 Canvas 元素
        id: canvasArea
        // 元素 id
        anchors.fill: parent
        // 填满视图
        renderStrategy: Canvas.FramebufferObject
        // 使用 FBO 渲染，重绘性能更好
        onPaint: canvasView.paintCanvas(getContext("2d"), width, height)
        // 绘制回调：把 2D 上下文交给 paintCanvas
    }

    Rectangle {
        // 框选选择框
        id: selectionBox
        // 元素 id
        visible: main.boxSelecting
        // 仅框选时可见
        x: Math.min(main.boxStartX, main.boxCurrentX)
        // 框左上角 x
        y: Math.min(main.boxStartY, main.boxCurrentY)
        // 框左上角 y
        width: Math.abs(main.boxCurrentX - main.boxStartX)
        // 框宽
        height: Math.abs(main.boxCurrentY - main.boxStartY)
        // 框高
        color: "#2058a6ff"
        // 半透明蓝色填充
        border.color: Theme.accent
        // 边框用强调色
        border.width: 1
        // 边框宽度
        z: 100
        // 层级较高，显示在画布之上
    }

    Rectangle {
        // 导航指示器：当前视野内无元件时显示，点击跳到最近元件
        id: navIndicator
        // 元素 id
        visible: main.visibleComponentCount === 0 && hasComponents
        // 视野内无可见元件但存在元件时显示
        anchors.right: parent.right
        // 右侧对齐
        anchors.bottom: parent.bottom
        // 底部对齐
        anchors.rightMargin: 44
        // 右外边距
        anchors.bottomMargin: 60
        // 下外边距
        width: 64; height: 64
        // 尺寸
        radius: 32
        // 半径等于半宽，呈圆形
        color: navMa.pressed ? Theme.accent : Theme.bgButton
        // 颜色：按下强调色 > 普通按钮色
        border.color: Theme.borderStrong
        // 边框色
        border.width: 2
        // 边框宽度
        z: 200
        // 层级最高

        Text {
            // 指示器箭头文字
            anchors.centerIn: parent
            // 居中
            text: "➤"
            // 箭头符号
            color: Theme.textWhite
            // 白色
            font.pixelSize: 24
            // 字号
            rotation: canvasView.nearestAngle
            // 按最近元件方位角旋转
        }

        MouseArea {
            // 点击区
            id: navMa
            // 点击区 id
            anchors.fill: parent
            // 填满指示器
            onClicked: canvasView.jumpToNearest()
            // 点击跳到最近元件
        }
    }

    MouseArea {
        // 画布鼠标交互区：把各种鼠标事件转发给 CanvasInteraction.js
        id: inputArea
        // 交互区 id
        anchors.fill: parent
        // 填满视图
        acceptedButtons: Qt.LeftButton
        // 只接受左键
        hoverEnabled: true
        // 启用悬停事件，保证移动事件持续到达

        onPressed: function(mouse) { CI.onPressed(main, canvasView, mouse) }
        // 按下：交给交互库处理
        onPositionChanged: function(mouse) { CI.onPositionChanged(main, canvasView, mouse) }
        // 移动：交给交互库处理
        onReleased: function(mouse) { CI.onReleased(main, canvasView, mouse) }
        // 释放：交给交互库处理
        onCanceled: { CI.onCanceled(main, canvasView) }
        // 取消：交给交互库清理状态
        onWheel: function(wheel) { CI.onWheel(main, canvasView, wheel) }
        // 滚轮：交给交互库处理缩放
        onDoubleClicked: function(mouse) { CI.onDoubleClicked(main, canvasView, mouse) }
        // 双击：交给交互库处理（如进入子电路）
    }

    // 定义"请求重绘"函数：供外部调用触发 Canvas 重绘
    function requestPaint() { canvasArea.requestPaint() }
    // 定义"取画布宽"函数
    function canvasWidth() { return canvasArea.width }
    // 定义"取画布高"函数
    function canvasHeight() { return canvasArea.height }
}