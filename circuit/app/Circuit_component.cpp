#include "../Circuit.h"
#include "../core/IdAllocator.h"
#include "../logic/ComponentLogic.h"
#include "../logic/event/EventEngine.h"
#include "../logic/move/MoveGesture.h"
#include "../logic/move/MoveResult.h"
#include <QVariantMap>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <QDebug>
#include <QPointF>
#include <QList>
#include <cmath>

namespace {

QString posKeyR(double x, double y) {
    return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
}

void collectPortWorldPositions(const QVariantMap& c, QSet<QString>& out) {
    double cx = c.value("x").toDouble();
    double cy = c.value("y").toDouble();
    QVariantList outs = ComponentLogic::outputPorts(c);
    for (const auto& pv : outs) {
        QVariantMap p = pv.toMap();
        out.insert(posKeyR(cx + p.value("x").toDouble(),
                           cy + p.value("y").toDouble()));
    }
    QVariantList ins = ComponentLogic::inputPorts(c);
    for (const auto& pv : ins) {
        QVariantMap p = pv.toMap();
        out.insert(posKeyR(cx + p.value("x").toDouble(),
                           cy + p.value("y").toDouble()));
    }
}

bool wireTouchesPositions(const QVariantMap& w, const QSet<QString>& positions) {
    QString wt = w.value("wireType").toString();
    if (wt != "ortho") return false;
    QVariantList path = w.value("path").toList();
    if (path.isEmpty()) return false;

    QList<QPointF> portPts;
    portPts.reserve(positions.size());
    for (const auto& key : positions) {
        QStringList parts = key.split(',');
        if (parts.size() != 2) continue;
        portPts.append(QPointF(parts[0].toDouble(), parts[1].toDouble()));
    }
    if (portPts.isEmpty()) return false;

    for (const auto& pv : path) {
        QVariantMap p = pv.toMap();
        double px = p.value("x").toDouble();
        double py = p.value("y").toDouble();
        for (const auto& pp : portPts) {
            if (qAbs(px - pp.x()) < 15.0 && qAbs(py - pp.y()) < 15.0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    if (m_viewOnly) return QString();
    if (bitWidth < 1) bitWidth = 1;
    if (bitWidth > 64) bitWidth = 64;
    if (inputCount < 2) inputCount = 2;
    if (inputCount > 64) inputCount = 64;
    if (type == "sub" && !canAddSubInstance(subId)) return QString();

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
    for (int i = 0; i < bitWidth; ++i) bits.append(false);
    c["inputBits"] = bits;

    QVariantList outPorts, inPorts;
    if (type == "text") c["content"] = "文本";
    else if (type == "clock") c["name"] = "CLK";
    else if (type == "const") c["name"] = "CONST";
    else if (type == "sub") {
        c["subId"] = subId;
        c["subInputNames"] = QVariantList();
        c["subOutputNames"] = QVariantList();
        c["subInputWidths"] = QVariantList();
        c["subOutputWidths"] = QVariantList();
    } else if (type == "splitter") {
        QVariantList splits;
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        inPorts.append(QString(bitWidth, '0'));
        for (int i = 0; i < splits.size(); ++i) {
            int w = splits[i].toInt(); if (w < 1) w = 1;
            outPorts.append(QString(w, '0'));
        }
    } else if (type == "hub") {
        QVariantList splits;
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        for (int i = 0; i < splits.size(); ++i) {
            int w = splits[i].toInt(); if (w < 1) w = 1;
            inPorts.append(QString(w, '0'));
        }
        outPorts.append(QString(bitWidth, '0'));
    } else if (type == "tgate") {
        c["inputCount"] = 3;
        outPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
    } else if (type == "ntran" || type == "ptran") {
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
    m_ctxSubInstCache.clear();
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

    QSet<QString> portPositions;
    collectPortWorldPositions(c, portPositions);

    if (c.value("type").toString() == "clock") m_clockStates.remove(id);
    ctx.components.removeAt(idx);
    m_ctxSubInstCache.clear();

    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        bool shouldRemove = false;
        if (w.value("fromComp").toString() == id ||
            w.value("toComp").toString() == id) {
            shouldRemove = true;
        } else if (wireTouchesPositions(w, portPositions)) {
            shouldRemove = true;
        }
        if (shouldRemove) ctx.wires.removeAt(i);
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

    QSet<QString> portPositions;
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        QVariantMap c = ctx.components[i].toMap();
        if (!idSet.contains(c.value("id").toString())) continue;
        collectPortWorldPositions(c, portPositions);
        if (c.value("type").toString() == "clock")
            m_clockStates.remove(c.value("id").toString());
        ctx.components.removeAt(i);
    }

    m_ctxSubInstCache.clear();

    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        bool shouldRemove = false;
        if (idSet.contains(w.value("fromComp").toString()) ||
            idSet.contains(w.value("toComp").toString())) {
            shouldRemove = true;
        } else if (wireTouchesPositions(w, portPositions)) {
            shouldRemove = true;
        }
        if (shouldRemove) ctx.wires.removeAt(i);
    }

    evaluateAll();
    emit changed();
    emit structureChanged();
}

void Circuit::updateWiresForMovedComponent(Context& ctx, const QString& movedId) {
    for (int i = 0; i < ctx.wires.size(); ++i) {
        QVariantMap w = ctx.wires[i].toMap();
        if (w.value("wireType").toString() != "ortho") continue;
        QString fromComp = w.value("fromComp").toString();
        QString toComp = w.value("toComp").toString();
        bool fromIsMoved = (fromComp == movedId);
        bool toIsMoved = (toComp == movedId);
        if (!fromIsMoved && !toIsMoved) continue;

        QVariantList path = w.value("path").toList();
        if (path.size() < 2) continue;

        auto getNewPortPos = [&](const QString& compId, int port, bool isOut) -> QPointF {
            int ci = ctx.indexOfComponent(compId);
            if (ci < 0) return QPointF();
            QVariantMap c = ctx.components[ci].toMap();
            QVariantList ports = isOut ? ComponentLogic::outputPorts(c) : ComponentLogic::inputPorts(c);
            if (port < 0 || port >= ports.size()) return QPointF();
            QVariantMap p = ports[port].toMap();
            return QPointF(c.value("x").toDouble() + p.value("x").toDouble(),
                           c.value("y").toDouble() + p.value("y").toDouble());
        };

        if (fromIsMoved) {
            int fp = w.value("fromPort").toInt();
            QPointF np = getNewPortPos(movedId, fp, true);
            QVariantMap oldP0 = path[0].toMap();
            double oldX = oldP0.value("x").toDouble();
            QVariantMap newP0;
            newP0["x"] = np.x(); newP0["y"] = np.y();
            path[0] = newP0;

            if (path.size() >= 2) {
                QVariantMap p1 = path[1].toMap();
                double p1x = p1.value("x").toDouble();
                bool wasVertical = (qAbs(p1x - oldX) < 0.5);
                if (wasVertical) p1["x"] = np.x();
                else p1["y"] = np.y();
                path[1] = p1;
            }
        }

        if (toIsMoved) {
            int tp = w.value("toPort").toInt();
            QPointF np = getNewPortPos(movedId, tp, false);
            int n = path.size();
            QVariantMap oldPn = path[n-1].toMap();
            double oldX = oldPn.value("x").toDouble();
            QVariantMap newPn;
            newPn["x"] = np.x(); newPn["y"] = np.y();
            path[n-1] = newPn;

            if (n >= 2) {
                QVariantMap pn1 = path[n-2].toMap();
                double pn1x = pn1.value("x").toDouble();
                bool wasVertical = (qAbs(pn1x - oldX) < 0.5);
                if (wasVertical) pn1["x"] = np.x();
                else pn1["y"] = np.y();
                path[n-2] = pn1;
            }
        }

        w["path"] = path;
        ctx.wires[i] = w;
    }
}

void Circuit::moveComponent(const QString& id, double x, double y) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    double ox = c.value("x").toDouble(), oy = c.value("y").toDouble();
    if (qAbs(ox - x) < 1e-9 && qAbs(oy - y) < 1e-9) return;
    pushUndo("move:" + id);
    c["x"] = x; c["y"] = y;
    ctx.components[idx] = c;
    updateWiresForMovedComponent(ctx, id);
    evaluateAll();
    emit geometryChanged();
    emit changed();
    emit structureChanged();
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
    for (const auto& id : ids) {
        if (ctx.indexOfComponent(id) >= 0) {
            updateWiresForMovedComponent(ctx, id);
        }
    }
    evaluateAll();
    emit geometryChanged();
    emit changed();
    emit structureChanged();
}

void Circuit::moveComponentWithWires(const QString& id, double x, double y) {
    moveComponent(id, x, y);
}

void Circuit::moveSelectionWithWires(const QStringList& ids, double dx, double dy) {
    moveSelection(ids, dx, dy);
}

// ★ 修复：rebuild 之后必须先跑完初始 Recalc 事件，再 apply
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

    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        if (m_engineDirty) {
            rebuildEventEngine();
            quint64 wd0 = m_eventEngine->wireDelay();
            quint64 gd0 = m_eventEngine->globalGateDelay();
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            applyEngineOutputsToContexts();
        }
        QString binStr;
        binStr.reserve(bw);
        for (int i = 0; i < bw; ++i) binStr += v ? '1' : '0';
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else if (m_engineType == "event") {
        m_engineDirty = true;
    } else {
        evaluateAll();
    }
    emit changed();
    emit structureChanged();
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

    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        if (m_engineDirty) {
            rebuildEventEngine();
            quint64 wd0 = m_eventEngine->wireDelay();
            quint64 gd0 = m_eventEngine->globalGateDelay();
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            applyEngineOutputsToContexts();
        }
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputBit(qid, bit, value);
        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else if (m_engineType == "event") {
        m_engineDirty = true;
    } else {
        evaluateAll();
    }
    emit changed();
    emit structureChanged();
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