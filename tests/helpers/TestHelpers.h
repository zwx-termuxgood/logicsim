#pragma once
#include <QVariantMap>
#include <QVariantList>
#include "core/CircuitContext.h"

// 最小元件夹具：只填充测试常用的字段，输入/输出状态留空，便于按场景补充。
// bw 为总线位宽，ic 为请求的输入数；实际端口数仍由元件类型的规则决定。
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
// 按值补充 ID，调用方的原始模板不受影响，可复用同一夹具构造多个元件。
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
