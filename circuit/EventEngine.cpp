#include "EventEngine.h"
#include "ComponentTraits.h"
#include <QDebug>
#include <algorithm>

EventEngine::EventEngine() {}
EventEngine::~EventEngine() {}

void EventEngine::setGlobalGateDelay(quint64 d) {
    if (d < 1) d = 1;
    m_opts.globalGateDelay = d;
}
void EventEngine::setWireDelay(quint64 d) {
    if (d < 1) d = 1;
    m_opts.wireDelay = d;
}
void EventEngine::setGateTypeDelay(const QString& type, quint64 d) {
    if (d < 1) d = 1;
    m_gateDelays[type] = d;
}
void EventEngine::removeGateTypeDelay(const QString& type) {
    m_gateDelays.remove(type);
}
void EventEngine::clearGateTypeDelays() {
    m_gateDelays.clear();
}

quint64 EventEngine::gateDelayFor(const QString& type) const {
    auto it = m_gateDelays.constFind(type);
    if (it != m_gateDelays.constEnd()) return it.value();
    return m_opts.globalGateDelay;
}

void EventEngine::clear() {
    m_nodes.clear();
    m_idToIdx.clear();
    m_inDrivers.clear();
    m_outDests.clear();
    m_portDriverValues.clear();
    while (!m_queue.empty()) m_queue.pop();
    m_time = 0;
    m_seq = 0;
    m_totalEventsProcessed = 0;
}

void EventEngine::reset() {
    QVector<QPair<int, QString>> savedInputs;
    savedInputs.reserve(m_nodes.size());
    for (int i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        if (n.isInput && !n.outVals.isEmpty())
            savedInputs.append(qMakePair(i, n.outVals[0]));
    }

    while (!m_queue.empty()) m_queue.pop();
    m_time = 0;
    m_seq = 0;
    m_totalEventsProcessed = 0;

    for (auto& n : m_nodes) {
        for (auto& s : n.inVals)  s = QString(s.length(), '0');
        for (auto& s : n.outVals) s = QString(s.length(), '0');
    }
    for (auto it = m_portDriverValues.begin(); it != m_portDriverValues.end(); ++it)
        it.value().clear();

    initValues();

    for (const auto& p : savedInputs) {
        int i = p.first;
        const QString& v = p.second;
        if (i < 0 || i >= m_nodes.size()) continue;
        if (!m_nodes[i].outVals.isEmpty()) m_nodes[i].outVals[0] = v;
        if (!m_nodes[i].inVals.isEmpty())  m_nodes[i].inVals[0]  = v;
    }

    initDriverValues();
}

QString EventEngine::resolveMultiDriver(const QVector<QString>& vals, int bw) {
    if (bw < 1) bw = 1;
    if (vals.isEmpty()) return QString(bw, '0');

    if (vals.size() == 1) {
        QString s = vals[0];
        if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
        else if (s.length() > bw) s = s.right(bw);
        return s;
    }

    QString result(bw, '0');
    for (int pos = 0; pos < bw; ++pos) {
        bool has0 = false, has1 = false, hasE = false;
        for (const auto& v : vals) {
            QString s = v;
            if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
            else if (s.length() > bw) s = s.right(bw);
            QChar c = (pos < s.length()) ? s[pos] : QChar('0');
            if (c == '0') has0 = true;
            else if (c == '1') has1 = true;
            else if (c == 'E') hasE = true;
        }
        if (hasE)              result[pos] = 'E';
        else if (has0 && has1) result[pos] = 'E';
        else if (has1)         result[pos] = '1';
        else if (has0)         result[pos] = '0';
        else                   result[pos] = 'Z';
    }
    return result;
}

QString EventEngine::mapUnary(const QString& src, int bw) {
    QString out; out.reserve(bw);
    int n = src.length();
    for (int b = 0; b < bw; ++b) {
        QChar c = (b < n) ? src[b] : QChar('0');
        if (c == 'Z' || c == 'E') out += 'E';
        else                       out += (c == '1') ? '0' : '1';
    }
    return out;
}

void EventEngine::build(const QVariantList& rootComps,
                        const QVariantList& rootWires,
                        const QHash<QString, QVariantList>& subComps,
                        const QHash<QString, QVariantList>& subWires)
{
    clear();
    buildRecursive(rootComps, rootWires, subComps, subWires, QString(), 0);
    initValues();
    initDriverValues();
}

void EventEngine::buildRecursive(const QVariantList& comps,
                                 const QVariantList& wires,
                                 const QHash<QString, QVariantList>& subComps,
                                 const QHash<QString, QVariantList>& subWires,
                                 const QString& prefix,
                                 int depth)
{
    if (depth > 32) return;

    QHash<QString, int> localIdx;
    localIdx.reserve(comps.size() * 2);
    for (const auto& cv : comps) {
        QVariantMap c = cv.toMap();
        QString type = c.value("type").toString();
        if (type == "text") continue;

        QString localId = c.value("id").toString();
        QString qid = prefix.isEmpty() ? localId : (prefix + "/" + localId);
        if (m_idToIdx.contains(qid)) continue;

        Node n;
        n.qid = qid;
        n.localId = localId;
        n.type = type;
        n.bitWidth = c.value("bitWidth").toInt();
        if (n.bitWidth < 1) n.bitWidth = 1;
        n.raw = c;
        if (type == "input" || type == "const") n.isInput  = true;
        else if (type == "clock")  n.isClock  = true;
        else if (type == "output") n.isOutput = true;
        else if (type == "led")    n.isLed    = true;

        if (type == "input" || type == "const" || type == "output") {
            n.inVals.append(QString(n.bitWidth, '0'));
            n.outVals.append(QString(n.bitWidth, '0'));
        } else if (type == "led") {
            int ic = ComponentTraits::inputCount(c);
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentTraits::inputBitWidth(c, i), '0'));
        } else {
            int ic = ComponentTraits::inputCount(c);
            int oc = ComponentTraits::outputCount(c);
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentTraits::inputBitWidth(c, i), '0'));
            for (int i = 0; i < oc; ++i)
                n.outVals.append(QString(ComponentTraits::outputBitWidth(c, i), '0'));
        }

        int idx = m_nodes.size();
        m_idToIdx[qid] = idx;
        localIdx[localId] = idx;
        m_nodes.append(n);
    }

    for (const auto& cv : comps) {
        QVariantMap c = cv.toMap();
        if (c.value("type").toString() != "sub") continue;
        QString localId = c.value("id").toString();
        QString qid = prefix.isEmpty() ? localId : (prefix + "/" + localId);
        QString subId = c.value("subId").toString();
        auto sit = subComps.constFind(subId);
        if (sit == subComps.constEnd()) continue;
        buildRecursive(sit.value(), subWires.value(subId),
                       subComps, subWires, qid, depth + 1);
    }

    for (const auto& wv : wires) {
        QVariantMap w = wv.toMap();
        QString fromLocal = w.value("fromComp").toString();
        QString toLocal   = w.value("toComp").toString();
        int fromPort = w.value("fromPort").toInt();
        int toPort   = w.value("toPort").toInt();

        int fi = localIdx.value(fromLocal, -1);
        int ti = localIdx.value(toLocal, -1);
        if (fi < 0 || ti < 0) continue;

        if (m_nodes[fi].type == "sub") {
            QString subId = m_nodes[fi].raw.value("subId").toString();
            QString subQid = m_nodes[fi].qid;
            auto sit = subComps.constFind(subId);
            if (sit == subComps.constEnd()) continue;
            int cnt = 0;
            QString innerQid;
            for (const auto& scv : sit.value()) {
                QVariantMap sc = scv.toMap();
                if (sc.value("type").toString() != "output") continue;
                if (cnt == fromPort) {
                    innerQid = subQid + "/" + sc.value("id").toString();
                    break;
                }
                cnt++;
            }
            if (innerQid.isEmpty()) continue;
            int innerIdx = idxOf(innerQid);
            if (innerIdx < 0) continue;
            fi = innerIdx;
            fromPort = 0;
        }
        if (m_nodes[ti].type == "sub") {
            QString subId = m_nodes[ti].raw.value("subId").toString();
            QString subQid = m_nodes[ti].qid;
            auto sit = subComps.constFind(subId);
            if (sit == subComps.constEnd()) continue;
            int cnt = 0;
            QString innerQid;
            for (const auto& scv : sit.value()) {
                QVariantMap sc = scv.toMap();
                if (sc.value("type").toString() != "input") continue;
                if (cnt == toPort) {
                    innerQid = subQid + "/" + sc.value("id").toString();
                    break;
                }
                cnt++;
            }
            if (innerQid.isEmpty()) continue;
            int innerIdx = idxOf(innerQid);
            if (innerIdx < 0) continue;
            ti = innerIdx;
            toPort = 0;
        }

        m_outDests[pkey(fi, fromPort)].append(qMakePair(ti, toPort));
        m_inDrivers[pkey(ti, toPort)].append({fi, fromPort});
    }
}

void EventEngine::initValues() {
    for (int i = 0; i < m_nodes.size(); ++i) {
        Node& n = m_nodes[i];
        if (n.isInput) {
            QVariantList bits = n.raw.value("inputBits").toList();
            QString s;
            s.reserve(n.bitWidth);
            for (int b = 0; b < n.bitWidth; ++b)
                s += (b < bits.size() && bits[b].toBool()) ? '1' : '0';
            if (!n.outVals.isEmpty()) n.outVals[0] = s;
            if (!n.inVals.isEmpty())  n.inVals[0]  = s;
        } else if (n.isClock) {
            QVariantList ops = n.raw.value("outputPorts").toList();
            QString s = ops.isEmpty() ? QString(n.bitWidth, '0')
                                      : ops[0].toString();
            if (s.length() != n.bitWidth) s = QString(n.bitWidth, '0');
            if (!n.outVals.isEmpty()) n.outVals[0] = s;
            if (!n.inVals.isEmpty())  n.inVals[0]  = s;
        }
    }
}

void EventEngine::initDriverValues() {
    for (auto it = m_inDrivers.constBegin(); it != m_inDrivers.constEnd(); ++it) {
        quint64 tkey = it.key();
        QHash<qint64, QString> dv;
        for (const auto& d : it.value()) {
            if (d.fromIdx < 0 || d.fromIdx >= m_nodes.size()) continue;
            const Node& fn = m_nodes[d.fromIdx];
            QString v;
            if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                v = fn.outVals[d.fromPort];
            if (v.isEmpty()) {
                int w = ComponentTraits::outputBitWidth(fn.raw, d.fromPort);
                if (w < 1) w = 1;
                v = QString(w, '0');
            }
            dv[dkey(d.fromIdx, d.fromPort)] = v;
        }
        m_portDriverValues[tkey] = dv;
    }

    for (auto it = m_inDrivers.constBegin(); it != m_inDrivers.constEnd(); ++it) {
        quint64 tkey = it.key();
        quint32 lo = quint32(tkey & 0xFFFFFFFF);
        int ti = int(lo >> 16);
        int tp = int(tkey & 0xFFFF);
        if (ti < 0 || ti >= m_nodes.size()) continue;
        Node& tn = m_nodes[ti];
        if (tp < 0 || tp >= tn.inVals.size()) continue;

        QVector<QString> driverVals;
        driverVals.reserve(it.value().size());
        for (const auto& d : it.value()) {
            QString v = m_portDriverValues[tkey].value(
                dkey(d.fromIdx, d.fromPort), QString());
            if (v.isEmpty()) {
                const Node& fn = m_nodes[d.fromIdx];
                if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                    v = fn.outVals[d.fromPort];
            }
            driverVals.append(v);
        }
        int bw = tn.inVals[tp].length();
        if (bw < 1) bw = tn.bitWidth;
        tn.inVals[tp] = resolveMultiDriver(driverVals, bw);
    }

    for (int pass = 0; pass < 32; ++pass) {
        bool anyChanged = false;
        for (int i = 0; i < m_nodes.size(); ++i) {
            Node& n = m_nodes[i];
            if (n.isInput || n.isClock) continue;

            QVector<QString> newOuts = computeOutputs(n, n.inVals);
            bool changed = (newOuts != n.outVals);
            if (!changed) continue;

            n.outVals = newOuts;
            anyChanged = true;

            for (int p = 0; p < newOuts.size(); ++p) {
                auto it = m_outDests.constFind(pkey(i, p));
                if (it == m_outDests.constEnd()) continue;
                for (const auto& d : it.value()) {
                    int ti = d.first;
                    int tp = d.second;
                    if (ti < 0 || ti >= m_nodes.size()) continue;
                    Node& tn = m_nodes[ti];
                    if (tp < 0 || tp >= tn.inVals.size()) continue;

                    quint64 tkey = pkey(ti, tp);
                    qint64  dk   = dkey(i, p);
                    m_portDriverValues[tkey][dk] = newOuts[p];

                    QVector<QString> driverVals;
                    driverVals.reserve(it.value().size());
                    auto it2 = m_inDrivers.constFind(tkey);
                    if (it2 != m_inDrivers.constEnd()) {
                        for (const auto& dd : it2.value()) {
                            QString v = m_portDriverValues[tkey].value(
                                dkey(dd.fromIdx, dd.fromPort), QString());
                            if (v.isEmpty()) {
                                const Node& fn = m_nodes[dd.fromIdx];
                                if (dd.fromPort >= 0 && dd.fromPort < fn.outVals.size())
                                    v = fn.outVals[dd.fromPort];
                            }
                            driverVals.append(v);
                        }
                    }
                    int bw = tn.inVals[tp].length();
                    if (bw < 1) bw = tn.bitWidth;
                    tn.inVals[tp] = resolveMultiDriver(driverVals, bw);
                }
            }
        }
        if (!anyChanged) break;
    }

    for (int i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        if (n.isInput || n.isClock) continue;
        scheduleRecalc(i, m_time);
    }
}

void EventEngine::scheduleRecalc(int idx, quint64 at) {
    Event ev;
    ev.kind = Event::Recalc;
    ev.time = at;
    ev.seq = ++m_seq;
    ev.nodeIdx = idx;
    ev.port = 0;
    ev.toIdx = -1;
    ev.toPort = -1;
    m_queue.push(ev);
}

void EventEngine::scheduleWirePropagation(int fromIdx, int fromPort,
                                          int toIdx, int toPort,
                                          const QString& value, quint64 at) {
    Event ev;
    ev.kind = Event::Wire;
    ev.time = at;
    ev.seq = ++m_seq;
    ev.nodeIdx = fromIdx;
    ev.port = fromPort;
    ev.toIdx = toIdx;
    ev.toPort = toPort;
    ev.value = value;
    m_queue.push(ev);
}

bool EventEngine::step() {
    if (m_queue.empty()) return false;
    Event ev = m_queue.top();
    m_queue.pop();
    if (ev.time > m_time) m_time = ev.time;
    processEvent(ev);
    ++m_totalEventsProcessed;
    return true;
}

int EventEngine::drainNow(quint64 budget) {
    if (budget == 0) budget = m_opts.runBudget;
    int processed = 0;
    quint64 startTime = m_time;
    while (!m_queue.empty()) {
        if (m_queue.top().time > startTime) break;
        if (quint64(processed) >= budget) break;
        Event ev = m_queue.top();
        m_queue.pop();
        processEvent(ev);
        ++processed;
        ++m_totalEventsProcessed;
    }
    return processed;
}

bool EventEngine::run(quint64 units, quint64 budget) {
    if (budget == 0) budget = m_opts.runBudget;
    quint64 endTime = m_time + units;
    quint64 processed = 0;

    while (!m_queue.empty()) {
        if (m_queue.top().time > endTime) {
            if (m_time < endTime) m_time = endTime;
            return true;
        }
        if (processed >= budget) {
            return false;
        }
        Event ev = m_queue.top();
        m_queue.pop();
        if (ev.time > m_time) m_time = ev.time;
        processEvent(ev);
        ++processed;
        ++m_totalEventsProcessed;
    }

    if (m_time < endTime) m_time = endTime;
    return true;
}

void EventEngine::processEvent(const Event& ev) {
    if (ev.kind == Event::Recalc) {
        recalcNode(ev.nodeIdx);
        return;
    }

    int ti = ev.toIdx;
    int tp = ev.toPort;
    if (ti < 0 || ti >= m_nodes.size()) return;
    Node& tn = m_nodes[ti];
    if (tp < 0 || tp >= tn.inVals.size()) return;

    quint64 tkey = pkey(ti, tp);
    qint64  dk   = dkey(ev.nodeIdx, ev.port);
    QHash<qint64, QString>& dv = m_portDriverValues[tkey];
    dv[dk] = ev.value;

    QVector<QString> driverVals;
    auto it = m_inDrivers.constFind(tkey);
    if (it != m_inDrivers.constEnd()) {
        driverVals.reserve(it.value().size());
        for (const auto& d : it.value()) {
            qint64 k = dkey(d.fromIdx, d.fromPort);
            QString v = dv.value(k, QString());
            if (v.isEmpty()) {
                const Node& fn = m_nodes[d.fromIdx];
                if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                    v = fn.outVals[d.fromPort];
            }
            driverVals.append(v);
        }
    } else {
        driverVals.append(ev.value);
    }

    int bw = tn.inVals[tp].length();
    if (bw < 1) bw = tn.bitWidth;
    QString merged = resolveMultiDriver(driverVals, bw);

    if (tn.inVals[tp] == merged) return;
    tn.inVals[tp] = merged;
    recalcNode(ti);
}

void EventEngine::recalcNode(int idx) {
    Node& n = m_nodes[idx];
    QVector<QString> newOuts = computeOutputs(n, n.inVals);
    bool changed = false;
    if (newOuts.size() != n.outVals.size()) {
        n.outVals = newOuts;
        changed = true;
    } else {
        for (int i = 0; i < newOuts.size(); ++i) {
            if (n.outVals[i] != newOuts[i]) { n.outVals[i] = newOuts[i]; changed = true; }
        }
    }
    if (!changed) return;

    quint64 at = m_time + gateDelayFor(n.type);
    for (int p = 0; p < n.outVals.size(); ++p) {
        auto it = m_outDests.constFind(pkey(idx, p));
        if (it == m_outDests.constEnd()) continue;
        const QString v = n.outVals[p];
        for (const auto& d : it.value()) {
            scheduleWirePropagation(idx, p, d.first, d.second, v, at);
        }
    }
}

QVector<QString> EventEngine::computeOutputs(const Node& n,
                                             const QVector<QString>& inputs) const
{
    QVector<QString> outs;
    int bw = n.bitWidth;
    const QString& t = n.type;

    auto inAt = [&](int i, int w) -> QString {
        if (i < inputs.size()) {
            QString s = inputs[i];
            if (s.length() != w) {
                if (s.length() < w) s = QString(w - s.length(), '0') + s;
                else s = s.right(w);
            }
            return s;
        }
        return QString(w, '0');
    };

    if (t == "input" || t == "const") {
        outs.append(inAt(0, bw));
    } else if (t == "clock") {
        if (!n.outVals.isEmpty()) outs.append(n.outVals[0]);
        else                      outs.append(QString(bw, '0'));
    } else if (t == "output" || t == "led") {
        outs.append(inAt(0, bw));
    } else if (t == "not") {
        outs.append(mapUnary(inAt(0, bw), bw));
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor") {
        int ic = inputs.size();
        const bool isAnd   = (t == "and" || t == "nand");
        const bool isOr    = (t == "or"  || t == "nor");
        const bool needNot = (t == "nand" || t == "nor" || t == "xnor");
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool hasErr = false;
            for (int p = 0; p < ic; ++p) {
                QChar c = (b < inputs[p].length()) ? inputs[p][b] : QChar('0');
                if (c == 'Z' || c == 'E') { hasErr = true; break; }
            }
            if (hasErr) { out += 'E'; continue; }
            bool r = isAnd ? true : false;
            for (int p = 0; p < ic; ++p) {
                bool v = (b < inputs[p].length() && inputs[p][b] == '1');
                if (isAnd)      r = r && v;
                else if (isOr)  r = r || v;
                else            r = r != v;
            }
            if (needNot) r = !r;
            out += r ? '1' : '0';
        }
        outs.append(out);
    } else if (t == "tgate") {
        QString d = inAt(0, bw);
        QString e = inAt(1, bw);
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            QChar ev = (b < e.length()) ? e[b] : QChar('0');
            if (ev == 'Z' || ev == 'E') out += 'E';
            else if (ev == '1')         out += dv;
            else                        out += 'Z';
        }
        outs.append(out);
    } else if (t == "ntran") {
        QString g = inAt(0, bw);
        QString d = inAt(1, bw);
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar gv = (b < g.length()) ? g[b] : QChar('0');
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            if (gv == 'Z' || gv == 'E') out += 'E';
            else if (gv == '1')         out += dv;
            else                        out += 'Z';
        }
        outs.append(out);
    } else if (t == "ptran") {
        QString g = inAt(0, bw);
        QString d = inAt(1, bw);
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar gv = (b < g.length()) ? g[b] : QChar('0');
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            if (gv == 'Z' || gv == 'E') out += 'E';
            else if (gv == '0')         out += dv;
            else                        out += 'Z';
        }
        outs.append(out);
    } else if (t == "splitter") {
        QString mainIn;
        if (!inputs.isEmpty()) mainIn = inputs[0];
        while (mainIn.length() < bw) mainIn += '0';
        if (mainIn.length() > bw) mainIn = mainIn.left(bw);

        QVariantList splits = n.raw.value("outputSplits").toList();
        int offset = bw;
        for (auto& sv : splits) {
            int len = sv.toInt();
            if (len < 1) len = 1;
            offset -= len;
            if (offset < 0) offset = 0;
            QString out; out.reserve(len);
            for (int b = 0; b < len; ++b)
                out += (offset + b < mainIn.length()) ? mainIn[offset + b] : QChar('0');
            outs.append(out);
        }
    } else if (t == "hub") {
        QString merged; merged.reserve(bw + 8);
        for (int s = inputs.size() - 1; s >= 0; --s) {
            int len = ComponentTraits::inputBitWidth(n.raw, s);
            if (len < 1) len = 1;
            QString sv = (s < inputs.size()) ? inputs[s] : QString(len, '0');
            while (sv.length() < len) sv += '0';
            if (sv.length() > len) sv = sv.left(len);
            merged += sv;
        }
        while (merged.length() < bw) merged += '0';
        if (merged.length() > bw) merged = merged.left(bw);
        outs.append(merged);
    }

    return outs;
}

void EventEngine::triggerInputBit(const QString& compId, int bit, bool value) {
    int idx = idxOf(compId);
    if (idx < 0) return;
    Node& n = m_nodes[idx];
    if (!n.isInput) return;
    QString s = n.outVals.isEmpty() ? QString(n.bitWidth, '0') : n.outVals[0];
    if (s.length() != n.bitWidth) s = QString(n.bitWidth, '0');
    int pos = bit;
    if (pos < 0 || pos >= s.length()) return;
    s[pos] = value ? '1' : '0';
    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    {
        QVariantList bits;
        bits.reserve(s.length());
        for (int i = 0; i < s.length(); ++i) bits.append(s[i] == '1');
        n.raw["inputBits"] = bits;
    }

    quint64 at = m_time + m_opts.wireDelay;
    auto it = m_outDests.constFind(pkey(idx, 0));
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

void EventEngine::triggerInputString(const QString& compId, const QString& binStr) {
    int idx = idxOf(compId);
    if (idx < 0) return;
    Node& n = m_nodes[idx];
    if (!n.isInput) return;
    QString s = binStr;
    while (s.length() < n.bitWidth) s = "0" + s;
    if (s.length() > n.bitWidth) s = s.right(n.bitWidth);
    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    {
        QVariantList bits;
        bits.reserve(s.length());
        for (int i = 0; i < s.length(); ++i) bits.append(s[i] == '1');
        n.raw["inputBits"] = bits;
    }

    quint64 at = m_time + m_opts.wireDelay;
    auto it = m_outDests.constFind(pkey(idx, 0));
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

void EventEngine::triggerClockToggle(const QString& compId) {
    int idx = idxOf(compId);
    if (idx < 0) return;
    Node& n = m_nodes[idx];
    if (!n.isClock) return;

    QString s;
    if (!n.outVals.isEmpty() && n.outVals[0].length() == n.bitWidth)
        s = n.outVals[0];
    else
        s = QString(n.bitWidth, '0');

    for (int i = 0; i < s.length(); ++i)
        s[i] = (s[i] == '1') ? '0' : '1';

    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    {
        QVariantList ops;
        ops.append(s);
        n.raw["outputPorts"] = ops;
    }

    quint64 at = m_time + m_opts.wireDelay;
    auto it = m_outDests.constFind(pkey(idx, 0));
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

void EventEngine::triggerForceOutput(const QString& compId, int port, const QString& value) {
    int idx = idxOf(compId);
    if (idx < 0) return;
    Node& n = m_nodes[idx];
    if (port < 0 || port >= n.outVals.size()) return;

    if (n.isClock && port == 0) {
        QVariantList ops;
        ops.append(value);
        n.raw["outputPorts"] = ops;
    }

    if (n.outVals[port] == value) return;
    n.outVals[port] = value;

    quint64 at = m_time + m_opts.wireDelay;
    auto it = m_outDests.constFind(pkey(idx, port));
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, port, d.first, d.second, value, at);
    }
}

QString EventEngine::getOutput(const QString& compId, int port) const {
    int idx = idxOf(compId);
    if (idx < 0) return QString();
    const Node& n = m_nodes[idx];
    if (port < 0 || port >= n.outVals.size()) return QString();
    return n.outVals[port];
}

QStringList EventEngine::getOutputs(const QString& compId) const {
    int idx = idxOf(compId);
    if (idx < 0) return QStringList();
    const Node& n = m_nodes[idx];
    QStringList out;
    out.reserve(n.outVals.size());
    for (const auto& s : n.outVals) out << s;
    return out;
}