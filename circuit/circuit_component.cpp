#include "circuit.h"
#include <QVariantMap>
#include <QSet>

void Circuit::removeComponent(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() == "clock") m_clockStates.remove(id);
    ctx.components.removeAt(idx);
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (w.value("fromComp").toString() == id || w.value("toComp").toString() == id)
            ctx.wires.removeAt(i);
    }
    evaluateAll();
    emit changed();
}

void Circuit::removeComponents(const QStringList& ids) {
    if (m_viewOnly) return;
    Context& ctx = currentCtx();
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        auto c = ctx.components[i].toMap();
        if (ids.contains(c.value("id").toString())) {
            if (c.value("type").toString() == "clock")
                m_clockStates.remove(c.value("id").toString());
            ctx.components.removeAt(i);
        }
    }
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (ids.contains(w.value("fromComp").toString()) ||
            ids.contains(w.value("toComp").toString()))
            ctx.wires.removeAt(i);
    }
    evaluateAll();
    emit changed();
}

void Circuit::moveComponent(const QString& id, double x, double y) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    c["x"] = x; c["y"] = y;
    ctx.components[idx] = c;
    emit geometryChanged();
}

void Circuit::moveSelection(const QStringList& ids, double dx, double dy) {
    if (m_viewOnly) return;
    if (ids.isEmpty()) return;
    Context& ctx = currentCtx();
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);
    for (int i = 0; i < ctx.components.size(); ++i) {
        QVariantMap c = ctx.components[i].toMap();
        if (idSet.contains(c.value("id").toString())) {
            c["x"] = c.value("x").toDouble() + dx;
            c["y"] = c.value("y").toDouble() + dy;
            ctx.components[i] = c;
        }
    }
    emit geometryChanged();
}

void Circuit::toggleInput(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "input") return;
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    bool v = !bits[0].toBool();
    for (int i = 0; i < bits.size(); ++i) bits[i] = v;
    c["inputBits"] = bits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::setBitValue(const QString& id, int bit, bool value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    if (bit < 0 || bit >= bits.size()) return;
    bits[bit] = value;
    c["inputBits"] = bits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::setComponentProp(const QString& id, const QString& key, const QVariant& value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (key == "bitWidth") {
        int bw = value.toInt();
        if (bw < 1) bw = 1;
        if (bw > 64) bw = 64;
        c["bitWidth"] = bw;
        QVariantList bits = c.value("inputBits").toList();
        while (bits.size() < bw) bits.prepend(false);
        while (bits.size() > bw) bits.removeFirst();
        c["inputBits"] = bits;
        QString t = c.value("type").toString();
        if (t == "splitter" || t == "hub") {
            QVariantList splits = c.value("outputSplits").toList();
            int sum = 0;
            for (auto& sv : splits) sum += sv.toInt();
            if (sum != bw) {
                QVariantList ns;
                for (int i = 0; i < bw; ++i) ns.append(1);
                c["outputSplits"] = ns;
            }
        }
    } else if (key == "inputCount") {
        int v = value.toInt();
        if (v < 2) v = 2;
        if (v > 64) v = 64;
        c[key] = v;
    } else {
        c[key] = value;
    }
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::renameComponent(const QString& id, const QString& name) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    c["name"] = name;
    ctx.components[idx] = c;
    emit changed();
}

void Circuit::setSplitterSplits(const QString& id, const QString& splitsStr) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return;
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    QString s = splitsStr;
    s.replace(',', ' ');
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    QVariantList splits;
    int sum = 0;
    for (auto& p : parts) {
        bool ok = false;
        int v = p.toInt(&ok);
        if (!ok || v < 1) continue;
        splits.append(v); sum += v;
    }
    if (sum != bw || splits.isEmpty()) return;
    c["outputSplits"] = splits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

QString Circuit::getSplitterSplitsStr(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    QVariantMap c = currentCtx().components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return {};
    QVariantList splits = c.value("outputSplits").toList();
    QStringList parts;
    for (auto& v : splits) parts.append(QString::number(v.toInt()));
    return parts.join(",");
}