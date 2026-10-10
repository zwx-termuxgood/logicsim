#include "circuit.h"
#include "ComponentTraits.h"
#include "CircuitEvaluator.h"
#include "EventEngine.h"
#include <QVariantMap>
#include <QString>
#include <QDebug>

static const char* kUndoPropName = "__logicsim_undo_v1";
static const quint64 kSimRunBudget = 200000;

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
    return static_cast<CircuitUndoData*>(
        property(kUndoPropName).value<void*>());
}

bool Circuit::canUndo() const {
    CircuitUndoData* d = und();
    return d && !d->undoStack.empty();
}

bool Circuit::canRedo() const {
    CircuitUndoData* d = und();
    return d && !d->redoStack.empty();
}

void Circuit::dumpInternalState(const QString& where) const {
    Q_UNUSED(where);
}

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

QVariantList Circuit::subcircuits() const {
    QVariantList out;
    for (auto it = m_subNames.begin(); it != m_subNames.end(); ++it) {
        QVariantMap m;
        m["id"] = it.key();
        m["name"] = it.value();
        const Context* ctx = ctxById(it.key());
        int inCount = 0, outCount = 0;
        if (ctx) {
            for (auto& cv : ctx->components) {
                QString t = cv.toMap().value("type").toString();
                if (t == "input") inCount++;
                else if (t == "output") outCount++;
            }
        }
        m["inCount"] = inCount;
        m["outCount"] = outCount;
        out.append(m);
    }
    return out;
}

QVariantList Circuit::editContexts() const {
    QVariantList out;
    QVariantMap rootMap;
    rootMap["id"] = "";
    rootMap["name"] = "主电路";
    out.append(rootMap);
    QStringList ids = m_subNames.keys();
    std::sort(ids.begin(), ids.end());
    for (auto& id : ids) {
        QVariantMap m;
        m["id"] = id;
        m["name"] = m_subNames.value(id);
        out.append(m);
    }
    return out;
}

int Circuit::indexOfComponent(const QString& id) const {
    return currentCtx().indexOfComponent(id);
}

int Circuit::indexOfWire(const QString& id) const {
    return currentCtx().indexOfWire(id);
}

int Circuit::componentInputCount(const QVariantMap& comp) {
    return ComponentTraits::inputCount(comp);
}
int Circuit::componentOutputCount(const QVariantMap& comp) {
    return ComponentTraits::outputCount(comp);
}
int Circuit::outputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentTraits::outputBitWidth(comp, portIdx);
}
int Circuit::inputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentTraits::inputBitWidth(comp, portIdx);
}

void Circuit::setEngineType(const QString& t) {
    if (m_engineType == t) return;
    if (t != "iter" && t != "event") return;

    if (m_simulationRunning) pauseSimulation();
    if (m_clockRunning) stopClock();

    m_engineType = t;

    if (m_engineType == "event") {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    } else {
        applyEngineOutputsToContexts();
        evaluateAll();
    }

    emit engineChanged();
    emit engineSettingsChanged();
    emit changed();
    emit structureChanged();
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

void Circuit::setGateTypeDelay(const QString& type, qulonglong d) {
    if (!m_eventEngine) return;
    m_eventEngine->setGateTypeDelay(type, d);
    emit engineSettingsChanged();
    emit changed();
}
qulonglong Circuit::getGateTypeDelay(const QString& type) const {
    if (!m_eventEngine) return 0;
    return m_eventEngine->gateDelayFor(type);
}
void Circuit::clearGateTypeDelays() {
    if (!m_eventEngine) return;
    m_eventEngine->clearGateTypeDelays();
    emit engineSettingsChanged();
    emit changed();
}

void Circuit::startSimulation() {
    if (m_engineType != "event") return;
    if (m_simulationRunning) return;

    if (m_engineDirty || !m_eventEngine || m_eventEngine->nodeCount() == 0) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    }

    m_simulationRunning = true;
    if (m_simTimer) m_simTimer->start();
    emit simulationChanged();
    emit changed();
}

void Circuit::pauseSimulation() {
    if (!m_simulationRunning) return;
    m_simulationRunning = false;
    if (m_simTimer) m_simTimer->stop();
    emit simulationChanged();
    emit changed();
}

void Circuit::resetSimulation() {
    if (!m_eventEngine) return;
    m_eventEngine->reset();
    applyEngineOutputsToContexts();
    emit simulationChanged();
    emit simulationTick();
    emit changed();
}

void Circuit::simulateStep() {
    if (m_engineType != "event" || !m_eventEngine) return;
    if (m_engineDirty) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    }
    m_eventEngine->step();
    applyEngineOutputsToContexts();
    emit simulationTick();
    emit changed();
}

void Circuit::onSimTimerTick() {
    if (!m_eventEngine || !m_simulationRunning) return;
    quint64 units = m_autoSpeed ? 1000 : (m_simulationSpeed / 60);
    if (units < 1) units = 1;
    m_eventEngine->run(units, kSimRunBudget);
    applyEngineOutputsToContexts();
    emit simulationTick();
    emit changed();
}

void Circuit::rebuildEventEngine() {
    if (!m_eventEngine) return;

    CircuitEvaluator::syncSubPins(m_root, m_subContexts);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        CircuitEvaluator::syncSubPins(it.value(), m_subContexts);

    QHash<QString, QVariantList> subComps;
    QHash<QString, QVariantList> subWires;
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        subComps[it.key()] = it.value().components;
        subWires[it.key()] = it.value().wires;
    }
    m_eventEngine->build(m_root.components, m_root.wires, subComps, subWires);
    m_engineDirty = false;
}

void Circuit::applyEngineOutputsToContexts() {
    if (!m_eventEngine) return;

    for (int i = 0; i < m_root.components.size(); ++i) {
        QVariantMap c = m_root.components[i].toMap();
        QString localId = c.value("id").toString();
        QString type = c.value("type").toString();

        if (type == "sub") {
            QString subId = c.value("subId").toString();
            auto sit = m_subContexts.find(subId);
            if (sit == m_subContexts.end()) continue;

            QVariantList outVals;
            for (const auto& scv : sit.value().components) {
                QVariantMap sc = scv.toMap();
                if (sc.value("type").toString() != "output") continue;
                QString innerQid = localId + "/" + sc.value("id").toString();
                QStringList outs = m_eventEngine->getOutputs(innerQid);
                QString v = outs.isEmpty() ? QString() : outs[0];
                if (v.isEmpty()) {
                    int w = sc.value("bitWidth").toInt();
                    if (w < 1) w = 1;
                    v = QString(w, '0');
                }
                outVals.append(v);
            }
            if (!outVals.isEmpty()) c["outputPorts"] = outVals;

            QVariantList inVals;
            for (const auto& scv : sit.value().components) {
                QVariantMap sc = scv.toMap();
                if (sc.value("type").toString() != "input") continue;
                QString innerQid = localId + "/" + sc.value("id").toString();
                QStringList outs = m_eventEngine->getOutputs(innerQid);
                QString v = outs.isEmpty() ? QString() : outs[0];
                if (v.isEmpty()) {
                    int w = sc.value("bitWidth").toInt();
                    if (w < 1) w = 1;
                    v = QString(w, '0');
                }
                inVals.append(v);
            }
            if (!inVals.isEmpty()) c["inputPortValues"] = inVals;

            m_root.components[i] = c;
            continue;
        }

        QStringList outs = m_eventEngine->getOutputs(localId);
        if (!outs.isEmpty()) {
            QVariantList ops;
            ops.reserve(outs.size());
            for (const auto& s : outs) ops.append(s);
            c["outputPorts"] = ops;
        }
        if (type == "output" || type == "led") {
            if (!outs.isEmpty()) {
                QVariantList ips;
                ips.reserve(outs.size());
                for (const auto& s : outs) ips.append(s);
                c["inputPortValues"] = ips;
            }
        }
        m_root.components[i] = c;
    }
}

QStringList Circuit::engineQidsFor(const QString& ctxId, const QString& localCompId) const {
    QStringList result;
    if (ctxId.isEmpty()) {
        result << localCompId;
        return result;
    }
    for (const auto& cv : m_root.components) {
        QVariantMap c = cv.toMap();
        if (c.value("type").toString() != "sub") continue;
        if (c.value("subId").toString() == ctxId) {
            result << (c.value("id").toString() + "/" + localCompId);
        }
    }
    return result;
}

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
    c["id"] = QString("C%1").arg(++ctx.compCounter);
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