#include "EventEngine.h"
#include "../ComponentLogic.h"
#include <algorithm>

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
            int ic = ComponentLogic::inputCount(c);
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentLogic::inputBitWidth(c, i), '0'));
        } else {
            int ic = ComponentLogic::inputCount(c);
            int oc = ComponentLogic::outputCount(c);
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentLogic::inputBitWidth(c, i), '0'));
            for (int i = 0; i < oc; ++i)
                n.outVals.append(QString(ComponentLogic::outputBitWidth(c, i), '0'));
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
                int w = ComponentLogic::outputBitWidth(fn.raw, d.fromPort);
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
