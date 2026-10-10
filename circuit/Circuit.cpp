#include "Circuit.h"
#include "core/Component.h"
#include "logic/ComponentLogic.h"
#include "logic/event/EventEngine.h"
#include <QVariantMap>
#include <QString>
#include <QDebug>

static const char* kUndoPropName = "__logicsim_undo_v1";

Circuit::Circuit(QObject* parent) : QObject(parent) {
    CircuitUndoData* d = new CircuitUndoData();
    setProperty(kUndoPropName, QVariant::fromValue(static_cast<void*>(d)));

    loadRecentFiles();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &Circuit::tick);

    m_eventEngine = new EventEngine();

    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(16);
    connect(m_simTimer, &QTimer::timeout, this, &Circuit::onSimTimerTick);
}

Circuit::~Circuit() {
    CircuitUndoData* d = und();
    if (d) {
        delete d;
        setProperty(kUndoPropName, QVariant());
    }
    if (m_eventEngine) { delete m_eventEngine; m_eventEngine = nullptr; }
}

CircuitUndoData* Circuit::und() const {
    return static_cast<CircuitUndoData*>(property(kUndoPropName).value<void*>());
}

bool Circuit::canUndo() const {
    CircuitUndoData* d = und();
    return d && !d->undoStack.empty();
}

bool Circuit::canRedo() const {
    CircuitUndoData* d = und();
    return d && !d->redoStack.empty();
}

void Circuit::dumpInternalState(const QString& where) const { Q_UNUSED(where); }

Circuit::Context& Circuit::currentCtx() {
    if (m_currentCtxId.isEmpty()) return m_root;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return m_root;
    return it.value();
}
const Circuit::Context& Circuit::currentCtx() const {
    if (m_currentCtxId.isEmpty()) return m_root;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return m_root;
    return it.value();
}
Circuit::Context* Circuit::ctxById(const QString& id) {
    if (id.isEmpty()) return &m_root;
    auto it = m_subContexts.find(id);
    if (it == m_subContexts.end()) return nullptr;
    return &it.value();
}
const Circuit::Context* Circuit::ctxById(const QString& id) const {
    if (id.isEmpty()) return &m_root;
    auto it = m_subContexts.find(id);
    if (it == m_subContexts.end()) return nullptr;
    return &it.value();
}

QVariantList Circuit::components() const { return currentCtx().components; }
QVariantList Circuit::wires() const { return currentCtx().wires; }

QString Circuit::contextName() const {
    if (m_currentCtxId.isEmpty()) return "主电路";
    QString n = m_subNames.value(m_currentCtxId, "子电路");
    if (m_viewOnly) n += "（浏览）";
    return n;
}

int Circuit::indexOfComponent(const QString& id) const {
    return currentCtx().indexOfComponent(id);
}
int Circuit::indexOfWire(const QString& id) const {
    return currentCtx().indexOfWire(id);
}

qulonglong Circuit::simTime() const {
    return m_eventEngine ? m_eventEngine->time() : 0;
}
qulonglong Circuit::globalGateDelay() const {
    return m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
}
void Circuit::setGlobalGateDelay(qulonglong d) {
    if (!m_eventEngine) return;
    if (d < 1) d = 1;
    if (m_eventEngine->globalGateDelay() == d) return;
    m_eventEngine->setGlobalGateDelay(d);
    emit engineSettingsChanged();
}
qulonglong Circuit::wireDelay() const {
    return m_eventEngine ? m_eventEngine->wireDelay() : 1;
}
void Circuit::setWireDelay(qulonglong d) {
    if (!m_eventEngine) return;
    if (d < 1) d = 1;
    if (m_eventEngine->wireDelay() == d) return;
    m_eventEngine->setWireDelay(d);
    emit engineSettingsChanged();
}
void Circuit::setSimulationSpeed(qulonglong s) {
    if (m_simulationSpeed == s) return;
    m_simulationSpeed = s;
    emit engineSettingsChanged();
}
void Circuit::setSimulationSpeedExp(int e) {
    if (e < 0) e = 0;
    if (e > 12) e = 12;
    if (m_simulationSpeedExp == e) return;
    m_simulationSpeedExp = e;
    qulonglong v = 1;
    for (int i = 0; i < e; ++i) v *= 10;
    m_simulationSpeed = v;
    emit engineSettingsChanged();
}
void Circuit::setAutoSpeed(bool a) {
    if (m_autoSpeed == a) return;
    m_autoSpeed = a;
    emit engineSettingsChanged();
}
