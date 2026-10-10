#include "../Circuit.h"
#include "../core/IdAllocator.h"
#include "../logic/event/EventEngine.h"
#include <QVariantMap>
#include <QSet>
#include <QtGlobal>
#include <QDebug>

QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    if (m_viewOnly) return QString();
    if (bitWidth < 1) bitWidth = 1;
    if (bitWidth > 64) bitWidth = 64;
    if (inputCount < 2) inputCount = 2;
    if (inputCount > 64) inputCount = 64;

    if (type == "sub") {
        if (!canAddSubInstance(subId)) return QString();
    }

    pushUndo();

    QVariantMap c;
    Context& ctx = currentCtx();
    c["id"] = IdAllocator::nextComponentId(ctx);
    c["type"] = type;
    c["x"] = x; c["y"] = y;
    c["bitWidth"] = bitWidth;
    c["inputCount"] = inputCount;
    c["displayBase"] = "bin";
    c["name"] = "";

    QVariantList bits;
    bits.reserve(bitWidth);
    for (int i = 0; i < bitWidth; ++i) bits.append(false);
    c["inputBits"] = bits;

    QVariantList outPorts, inPorts;

    if (type == "text") {
        c["content"] = "文本";
    } else if (type == "clock") {
        c["name"] = "CLK";
    } else if (type == "const") {
        c["name"] = "CONST";
    } else if (type == "sub") {
        c["subId"] = subId;
        c["subInputNames"] = QVariantList();
        c["subOutputNames"] = QVariantList();
        c["subInputWidths"] = QVariantList();
        c["subOutputWidths"] = QVariantList();
    } else if (type == "splitter") {
        QVariantList splits;
        splits.reserve(bitWidth);
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        int segN = splits.size();
        inPorts.append(QString(bitWidth, '0'));
        for (int i = 0; i < segN; ++i) {
            int w = splits[i].toInt(); if (w < 1) w = 1;
            outPorts.append(QString(w, '0'));
        }
    } else if (type == "hub") {
        QVariantList splits;
        splits.reserve(bitWidth);
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        int segN = splits.size();
        for (int i = 0; i < segN; ++i) {
            int w = splits[i].toInt(); if (w < 1) w = 1;
            inPorts.append(QString(w, '0'));
        }
        outPorts.append(QString(bitWidth, '0'));
    } else if (type == "tgate" || type == "ntran" || type == "ptran") {
        c["inputCount"] = 2;
        outPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
    } else {
        int n = 0;
        if (type == "input" || type == "const" || type == "clock" || type == "not" ||
            type == "and" || type == "or" || type == "nand" ||
            type == "nor" || type == "xor" || type == "xnor") n = 1;
        for (int i = 0; i < n; ++i) outPorts.append(QString(bitWidth, '0'));
        int ic = 0;
        if (type == "not" || type == "led" || type == "output") ic = 1;
        else if (type == "and" || type == "or" || type == "nand" ||
                 type == "nor" || type == "xor" || type == "xnor") ic = inputCount;
        for (int i = 0; i < ic; ++i) inPorts.append(QString(bitWidth, '0'));
    }

    c["outputPorts"] = outPorts;
    c["inputPortValues"] = inPorts;

    ctx.components.append(c);
    if (type == "clock") m_clockStates[c["id"].toString()] = false;

    evaluateAll();
    emit changed();
    emit structureChanged();
    return c["id"].toString();
}

void Circuit::removeComponent(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    pushUndo();
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
    emit structureChanged();
}

void Circuit::removeComponents(const QStringList& ids) {
    if (m_viewOnly) return;
    if (ids.isEmpty()) return;
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);
    bool any = false;
    for (const auto& cv : currentCtx().components) {
        if (idSet.contains(cv.toMap().value("id").toString())) { any = true; break; }
    }
    if (!any) return;
    pushUndo();
    Context& ctx = currentCtx();
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        auto c = ctx.components[i].toMap();
        if (idSet.contains(c.value("id").toString())) {
            if (c.value("type").toString() == "clock")
                m_clockStates.remove(c.value("id").toString());
            ctx.components.removeAt(i);
        }
    }
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (idSet.contains(w.value("fromComp").toString()) ||
            idSet.contains(w.value("toComp").toString()))
            ctx.wires.removeAt(i);
    }
    evaluateAll();
    emit changed();
    emit structureChanged();
}

void Circuit::moveComponent(const QString& id, double x, double y) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    double ox = c.value("x").toDouble();
    double oy = c.value("y").toDouble();
    if (qAbs(ox - x) < 1e-9 && qAbs(oy - y) < 1e-9) return;
    pushUndo("move:" + id);
    c["x"] = x; c["y"] = y;
    ctx.components[idx] = c;
    emit geometryChanged();
}

void Circuit::moveSelection(const QStringList& ids, double dx, double dy) {
    if (m_viewOnly) return;
    if (ids.isEmpty()) return;
    if (qAbs(dx) < 1e-9 && qAbs(dy) < 1e-9) return;
    Context& ctx = currentCtx();
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);
    bool any = false;
    for (int i = 0; i < ctx.components.size(); ++i) {
        if (idSet.contains(ctx.components[i].toMap().value("id").toString())) { any = true; break; }
    }
    if (!any) return;
    pushUndo("moveSel");
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

    if (m_engineType == "event" && m_eventEngine) {
        QString binStr;
        binStr.reserve(bw);
        for (int i = 0; i < bw; ++i) binStr += v ? '1' : '0';
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else {
        evaluateAll();
    }
    emit changed();
}

void Circuit::setBitValue(const QString& id, int bit, bool value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "input" && t != "const") return;
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    if (bit < 0 || bit >= bits.size()) return;
    if (bits[bit].toBool() == value) return;
    bits[bit] = value;
    c["inputBits"] = bits;
    ctx.components[idx] = c;

    if (m_engineType == "event" && m_eventEngine) {
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputBit(qid, bit, value);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else {
        evaluateAll();
    }
    emit changed();
}

void Circuit::setComponentProp(const QString& id, const QString& key, const QVariant& value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (key == "bitWidth" || key == "inputCount" || key == "rotation") {
        if (c.value(key).toInt() == value.toInt()) return;
    } else {
        if (c.value(key) == value) return;
    }
    pushUndo("prop:" + id + ":" + key);
    bool structural = false;
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
                ns.reserve(bw);
                for (int i = 0; i < bw; ++i) ns.append(1);
                c["outputSplits"] = ns;
            }
        }
        structural = true;
    } else if (key == "inputCount") {
        int v = value.toInt();
        if (v < 2) v = 2;
        if (v > 64) v = 64;
        c[key] = v;
        structural = true;
    } else if (key == "rotation") {
        int v = value.toInt();
        v = ((v % 4) + 4) % 4;
        c["rotation"] = v;
        structural = true;
    } else {
        c[key] = value;
    }
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
    if (structural) emit structureChanged();
}

void Circuit::rotateComponent(const QString& id, int delta) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    int cur = c.value("rotation").toInt();
    int nw = ((cur + delta) % 4 + 4) % 4;
    if (nw == cur) return;
    pushUndo("rot:" + id);
    c["rotation"] = nw;
    ctx.components[idx] = c;
    emit changed();
    emit structureChanged();
}

void Circuit::renameComponent(const QString& id, const QString& name) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("name").toString() == name) return;
    pushUndo("rename:" + id);
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
    if (c.value("outputSplits").toList() == splits) return;
    pushUndo("splits:" + id);
    c["outputSplits"] = splits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
    emit structureChanged();
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

QString Circuit::addText(double x, double y, const QString& text) {
    QString id = addComponent("text", x, y, 1, 2, "");
    if (!id.isEmpty()) setTextContent(id, text);
    return id;
}

void Circuit::setTextContent(const QString& id, const QString& content) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "text") return;
    if (c.value("content").toString() == content) return;
    pushUndo("text:" + id);
    c["content"] = content;
    ctx.components[idx] = c;
    emit changed();
}
