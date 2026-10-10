#include "EventEngine.h"
#include "../ComponentLogic.h"
#include <algorithm>

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
            int len = ComponentLogic::inputBitWidth(n.raw, s);
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
