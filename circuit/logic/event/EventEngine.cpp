#include "EventEngine.h"
#include "../ComponentLogic.h"
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
