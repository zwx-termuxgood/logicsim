#pragma once
#include <QVariantMap>
#include <QVariantList>
#include "core/CircuitContext.h"

inline QVariantMap makeComp(const QString& type, double x = 0, double y = 0,
                            int bw = 1, int ic = 2) {
    QVariantMap c;
    c["type"] = type;
    c["x"] = x; c["y"] = y;
    c["bitWidth"] = bw;
    c["inputCount"] = ic;
    c["displayBase"] = "bin";
    c["name"] = "";
    c["inputBits"] = QVariantList();
    c["outputPorts"] = QVariantList();
    c["inputPortValues"] = QVariantList();
    return c;
}
inline QVariantMap withId(QVariantMap c, const QString& id) { c["id"] = id; return c; }
inline QVariantMap pt(double x, double y) {
    QVariantMap p; p["x"] = x; p["y"] = y; return p;
}
inline QVariantMap makeOrthoWire(const QString& id, const QVariantList& path,
                                 const QString& fromComp = "", int fromPort = 0,
                                 const QString& toComp = "", int toPort = 0) {
    QVariantMap w;
    w["id"] = id;
    w["wireType"] = "ortho";
    w["path"] = path;
    if (!fromComp.isEmpty()) { w["fromComp"] = fromComp; w["fromPort"] = fromPort; }
    if (!toComp.isEmpty())   { w["toComp"]   = toComp;   w["toPort"]   = toPort;   }
    return w;
}
inline void addComp(CircuitContext& ctx, const QVariantMap& c) { ctx.components.append(c); }
inline void addWire(CircuitContext& ctx, const QVariantMap& w) { ctx.wires.append(w); }
