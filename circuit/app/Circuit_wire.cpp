#include "../Circuit.h"
#include "../core/IdAllocator.h"
#include "../logic/ComponentLogic.h"
#include "../logic/WireNetworkSolver.h"
#include <QVariantMap>
#include <QPointF>
#include <QHash>
#include <QSet>
#include <cmath>

namespace {
QVariantMap makePoint(double x, double y) {
    QVariantMap p;
    p["x"] = x; p["y"] = y;
    return p;
}
double pointSegDistSq(double px, double py, double x1, double y1, double x2, double y2) {
    double dx = x2 - x1, dy = y2 - y1;
    double len2 = dx * dx + dy * dy;
    double t = 0;
    if (len2 > 1e-9) {
        t = ((px - x1) * dx + (py - y1) * dy) / len2;
        if (t < 0) t = 0; else if (t > 1) t = 1;
    }
    double cx = x1 + t * dx, cy = y1 + t * dy;
    double ddx = px - cx, ddy = py - cy;
    return ddx * ddx + ddy * ddy;
}
}

QVariantMap Circuit::findPortAt(double x, double y, double tol) const {
    const Context& ctx = currentCtx();
    QVariantMap best;
    double bestDist = tol * tol;
    for (const auto& cv : ctx.components) {
        QVariantMap c = cv.toMap();
        QString cid = c.value("id").toString();
        QString t = c.value("type").toString();
        if (t == "text") continue;
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();
        QVariantList outs = ComponentLogic::outputPorts(c);
        for (int i = 0; i < outs.size(); ++i) {
            QVariantMap p = outs[i].toMap();
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            double dx = x - px, dy = y - py;
            double d = dx * dx + dy * dy;
            if (d < bestDist) {
                bestDist = d;
                best.clear();
                best["compId"] = cid;
                best["port"] = i;
                best["isOutput"] = true;
                best["x"] = px;
                best["y"] = py;
            }
        }
        QVariantList ins = ComponentLogic::inputPorts(c);
        for (int i = 0; i < ins.size(); ++i) {
            QVariantMap p = ins[i].toMap();
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            double dx = x - px, dy = y - py;
            double d = dx * dx + dy * dy;
            if (d < bestDist) {
                bestDist = d;
                best.clear();
                best["compId"] = cid;
                best["port"] = i;
                best["isOutput"] = false;
                best["x"] = px;
                best["y"] = py;
            }
        }
    }
    return best;
}

QVariantList Circuit::allWireSegments() const {
    QVariantList out;
    const Context& ctx = currentCtx();
    for (const auto& wv : ctx.wires) {
        QVariantMap w = wv.toMap();
        if (w.value("wireType").toString() != "ortho") continue;
        QString id = w.value("id").toString();
        QVariantList path = w.value("path").toList();
        for (int i = 0; i + 1 < path.size(); ++i) {
            QVariantMap p0 = path[i].toMap();
            QVariantMap p1 = path[i + 1].toMap();
            QVariantMap seg;
            seg["wireId"] = id;
            seg["x1"] = p0.value("x");
            seg["y1"] = p0.value("y");
            seg["x2"] = p1.value("x");
            seg["y2"] = p1.value("y");
            out.append(seg);
        }
    }
    return out;
}

QVariantList Circuit::connectionDots() const {
    return WireNetworkSolver::connectionDots(currentCtx().components, currentCtx().wires);
}

// ============================================================
// addOrthoWire
//   起点/终点坐标全部由 JS 侧决定（吸附端口 / 从线引出 / 网格化）
//   本函数只负责：
//     - 端口绑定（根据 isOutput 分配 fromComp/toComp）
//     - 反向时交换端点 + 镜像 direction
//     - 生成 L 型 path
// ============================================================
QString Circuit::addOrthoWire(double fromX, double fromY,
                              double toX, double toY,
                              int direction)
{
    if (m_viewOnly) return QString();

    QVariantMap fPort = findPortAt(fromX, fromY, 15.0);
    QVariantMap tPort = findPortAt(toX, toY, 15.0);

    QString fromComp, toComp;
    int fromPort = -1, toPort = -1;

    // 起点：命中端口用端口精确坐标，否则保留 JS 传的精确点（从线引出的 T 型点）
    if (!fPort.isEmpty()) {
        fromX = fPort.value("x").toDouble();
        fromY = fPort.value("y").toDouble();
    }
    // 不网格化（JS 侧已处理）

    // 终点：命中端口用端口精确坐标，否则保留 JS 传的精确点（已吸附或网格化）
    if (!tPort.isEmpty()) {
        toX = tPort.value("x").toDouble();
        toY = tPort.value("y").toDouble();
    }
    // 不网格化（JS 侧已处理）

    // 端口绑定
    if (!fPort.isEmpty() && !tPort.isEmpty()) {
        bool fIsOut = fPort.value("isOutput").toBool();
        bool tIsOut = tPort.value("isOutput").toBool();
        if (fIsOut && !tIsOut) {
            fromComp = fPort.value("compId").toString();
            fromPort = fPort.value("port").toInt();
            toComp   = tPort.value("compId").toString();
            toPort   = tPort.value("port").toInt();
        } else if (!fIsOut && tIsOut) {
            fromComp = tPort.value("compId").toString();
            fromPort = tPort.value("port").toInt();
            toComp   = fPort.value("compId").toString();
            toPort   = fPort.value("port").toInt();
            std::swap(fromX, toX);
            std::swap(fromY, toY);
            if (direction == 1) direction = 2;
            else if (direction == 2) direction = 1;
        }
    } else if (!fPort.isEmpty() && fPort.value("isOutput").toBool()) {
        fromComp = fPort.value("compId").toString();
        fromPort = fPort.value("port").toInt();
    } else if (!tPort.isEmpty() && !tPort.value("isOutput").toBool()) {
        toComp = tPort.value("compId").toString();
        toPort = tPort.value("port").toInt();
    }

    if (qAbs(fromX - toX) < 0.5 && qAbs(fromY - toY) < 0.5) return QString();

    Context& ctx = currentCtx();
    pushUndo();

    QVariantList path;
    path.append(makePoint(fromX, fromY));

    bool sameRow = qAbs(fromY - toY) < 0.5;
    bool sameCol = qAbs(fromX - toX) < 0.5;
    if (!sameRow && !sameCol) {
        QVariantMap corner;
        if (direction == 2) {
            corner["x"] = fromX; corner["y"] = toY;
        } else {
            corner["x"] = toX; corner["y"] = fromY;
        }
        path.append(corner);
    }
    path.append(makePoint(toX, toY));

    QVariantMap w;
    w["id"] = IdAllocator::nextWireId(ctx);
    w["wireType"] = "ortho";
    w["path"] = path;
    if (!fromComp.isEmpty()) {
        w["fromComp"] = fromComp;
        w["fromPort"] = fromPort;
    }
    if (!toComp.isEmpty()) {
        w["toComp"] = toComp;
        w["toPort"] = toPort;
    }
    ctx.wires.append(w);

    evaluateAll();
    emit changed();
    emit structureChanged();
    return w["id"].toString();
}

bool Circuit::shortenWireEnd(const QString& wireId, double newX, double newY) {
    if (m_viewOnly) return false;
    int idx = indexOfWire(wireId);
    if (idx < 0) return false;
    Context& ctx = currentCtx();
    QVariantMap w = ctx.wires[idx].toMap();
    if (w.value("wireType").toString() != "ortho") return false;
    QVariantList path = w.value("path").toList();
    if (path.size() < 2) return false;
    pushUndo("shorten:" + wireId);
    QVariantMap p0 = path.first().toMap();
    QVariantMap p1 = path.last().toMap();
    double x0 = p0.value("x").toDouble(), y0 = p0.value("y").toDouble();
    double x1 = p1.value("x").toDouble(), y1 = p1.value("y").toDouble();
    newX = std::round(newX / 10.0) * 10.0;
    newY = std::round(newY / 10.0) * 10.0;
    double d0 = std::hypot(x0 - newX, y0 - newY);
    double d1 = std::hypot(x1 - newX, y1 - newY);
    bool shrinkFront = (d0 <= d1);
    double ox, oy;
    if (shrinkFront) { ox = x1; oy = y1; }
    else { ox = x0; oy = y0; }
    if (qAbs(ox - newX) < 0.5 && qAbs(oy - newY) < 0.5) {
        ctx.wires.removeAt(idx);
        evaluateAll(); emit changed(); emit structureChanged();
        return true;
    }
    QVariantList newPath;
    if (shrinkFront) {
        newPath.append(makePoint(newX, newY));
        if (qAbs(newX - ox) > 0.5 && qAbs(newY - oy) > 0.5) {
            QVariantMap mid; mid["x"] = newX; mid["y"] = oy;
            newPath.append(mid);
        }
        newPath.append(makePoint(ox, oy));
        w.remove("fromComp"); w.remove("fromPort");
    } else {
        newPath.append(makePoint(ox, oy));
        if (qAbs(newX - ox) > 0.5 && qAbs(newY - oy) > 0.5) {
            QVariantMap mid; mid["x"] = ox; mid["y"] = newY;
            newPath.append(mid);
        }
        newPath.append(makePoint(newX, newY));
        w.remove("toComp"); w.remove("toPort");
    }
    w["path"] = newPath;
    ctx.wires[idx] = w;
    evaluateAll();
    emit changed();
    emit structureChanged();
    return true;
}

QString Circuit::addWire(const QString& fromComp, int fromPort,
                         const QString& toComp, int toPort) {
    if (m_viewOnly) return QString();
    if (indexOfComponent(fromComp) < 0) return QString();
    if (indexOfComponent(toComp) < 0) return QString();
    Context& ctx = currentCtx();
    pushUndo();
    QVariantMap w;
    w["id"] = IdAllocator::nextWireId(ctx);
    w["fromComp"] = fromComp;
    w["fromPort"] = fromPort;
    w["toComp"] = toComp;
    w["toPort"] = toPort;
    ctx.wires.append(w);
    evaluateAll();
    emit changed();
    emit structureChanged();
    return w["id"].toString();
}

void Circuit::removeWire(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfWire(id);
    if (idx < 0) return;
    pushUndo();
    currentCtx().wires.removeAt(idx);
    evaluateAll();
    emit changed();
    emit structureChanged();
}

QString Circuit::wireAtPoint(double x, double y, double tol) const {
    const Context& ctx = currentCtx();
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        QString id = w.value("id").toString();
        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";
        if (wt == "virtual") continue;
        if (wt == "ortho") {
            QVariantList path = w.value("path").toList();
            for (int i = 0; i + 1 < path.size(); ++i) {
                QVariantMap p0 = path[i].toMap();
                QVariantMap p1 = path[i + 1].toMap();
                if (pointSegDistSq(x, y,
                                   p0.value("x").toDouble(), p0.value("y").toDouble(),
                                   p1.value("x").toDouble(), p1.value("y").toDouble())
                    < tol * tol) return id;
            }
            continue;
        }
        QString fromId = w.value("fromComp").toString();
        QString toId = w.value("toComp").toString();
        int fromIdx = w.value("fromPort").toInt();
        int toIdx = w.value("toPort").toInt();
        auto compPos = [&](const QString& cid, int port, bool isOut,
                           double& px, double& py) -> bool {
            int ci = indexOfComponent(cid);
            if (ci < 0) return false;
            QVariantMap c = ctx.components[ci].toMap();
            QVariantList ports = isOut ? ComponentLogic::outputPorts(c)
                                       : ComponentLogic::inputPorts(c);
            if (port < 0 || port >= ports.size()) return false;
            QVariantMap p = ports[port].toMap();
            px = c.value("x").toDouble() + p.value("x").toDouble();
            py = c.value("y").toDouble() + p.value("y").toDouble();
            return true;
        };
        double x1, y1, x2, y2;
        if (!compPos(fromId, fromIdx, true, x1, y1)) continue;
        if (!compPos(toId, toIdx, false, x2, y2)) continue;
        double cx = std::max(std::abs(x2 - x1) / 2.0, 40.0);
        const int samples = 20;
        for (int i = 0; i <= samples; ++i) {
            double t = (double)i / samples;
            double mt = 1.0 - t;
            double bx = mt*mt*mt*x1 + 3*mt*mt*t*(x1+cx) + 3*mt*t*t*(x2-cx) + t*t*t*x2;
            double by = mt*mt*mt*y1 + 3*mt*mt*t*y1 + 3*mt*t*t*y2 + t*t*t*y2;
            double dx = bx - x, dy = by - y;
            if (dx*dx + dy*dy < tol*tol) return id;
        }
    }
    return QString();
}

bool Circuit::setWirePathPoint(const QString& id, int index, double x, double y) {
    if (m_viewOnly) return false;
    int idx = indexOfWire(id);
    if (idx < 0) return false;
    Context& ctx = currentCtx();
    QVariantMap w = ctx.wires[idx].toMap();
    if (w.value("wireType").toString() != "ortho") return false;
    QVariantList path = w.value("path").toList();
    if (index < 0 || index >= path.size()) return false;
    if (index == 0 || index == path.size() - 1) return false;
    pushUndo("editPath:" + id);
    x = std::round(x / 10.0) * 10.0;
    y = std::round(y / 10.0) * 10.0;
    QVariantMap pt = path[index].toMap();
    pt["x"] = x; pt["y"] = y;
    path[index] = pt;
    if (index - 1 >= 0 && index + 1 < path.size()) {
        QVariantMap prev = path[index - 1].toMap();
        QVariantMap next = path[index + 1].toMap();
        double px = prev.value("x").toDouble(), py = prev.value("y").toDouble();
        bool horiz = (qAbs(px - x) <= qAbs(py - y));
        if (horiz) {
            if (index - 1 != 0) { prev["y"] = y; path[index - 1] = prev; }
            if (index + 1 != (int)path.size() - 1) { next["x"] = x; path[index + 1] = next; }
        } else {
            if (index - 1 != 0) { prev["x"] = x; path[index - 1] = prev; }
            if (index + 1 != (int)path.size() - 1) { next["y"] = y; path[index + 1] = next; }
        }
    }
    w["path"] = path;
    ctx.wires[idx] = w;
    evaluateAll();
    emit changed();
    emit structureChanged();
    return true;
}

QVariantList Circuit::getWirePath(const QString& id) const {
    int idx = indexOfWire(id);
    if (idx < 0) return {};
    return currentCtx().wires[idx].toMap().value("path").toList();
}

QString Circuit::getWireType(const QString& id) const {
    int idx = indexOfWire(id);
    if (idx < 0) return "";
    QString wt = currentCtx().wires[idx].toMap().value("wireType").toString();
    return wt.isEmpty() ? "bezier" : wt;
}