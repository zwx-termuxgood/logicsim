.pragma library

function worldToScreen(viewScale, viewX, viewY, wx, wy) {
    return { x: wx * viewScale + viewX, y: wy * viewScale + viewY }
}

function screenToWorld(viewScale, viewX, viewY, sx, sy) {
    return { x: (sx - viewX) / viewScale, y: (sy - viewY) / viewScale }
}

function isVisible(viewScale, viewX, viewY, canvasW, canvasH, wx, wy, w, h) {
    var sx = (wx - w/2) * viewScale + viewX
    var sy = (wy - h/2) * viewScale + viewY
    var ex = sx + w * viewScale
    var ey = sy + h * viewScale
    return ex >= -50 && sx <= canvasW + 50 &&
    ey >= -50 && sy <= canvasH + 50
}

function roundRect(ctx, x, y, w, h, r) {
    if (r > w/2) r = w/2
    if (r > h/2) r = h/2
    ctx.beginPath()
    ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y)
    ctx.arcTo(x + w, y, x + w, y + r, r)
    ctx.lineTo(x + w, y + h - r)
    ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
    ctx.lineTo(x + r, y + h)
    ctx.arcTo(x, y + h, x, y + h - r, r)
    ctx.lineTo(x, y + r)
    ctx.arcTo(x, y, x + r, y, r)
    ctx.closePath()
}