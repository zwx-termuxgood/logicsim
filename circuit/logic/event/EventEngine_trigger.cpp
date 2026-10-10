#include "EventEngine.h"

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
