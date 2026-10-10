#include "circuit.h"
#include "PortGeometry.h"

QVariantMap Circuit::getComponent(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return currentCtx().components[idx].toMap();
}

QVariantMap Circuit::getWire(const QString& id) const {
    int idx = indexOfWire(id);
    if (idx < 0) return {};
    return currentCtx().wires[idx].toMap();
}

QVariantList Circuit::outputPortsInfo(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return PortGeometry::outputPorts(currentCtx().components[idx].toMap());
}

QVariantList Circuit::inputPortsInfo(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return PortGeometry::inputPorts(currentCtx().components[idx].toMap());
}

QVariantMap Circuit::allPortsInfo() const {
    QVariantMap out;
    const Context& ctx = currentCtx();
    for (const auto& cv : ctx.components) {
        QVariantMap c = cv.toMap();
        QString id = c.value("id").toString();
        QVariantMap info;
        info["out"] = PortGeometry::outputPorts(c);
        info["in"] = PortGeometry::inputPorts(c);
        out[id] = info;
    }
    return out;
}