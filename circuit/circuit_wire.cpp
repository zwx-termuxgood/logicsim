#include "circuit.h"
#include "PortGeometry.h"
#include <QVariantMap>
#include <algorithm>
#include <cmath>

QString Circuit::addWire(const QString& fromComp, int fromPort,
                         const QString& toComp, int toPort) {
    if (m_viewOnly) return QString();
    if (indexOfComponent(fromComp) < 0 || indexOfComponent(toComp) < 0) return QString();
    if (fromComp == toComp) return QString();
    Context& ctx = currentCtx();

    pushUndo();

    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (w.value("toComp").toString() == toComp && w.value("toPort").toInt() == toPort)
            ctx.wires.removeAt(i);
    }

    QVariantMap w;
    w["id"] = QString("W%1").arg(++ctx.wireCounter);
    w["fromComp"] = fromComp;
    w["fromPort"] = fromPort;
    w["toComp"] = toComp;
    w["toPort"] = toPort;
    ctx.wires.append(w);
    evaluateAll();
    emit changed();
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
}

QString Circuit::wireAtPoint(double x, double y, double tol) const {
    const Context& ctx = currentCtx();
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        QString id = w.value("id").toString();
        QString fromId = w.value("fromComp").toString();
        QString toId = w.value("toComp").toString();
        int fromIdx = w.value("fromPort").toInt();
        int toIdx = w.value("toPort").toInt();

        auto compPos = [&](const QString& cid, int port, bool isOut, double& px, double& py) -> bool {
            int ci = indexOfComponent(cid);
            if (ci < 0) return false;
            QVariantMap c = ctx.components[ci].toMap();
            QVariantList ports = isOut ? outputPortsInfo(cid) : inputPortsInfo(cid);
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