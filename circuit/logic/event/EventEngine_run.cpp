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

    // 复用成员缓冲区，避免每事件堆分配
    QVector<QString>& driverVals = m_driverValBuf;
    driverVals.clear();
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

    // 记录输出变化的节点，供 Circuit 侧增量更新（去重）
    markDirty(n.qid);

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
    const int bw = n.bitWidth;
    const QString& t = n.type;

    // ---- 数据源相关类型：各自处理 ----
    if (t == "input" || t == "const" || t == "output" || t == "led") {
        QString s = inputs.isEmpty() ? QString(bw, '0') : inputs[0];
        if (s.length() != bw) {
            if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
            else                 s = s.right(bw);
        }
        return { s };
    }
    if (t == "clock") {
        if (!n.outVals.isEmpty()) return { n.outVals[0] };
        return { QString(bw, '0') };
    }

    // ---- 纯组合逻辑类型：委托 ComponentLogic 统一实现 ----
    // 覆盖 not/and/or/nand/nor/xor/xnor/tgate/ntran/ptran/splitter/hub
    // 未识别类型（如 sub）返回空，保持与旧行为一致（sub 节点输出由外部驱动）
    return ComponentLogic::computeCombinationalOutputs(
        t, inputs, bw, n.raw.value("outputSplits").toList());
}