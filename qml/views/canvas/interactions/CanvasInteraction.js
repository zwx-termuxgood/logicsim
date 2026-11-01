.pragma library
// 声明为共享 JS 库：供多个 QML 文件 import 使用，不绑定到某个 QML 实例

// 定义"在屏幕坐标附近查找最近端口"辅助函数
function findPortAt(main, cv, sx, sy) {
    // 取缓存的元件列表
    var comps = main.cachedComps
    // 最佳命中结果，初值 null
    var best = null
    // 最佳距离平方，初值 900（即 30px 容差的平方）
    var bestDist = 900
    // 遍历所有元件
    for (var i = 0; i < comps.length; i++) {
        // 当前元件
        var c = comps[i]
        // 该元件的输出端口几何列表
        var outs = main.outPortsById[c.id] || []
        // 遍历输出端口
        for (var k = 0; k < outs.length; k++) {
            // 端口相对偏移
            var p = outs[k]
            // 端口屏幕坐标
            var pp = cv.worldToScreen(c.x + p.x, c.y + p.y)
            // 鼠标与端口屏幕偏移
            var dx = sx - pp.x, dy = sy - pp.y
            // 距离平方
            var d = dx*dx + dy*dy
            // 更近则更新最佳
            if (d < bestDist) {
                // 更新最佳距离
                bestDist = d
                // 记录命中结果（元件 id、端口索引、是否输出）
                best = { compId: c.id, portIdx: k, isOutput: true }
            }
        }
        // 该元件的输入端口几何列表
        var ins = main.inPortsById[c.id] || []
        // 遍历输入端口
        for (var k2 = 0; k2 < ins.length; k2++) {
            // 端口相对偏移
            var p2 = ins[k2]
            // 端口屏幕坐标
            var pp2 = cv.worldToScreen(c.x + p2.x, c.y + p2.y)
            // 鼠标与端口屏幕偏移
            var dx2 = sx - pp2.x, dy2 = sy - pp2.y
            // 距离平方
            var d2 = dx2*dx2 + dy2*dy2
            // 更近则更新最佳
            if (d2 < bestDist) {
                // 更新最佳距离
                bestDist = d2
                // 记录命中结果（输入端口）
                best = { compId: c.id, portIdx: k2, isOutput: false }
            }
        }
    }
    // 返回最佳命中（未命中为 null）
    return best
}

// 定义"综合出当前生效线型"辅助函数
function resolveEffectiveWireType(main) {
    // 取全局连线模式
    var wm = main.circuitRef.wireMode
    // ortho 模式强制 ortho
    if (wm === "ortho") return "ortho"
    // bezier 模式强制 bezier
    if (wm === "bezier") return "bezier"
    // switchable 模式用当前线型
    var cwt = main.circuitRef.currentWireType
    // 当前线型为 ortho 则返回 ortho，否则返回 bezier
    return (cwt === "ortho") ? "ortho" : "bezier"
}

// 定义"把鼠标位置吸附到端口或网格"辅助函数
function snapWorldPoint(main, cv, mouse) {
    // 先尝试命中端口
    var port = findPortAt(main, cv, mouse.x, mouse.y)
    // 命中端口：返回端口世界坐标
    if (port) {
        // 取端口世界坐标
        var ep = cv.portPos(port.compId, port.portIdx, port.isOutput)
        // 返回坐标与命中端口信息
        return { x: ep.x, y: ep.y, port: port }
    }
    // 未命中端口：世界坐标按 10px 网格取整
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    // 返回取整后的世界坐标，port 为 null 表示未命中端口
    return {
        // x 网格化到 10px
        x: Math.round(wp.x / 10) * 10,
        // y 网格化到 10px
        y: Math.round(wp.y / 10) * 10,
        // 未命中端口
        port: null
    }
}

// 定义"把点吸附到某根 ortho 线的最近点上"辅助函数
function snapToWire(main, wireId, wx, wy) {
    // 取连线 map
    var w = main.circuitRef.getWire(wireId)
    // 不是有效 ortho 线则返回 null
    if (!w || !w.path || w.path.length < 2) return null
    // 取路径
    var path = w.path
    // 最近点 x 坐标
    var bestX = 0, bestY = 0, bestD = 1e18
    // 最近点所在段是否水平
    var bestIsHorizontal = false
    // 遍历每段求点到线段最近点
    for (var i = 0; i + 1 < path.length; i++) {
        // 段起点
        var p0 = path[i], p1 = path[i + 1]
        // 段两端坐标
        var x1 = p0.x, y1 = p0.y, x2 = p1.x, y2 = p1.y
        // 段方向向量
        var dx = x2 - x1, dy = y2 - y1
        // 段长度平方
        var len2 = dx*dx + dy*dy
        // 投影参数
        var t = 0
        // 非退化段才计算投影
        if (len2 > 1e-9) {
            // 点在直线上的投影参数
            t = ((wx - x1) * dx + (wy - y1) * dy) / len2
            // 钳到 [0,1]，限制在线段内
            if (t < 0) t = 0; else if (t > 1) t = 1
        }
        // 段上最近点
        var cx = x1 + t * dx, cy = y1 + t * dy
        // 距离平方
        var d = (wx - cx) * (wx - cx) + (wy - cy) * (wy - cy)
        // 更近则更新
        if (d < bestD) {
            // 更新最佳距离
            bestD = d
            // 更新最近点坐标
            bestX = cx
            bestY = cy
            // 判断该段是否水平
            bestIsHorizontal = Math.abs(y1 - y2) < 0.5
        }
    }
    // 水平段：x 网格化并限制在该段区间内
    if (bestIsHorizontal) {
        // x 网格化到 10px
        var snappedX = Math.round(bestX / 10) * 10
        // 路径 x 区间下界
        var lo = Math.min(path[0].x, path[path.length-1].x)
        // 路径 x 区间上界
        var hi = Math.max(path[0].x, path[path.length-1].x)
        // 下界钳位
        if (snappedX < lo) snappedX = lo
        // 上界钳位
        if (snappedX > hi) snappedX = hi
        // 返回吸附点
        return { x: snappedX, y: bestY }
    // 垂直段：y 网格化并限制在该段区间内
    } else {
        // y 网格化到 10px
        var snappedY = Math.round(bestY / 10) * 10
        // 路径 y 区间下界
        var lo2 = Math.min(path[0].y, path[path.length-1].y)
        // 路径 y 区间上界
        var hi2 = Math.max(path[0].y, path[path.length-1].y)
        // 下界钳位
        if (snappedY < lo2) snappedY = lo2
        // 上界钳位
        if (snappedY > hi2) snappedY = hi2
        // 返回吸附点
        return { x: bestX, y: snappedY }
    }
}

// 定义"更新正交连线预览的拐角方向"辅助函数
function updateWiringDirection(main, sx, sy, wx, wy) {
    // 方向常量：1 横优先，2 竖优先
    var H = 1, V = 2
    // 起终点是否同 x
    var sameX = Math.abs(wx - sx) < 0.5
    // 起终点是否同 y
    var sameY = Math.abs(wy - sy) < 0.5
    // 当前方向
    var dir = main.wiringDirection
    // 尚未确定方向：按位移主分量确定
    if (dir === 0) {
        // 横向有位移则横优先
        if (!sameX) dir = H
        // 否则纵向有位移则竖优先
        else if (!sameY) dir = V
    // 当前横优先但已同 x：若也同 y 则复位，否则切竖优先
    } else if (dir === H && sameX) {
        // 完全重合则复位
        if (sameY) dir = 0
        // 否则切换为竖优先
        else dir = V
    // 当前竖优先但已同 y：若也同 x 则复位，否则切横优先
    } else if (dir === V && sameY) {
        // 完全重合则复位
        if (sameX) dir = 0
        // 否则切换为横优先
        else dir = H
    }
    // 写回方向
    main.wiringDirection = dir
}

// 定义"点是否严格落在正交线段内部"辅助函数（容差 2.0，吸收缩放误差）
function pointOnSegInterior(px, py, x1, y1, x2, y2) {
    // 内部判断容差
    var tol = 2.0
    // 水平段：y 需贴合，x 需严格在区间内部
    if (Math.abs(y1 - y2) < 0.5) {
        // 点 y 与线段 y 差超过容差则不在线上
        if (Math.abs(py - y1) > 0.5) return false
        // 线段 x 区间下界
        var lo = Math.min(x1, x2)
        // 线段 x 区间上界
        var hi = Math.max(x1, x2)
        // 点 x 必须严格落在区间内部
        return px > lo + tol && px < hi - tol
    }
    // 垂直段：x 需贴合，y 需严格在区间内部
    if (Math.abs(x1 - x2) < 0.5) {
        // 点 x 与线段 x 差超过容差则不在线上
        if (Math.abs(px - x1) > 0.5) return false
        // 线段 y 区间下界
        var lo2 = Math.min(y1, y2)
        // 线段 y 区间上界
        var hi2 = Math.max(y1, y2)
        // 点 y 必须严格落在区间内部
        return py > lo2 + tol && py < hi2 - tol
    }
    // 非正交段不做判断
    return false
}

// 定义"查找起点处可缩短的连线"辅助函数：起点落在某线端点附近即为其缩短候选
function findShortenTargetAt(main, wx, wy) {
    // 候选连线 id 列表
    var wids = []
    // 遍历所有缓存连线
    for (var i = 0; i < main.cachedWires.length; i++) {
        // 当前连线
        var w = main.cachedWires[i]
        // 只处理 ortho 线
        if ((w.wireType || "bezier") !== "ortho") continue
        // 无效路径跳过
        if (!w.path || w.path.length < 2) continue
        // 首顶点
        var p0 = w.path[0]
        // 末顶点
        var pn = w.path[w.path.length - 1]
        // 起点与首/末顶点在 3px 内重合则记为候选
        if ((Math.abs(p0.x - wx) < 3 && Math.abs(p0.y - wy) < 3) ||
            (Math.abs(pn.x - wx) < 3 && Math.abs(pn.y - wy) < 3)) {
            // 加入候选列表
            wids.push(w.id)
        }
    }
    // 返回候选列表
    return wids
}

// 定义"从端口开始拉线"辅助函数
function startWiringFromPort(main, cv, onPort) {
    // 起点世界坐标
    var startPt = cv.portPos(onPort.compId, onPort.portIdx, onPort.isOutput)
    // 记录起点元件
    main.wiringFromComp = onPort.compId
    // 记录起点端口
    main.wiringFromPort = onPort.portIdx
    // 记录起点是否为输出端口
    main.wiringStartIsOutput = onPort.isOutput
    // 进入拉线状态
    main.wiring = true
    // 记录起点世界 x
    main.wiringStartX = startPt.x
    // 记录起点世界 y
    main.wiringStartY = startPt.y
    // 起点屏幕坐标
    var ss = cv.worldToScreen(startPt.x, startPt.y)
    // 预览终点 x 初始化为起点
    main.wireEndX = ss.x
    // 预览终点 y 初始化为起点
    main.wireEndY = ss.y
    // 方向复位
    main.wiringDirection = 0
    // 清空选中连线
    main.selectedWireId = ""
}

// 定义"从连线中间开始拉线（T 型接入）"辅助函数
function startWiringFromWire(main, cv, snapPt) {
    // 无起点元件
    main.wiringFromComp = ""
    // 无起点端口
    main.wiringFromPort = -1
    // 视为输出侧起步（从线中间引出等价于从网络引出）
    main.wiringStartIsOutput = true
    // 进入拉线状态
    main.wiring = true
    // 记录起点世界 x
    main.wiringStartX = snapPt.x
    // 记录起点世界 y
    main.wiringStartY = snapPt.y
    // 起点屏幕坐标
    var ss = cv.worldToScreen(snapPt.x, snapPt.y)
    // 预览终点 x 初始化为起点
    main.wireEndX = ss.x
    // 预览终点 y 初始化为起点
    main.wireEndY = ss.y
    // 方向复位
    main.wiringDirection = 0
    // 清空选中连线
    main.selectedWireId = ""
}

// 定义"鼠标按下"处理函数
function onPressed(main, cv, mouse) {
    // 记录鼠标 x，供平移使用
    cv.lastMouseX = mouse.x
    // 记录鼠标 y，供平移使用
    cv.lastMouseY = mouse.y
    // 鼠标世界坐标
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    // 是否可编辑
    var editable = !main.circuitViewOnly

    // 正在放置元件：在点击处放置并退出放置态
    if (editable && main.mode === "edit" && main.placingType !== "") {
        // 子电路实例需带 subId
        var subId = main.placingType === "sub" ? main.placingSubId : ""
        // 添加元件，返回新 id
        var newId = main.circuitRef.addComponent(main.placingType, wp.x, wp.y,
                                                 main.placingBitWidth,
                                                 main.placingInputCount,
                                                 subId)
        // 添加失败提示
        if (newId === "") main.statusText = "⚠ 无法放置"
        // 成功则选中新元件
        else {
            // 选中新元件
            main.selectedCompId = newId
            // 清空选中连线
            main.selectedWireId = ""
            // 刷新选中信息
            main.refreshSel()
        }
        // 退出放置态
        main.placingType = ""
        // 清空待放置子电路 id
        main.placingSubId = ""
        // 放置完成，结束处理
        return
    }

    // 元件列表
    var comps = main.cachedComps

    // select 模式：优先命中连线，其次元件，最后框选
    if (main.mode === "select") {
        // 命中测试连线（容差随缩放补偿）
        var wid = main.circuitRef.wireAtPoint(wp.x, wp.y, 15 / Math.max(0.1, main.viewScale))
        // 命中连线：选中它
        if (wid && wid !== "") {
            // 记录选中连线 id
            main.selectedWireId = wid
            // 清空多选
            main.selectedIds = []
            // 清空单选元件
            main.selectedCompId = ""
            // 状态提示
            main.statusText = "已选中连线（Delete 删除，Ctrl+C 复制）"
            // 请求重绘
            cv.requestPaint()
            // 处理结束
            return
        }
        // 倒序命中元件（后绘制者优先）
        for (var i = comps.length - 1; i >= 0; i--) {
            // 当前元件
            var c = comps[i]
            // 元件尺寸
            var size = cv.compSize(c)
            // 点落在元件包围盒内
            if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                // 若尚未选中则设为唯一选中
                if (main.selectedIds.indexOf(c.id) < 0) main.selectedIds = [c.id]
                // 清空选中连线
                main.selectedWireId = ""
                // 打断撤销合并，开始新的拖动会话
                main.circuitRef.breakUndoMerge()
                // 进入拖动选中状态
                main.draggingSelection = true
                // 记录拖动起点世界 x
                main.lastDragWorldX = wp.x
                // 记录拖动起点世界 y
                main.lastDragWorldY = wp.y
                // 请求重绘
                cv.requestPaint()
                // 处理结束
                return
            }
        }
        // 空白处按下：清空选择
        main.selectedIds = []
        // 清空选中连线
        main.selectedWireId = ""
        // 进入框选状态
        main.boxSelecting = true
        // 记录框起点屏幕 x
        main.boxStartX = mouse.x
        // 记录框起点屏幕 y
        main.boxStartY = mouse.y
        // 框当前点 x 初始化为起点
        main.boxCurrentX = mouse.x
        // 框当前点 y 初始化为起点
        main.boxCurrentY = mouse.y
        // 请求重绘
        cv.requestPaint()
        // 处理结束
        return
    }

    // 编辑模式：先探测起点处的缩短候选
    var shortenCandidates = []
    // 仅在可编辑的编辑模式下探测
    if (editable && main.mode === "edit") {
        // 查找起点处可缩短的连线
        shortenCandidates = findShortenTargetAt(main, wp.x, wp.y)
    }

    // 编辑模式：端口优先于元件
    if (editable && main.mode === "edit") {
        // 命中端口
        var onPort = findPortAt(main, cv, mouse.x, mouse.y)
        // 命中端口则从端口开始拉线
        if (onPort) {
            // 从端口开始拉线
            startWiringFromPort(main, cv, onPort)
            // 记录缩短候选
            main.shortenWireCandidates = shortenCandidates
            // 状态提示
            main.statusText = "从端口引出（拖回原线内部可缩短）"
            // 请求重绘
            cv.requestPaint()
            // 处理结束
            return
        }

        // 未命中端口但命中连线：正交模式下从线中间 T 型引出
        var wireId = main.circuitRef.wireAtPoint(wp.x, wp.y, 15 / Math.max(0.1, main.viewScale))
        // 命中连线
        if (wireId && wireId !== "") {
            // 取生效线型
            var effType = resolveEffectiveWireType(main)
            // 正交线才支持 T 型引出
            if (effType === "ortho") {
                // 吸附到线上最近点
                var snapPt = snapToWire(main, wireId, wp.x, wp.y)
                // 吸附成功则开始拉线
                if (snapPt) {
                    // 从线中间开始拉线
                    startWiringFromWire(main, cv, snapPt)
                    // 保留缩短候选：用户可能想拖回原线内部缩短
                    main.shortenWireCandidates = shortenCandidates
                    // 状态提示
                    main.statusText = "从线引出（T 型接入，拖回线上方可缩短）"
                    // 请求重绘
                    cv.requestPaint()
                    // 处理结束
                    return
                }
            } else {
                // 贝塞尔线不支持从线中间引出
                main.statusText = "⚠ 贝塞尔线只能从端口引出"
            }
        }
    }

    // 编辑/控制模式：命中元件
    for (var i4 = comps.length - 1; i4 >= 0; i4--) {
        // 当前元件
        var c4 = comps[i4]
        // 元件尺寸
        var size4 = cv.compSize(c4)
        // 点落在元件包围盒内
        if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
            // 选中元件
            main.selectedCompId = c4.id
            // 清空选中连线
            main.selectedWireId = ""
            // 刷新选中信息
            main.refreshSel()

            // 控制模式：操作输入/时钟
            if (editable && main.mode === "control") {
                // 输入/常量元件：二进制显示下点具体位翻转
                if (c4.type === "input" || c4.type === "const") {
                    // 取显示进制
                    var base = c4.displayBase || "bin"
                    // 二进制显示才按位翻转
                    if (base === "bin") {
                        // 计算点击位置对应的位索引
                        var bitIdx = cv.bitIndexAt(c4, wp.x)
                        // 取位列表
                        var bits = c4.inputBits || []
                        // 翻转该位
                        main.circuitRef.setBitValue(c4.id, bitIdx, !(bits[bitIdx] || false))
                    }
                // 时钟元件：手动翻转
                } else if (c4.type === "clock") {
                    // 翻转时钟输出
                    main.circuitRef.toggleClock(c4.id)
                }
            // 编辑模式：开始拖动元件
            } else if (editable && main.mode === "edit") {
                // 打断撤销合并，开始新的拖动会话
                main.circuitRef.breakUndoMerge()
                // 进入拖动元件状态
                cv.draggingComp = true
                // 记录拖动元件 id
                cv.dragCompId = c4.id
            }
            // 处理结束
            return
        }
    }

    // 空白处：清空选择
    main.selectedCompId = ""
    // 清空选中连线
    main.selectedWireId = ""
    // 刷新选中信息
    main.refreshSel()
    // 清空缩短候选
    main.shortenWireCandidates = []
    // 进入平移画布状态
    cv.panning = true
}

// 定义"鼠标移动"处理函数
function onPositionChanged(main, cv, mouse) {
    // 框选中：只更新框的当前点
    if (main.boxSelecting) {
        // 更新框当前点屏幕 x
        main.boxCurrentX = mouse.x
        // 更新框当前点屏幕 y
        main.boxCurrentY = mouse.y
        // 框选处理结束
        return
    }

    // 拖动选中集合：按相对位移移动所有选中元件
    if (main.draggingSelection) {
        // 鼠标世界坐标
        var wp = cv.screenToWorld(mouse.x, mouse.y)
        // 相对上次的世界位移 x
        var dwx = wp.x - main.lastDragWorldX
        // 相对上次的世界位移 y
        var dwy = wp.y - main.lastDragWorldY
        // 有实际位移才移动
        if (Math.abs(dwx) > 0.001 || Math.abs(dwy) > 0.001) {
            // 批量相对移动选中元件
            main.circuitRef.moveSelection(main.selectedIds, dwx, dwy)
            // 更新上次拖动世界 x
            main.lastDragWorldX = wp.x
            // 更新上次拖动世界 y
            main.lastDragWorldY = wp.y
        }
        // 拖动选中处理结束
        return
    }

    // 拉线中：更新预览终点与方向
    if (main.wiring) {
        // 吸附后的世界点
        var sp = snapWorldPoint(main, cv, mouse)
        // 起点世界 x
        var sx = main.wiringStartX
        // 起点世界 y
        var sy = main.wiringStartY
        // 更新拐角方向
        updateWiringDirection(main, sx, sy, sp.x, sp.y)
        // 终点屏幕坐标
        var es = cv.worldToScreen(sp.x, sp.y)
        // 更新预览终点屏幕 x
        main.wireEndX = es.x
        // 更新预览终点屏幕 y
        main.wireEndY = es.y
        // 请求重绘
        cv.requestPaint()
        // 拉线处理结束
        return
    }

    // 拖动单个元件：移动到鼠标世界坐标
    if (cv.draggingComp && cv.dragCompId !== "" && !main.circuitViewOnly) {
        // 鼠标世界坐标
        var wp3 = cv.screenToWorld(mouse.x, mouse.y)
        // 移动元件到该坐标
        main.circuitRef.moveComponent(cv.dragCompId, wp3.x, wp3.y)
        // 拖动元件处理结束
        return
    }

    // 平移画布：按屏幕位移累加视图偏移
    if (cv.panning) {
        // 视图 x 累加屏幕位移
        main.viewX += mouse.x - cv.lastMouseX
        // 视图 y 累加屏幕位移
        main.viewY += mouse.y - cv.lastMouseY
        // 更新上次鼠标 x
        cv.lastMouseX = mouse.x
        // 更新上次鼠标 y
        cv.lastMouseY = mouse.y
        // 请求重绘
        cv.requestPaint()
    }
}

// 定义"鼠标释放"处理函数
function onReleased(main, cv, mouse) {
    // 框选结束：根据框范围选中元件
    if (main.boxSelecting) {
        // 框左上角 x
        var x1 = Math.min(main.boxStartX, mouse.x)
        // 框左上角 y
        var y1 = Math.min(main.boxStartY, mouse.y)
        // 框右下角 x
        var x2 = Math.max(main.boxStartX, mouse.x)
        // 框右下角 y
        var y2 = Math.max(main.boxStartY, mouse.y)
        // 框太小视为点击空白，清空选择
        if (Math.abs(x2 - x1) < 5 && Math.abs(y2 - y1) < 5) {
            // 清空多选
            main.selectedIds = []
        } else {
            // 收集框内元件 id
            var ids = []
            // 遍历所有元件
            for (var i = 0; i < main.cachedComps.length; i++) {
                // 当前元件
                var c = main.cachedComps[i]
                // 元件中心屏幕坐标
                var sp = cv.worldToScreen(c.x, c.y)
                // 中心落在框内则选中
                if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2)
                    ids.push(c.id)
            }
            // 写回多选
            main.selectedIds = ids
            // 状态提示
            main.statusText = "已选中 " + ids.length + " 个元件"
        }
        // 退出框选
        main.boxSelecting = false
        // 请求重绘
        cv.requestPaint()
    }

    // 拉线结束：决定缩短、连线或放弃
    if (main.wiring) {
        // 吸附终点
        var sp2 = snapWorldPoint(main, cv, mouse)
        // 终点世界 x
        var endX = sp2.x
        // 终点世界 y
        var endY = sp2.y
        // 终点命中的端口（可能为 null）
        var endPort = sp2.port

        // 取缩短候选线
        var cands = main.shortenWireCandidates || []
        // 是否已执行缩短
        var didShorten = false
        // 遍历缩短候选
        for (var ci = 0; ci < cands.length; ci++) {
            // 候选线 id
            var candId = cands[ci]
            // 取候选线 map
            var sw = main.circuitRef.getWire(candId)
            // 无效候选跳过
            if (!sw || !sw.path || sw.path.length < 2) continue
            // 终点是否落在该线内部
            var inside = false
            // 遍历该线每段
            for (var k = 0; k + 1 < sw.path.length; k++) {
                // 判断终点是否落在该段内部
                if (pointOnSegInterior(endX, endY,
                                       sw.path[k].x, sw.path[k].y,
                                       sw.path[k+1].x, sw.path[k+1].y)) {
                    // 命中内部
                    inside = true
                    // 停止遍历
                    break
                }
            }
            // 命中内部则缩短该线
            if (inside) {
                // 缩短该线末端到终点
                main.circuitRef.shortenWireEnd(candId, endX, endY)
                // 状态提示
                main.statusText = "已缩短连线"
                // 标记已缩短
                didShorten = true
                // 只缩短一条，跳出
                break
            }
        }

        // 未缩短则按线型创建连线
        if (!didShorten) {
            // 取生效线型
            var effType = resolveEffectiveWireType(main)
            // 起点世界 x
            var startX = main.wiringStartX
            // 起点世界 y
            var startY = main.wiringStartY

            // 贝塞尔：必须两端都是端口，且方向为输出→输入
            if (effType === "bezier") {
                // 起点不是端口则报错
                if (main.wiringFromComp === "") {
                    main.statusText = "⚠ 贝塞尔线需从端口开始"
                // 终点不是端口则报错
                } else if (!endPort) {
                    main.statusText = "⚠ 贝塞尔线需拖到端口"
                } else {
                    // 起终点不能是同一个端口
                    var same = (endPort.compId === main.wiringFromComp
                                && endPort.portIdx === main.wiringFromPort
                                && endPort.isOutput === main.wiringStartIsOutput)
                    // 不是同一端口才连线
                    if (!same) {
                        // 起点是否输出
                        var startIsOut = main.wiringStartIsOutput
                        // 终点是否输出
                        var endIsOut = endPort.isOutput
                        // 输出→输入
                        if (startIsOut && !endIsOut) {
                            // 创建贝塞尔线
                            main.circuitRef.addWire(main.wiringFromComp, main.wiringFromPort,
                                                    endPort.compId, endPort.portIdx)
                            // 状态提示
                            main.statusText = "已连线（贝塞尔）"
                        // 输入→输出（反向，交换端点）
                        } else if (!startIsOut && endIsOut) {
                            // 创建贝塞尔线（端点交换）
                            main.circuitRef.addWire(endPort.compId, endPort.portIdx,
                                                    main.wiringFromComp, main.wiringFromPort)
                            // 状态提示
                            main.statusText = "已连线（贝塞尔）"
                        } else {
                            // 方向不合法
                            main.statusText = "⚠ 需连接 输出→输入"
                        }
                    }
                }
            // 正交：终点无端口时尝试吸到线上，然后创建正交线
            } else {
                // 终点无端口才尝试吸线
                if (!endPort) {
                    // 命中测试终点附近的线
                    var endWireId = main.circuitRef.wireAtPoint(endX, endY, 15 / Math.max(0.1, main.viewScale))
                    // 命中线则吸附
                    if (endWireId && endWireId !== "") {
                        // 吸附到线上最近点
                        var snapEnd = snapToWire(main, endWireId, endX, endY)
                        // 吸附成功则更新终点
                        if (snapEnd) { endX = snapEnd.x; endY = snapEnd.y }
                    }
                }
                // 有实际拖动距离才创建
                var hasDragged = (Math.abs(endX - startX) > 0.5 ||
                                  Math.abs(endY - startY) > 0.5)
                // 有拖动才创建
                if (hasDragged) {
                    // 取拐角方向，未定则默认横优先
                    var dir = main.wiringDirection
                    if (dir === 0) dir = 1
                    // 创建正交线
                    main.circuitRef.addOrthoWire(startX, startY, endX, endY, dir)
                    // 状态提示
                    main.statusText = "已连线（正交）"
                }
            }
        }

        // 退出拉线状态
        main.wiring = false
        // 清空起点元件
        main.wiringFromComp = ""
        // 清空起点端口
        main.wiringFromPort = -1
        // 方向复位
        main.wiringDirection = 0
        // 清空缩短候选
        main.shortenWireCandidates = []
        // 请求重绘
        cv.requestPaint()
    }

    // 清理平移状态
    cv.panning = false
    // 清理拖动元件状态
    cv.draggingComp = false
    // 清空拖动元件 id
    cv.dragCompId = ""
    // 清理拖动选中状态
    main.draggingSelection = false
    // 打断撤销合并，结束本次手势
    main.circuitRef.breakUndoMerge()
}

// 定义"操作被取消"处理函数：清理所有临时交互状态
function onCanceled(main, cv) {
    // 清理平移
    cv.panning = false
    // 清理拖动元件
    cv.draggingComp = false
    // 清空拖动元件 id
    cv.dragCompId = ""
    // 清理拖动选中
    main.draggingSelection = false
    // 清理框选
    main.boxSelecting = false
    // 清理拉线
    main.wiring = false
    // 清空起点元件
    main.wiringFromComp = ""
    // 清空起点端口
    main.wiringFromPort = -1
    // 方向复位
    main.wiringDirection = 0
    // 清空缩短候选
    main.shortenWireCandidates = []
    // 打断撤销合并
    main.circuitRef.breakUndoMerge()
    // 请求重绘
    cv.requestPaint()
}

// 定义"滚轮缩放"处理函数
function onWheel(main, cv, wheel) {
    // 旧缩放
    var oldScale = main.viewScale
    // 缩放因子：向上滚放大，向下滚缩小
    var factor = wheel.angleDelta.y > 0 ? 1.15 : 1/1.15
    // 新缩放，钳位到 [0.15, 4.0]
    var newScale = Math.max(0.15, Math.min(4.0, main.viewScale * factor))
    // 缩放未变则返回
    if (newScale === oldScale) return
    // 鼠标屏幕 x 作为缩放锚点
    var mx = wheel.x
    // 鼠标屏幕 y 作为缩放锚点
    var my = wheel.y
    // 调整视图 x，使锚点下的世界坐标保持不动
    main.viewX = mx - (mx - main.viewX) * (newScale / oldScale)
    // 调整视图 y，使锚点下的世界坐标保持不动
    main.viewY = my - (my - main.viewY) * (newScale / oldScale)
    // 写回缩放
    main.viewScale = newScale
    // 请求重绘
    main.triggerRepaint()
}

// 定义"双击"处理函数
function onDoubleClicked(main, cv, mouse) {
    // 鼠标世界坐标
    var wp = cv.screenToWorld(mouse.x, mouse.y)
    // 倒序命中元件（后绘制者优先）
    for (var i = main.cachedComps.length - 1; i >= 0; i--) {
        // 当前元件
        var c = main.cachedComps[i]
        // 元件尺寸
        var size = cv.compSize(c)
        // 点落在元件包围盒内
        if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
            // 双击子电路实例：进入浏览
            if (c.type === "sub") main.circuitRef.enterSubcircuit(c.subId)
            // 命中即结束
            return
        }
    }
}