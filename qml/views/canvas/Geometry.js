.pragma library
// 声明为共享 JS 库：供多个 QML 文件 import 使用，不绑定到某个 QML 实例

// 定义"世界坐标转屏幕坐标"辅助函数
function worldToScreen(viewScale, viewX, viewY, wx, wy) {
    // 返回换算后的屏幕坐标：屏幕 = 世界 * 缩放 + 视图偏移
    return { x: wx * viewScale + viewX, y: wy * viewScale + viewY }
}

// 定义"屏幕坐标转世界坐标"辅助函数
function screenToWorld(viewScale, viewX, viewY, sx, sy) {
    // 返回换算后的世界坐标：世界 = (屏幕 - 视图偏移) / 缩放
    return { x: (sx - viewX) / viewScale, y: (sy - viewY) / viewScale }
}

// 定义"判断元件包围盒是否与画布可见区域相交"辅助函数
function isVisible(viewScale, viewX, viewY, canvasW, canvasH, wx, wy, w, h) {
    // 包围盒左上角屏幕 x（世界坐标减去半宽再换算）
    var sx = (wx - w/2) * viewScale + viewX
    // 包围盒左上角屏幕 y（世界坐标减去半高再换算）
    var sy = (wy - h/2) * viewScale + viewY
    // 包围盒右下角屏幕 x（左上角加缩放后宽度）
    var ex = sx + w * viewScale
    // 包围盒右下角屏幕 y（左上角加缩放后高度）
    var ey = sy + h * viewScale
    // 与画布区域（外扩 50px 容差）相交即可见
    return ex >= -50 && sx <= canvasW + 50 &&
    // 纵向同样判断相交
    ey >= -50 && sy <= canvasH + 50
}

// 定义"在 Canvas 上下文绘制圆角矩形路径"辅助函数
function roundRect(ctx, x, y, w, h, r) {
    // 圆角半径不超过宽度一半
    if (r > w/2) r = w/2
    // 圆角半径不超过高度一半
    if (r > h/2) r = h/2
    // 开始新路径
    ctx.beginPath()
    // 移动到上边起点（左上圆角之后）
    ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y)
    // 右上圆角
    ctx.arcTo(x + w, y, x + w, y + r, r)
    // 右边直线
    ctx.lineTo(x + w, y + h - r)
    // 右下圆角
    ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
    // 下边直线
    ctx.lineTo(x + r, y + h)
    // 左下圆角
    ctx.arcTo(x, y + h, x, y + h - r, r)
    // 左边直线
    ctx.lineTo(x, y + r)
    // 左上圆角
    ctx.arcTo(x, y, x + r, y, r)
    // 闭合路径
    ctx.closePath()
}