#include "ClipboardCodec.h"
#include "../core/IdAllocator.h"
#include <QSet>

namespace ClipboardCodec {

QVariantList encode(const CircuitContext& ctx, const QStringList& ids) {
    QVariantList out;
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);

    QVariantList compsOut, wiresOut;
    for (auto& cv : ctx.components) {
        auto c = cv.toMap();
        if (idSet.contains(c.value("id").toString())) compsOut.append(c);
    }
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        if (idSet.contains(w.value("fromComp").toString()) &&
            idSet.contains(w.value("toComp").toString()))
            wiresOut.append(w);
    }

    QVariantMap data;
    data["components"] = compsOut;
    data["wires"] = wiresOut;
    out.append(data);
    return out;
}

PasteResult paste(CircuitContext& ctx,
                  const QVariantList& data,
                  double dx, double dy)
{
    PasteResult result;
    if (data.isEmpty()) return result;

    QVariantMap d = data[0].toMap();
    QVariantList comps = d.value("components").toList();
    QVariantList wires = d.value("wires").toList();
    if (comps.isEmpty()) return result;

    QHash<QString, QString> idMap;
    for (auto& cv : comps) {
        QVariantMap c = cv.toMap();
        QString oldId = c.value("id").toString();
        QString newId = IdAllocator::nextComponentId(ctx);
        idMap[oldId] = newId;
        c["id"] = newId;
        c["x"] = c.value("x").toDouble() + dx;
        c["y"] = c.value("y").toDouble() + dy;
        if (c.value("type").toString() == "clock")
            result.newClockStates[newId] = false;
        ctx.components.append(c);
        result.newIds.append(newId);
    }

    for (auto& wv : wires) {
        QVariantMap w = wv.toMap();
        QString from = idMap.value(w.value("fromComp").toString(), "");
        QString to = idMap.value(w.value("toComp").toString(), "");
        if (from.isEmpty() || to.isEmpty()) continue;
        QVariantMap nw;
        nw["id"] = IdAllocator::nextWireId(ctx);
        nw["fromComp"] = from;
        nw["fromPort"] = w.value("fromPort");
        nw["toComp"] = to;
        nw["toPort"] = w.value("toPort");
        ctx.wires.append(nw);
    }

    return result;
}

} // namespace ClipboardCodec
