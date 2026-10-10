#include "circuit.h"
#include "ComponentTraits.h"
#include "EventEngine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

void Circuit::refreshErrors() {
    m_errors.clear();
    auto checkCtx = [&](const Context& ctx, const QString& label) {
        for (auto& wv : ctx.wires) {
            QVariantMap w = wv.toMap();
            QString fromId = w.value("fromComp").toString();
            QString toId = w.value("toComp").toString();
            int fromPort = w.value("fromPort").toInt();
            int toPort = w.value("toPort").toInt();
            int fi = -1, ti = -1;
            for (int i = 0; i < ctx.components.size(); ++i) {
                QString cid = ctx.components[i].toMap().value("id").toString();
                if (cid == fromId) fi = i;
                if (cid == toId) ti = i;
            }
            if (fi < 0 || ti < 0) continue;
            int fbw = ComponentTraits::outputBitWidth(ctx.components[fi].toMap(), fromPort);
            int tbw = ComponentTraits::inputBitWidth(ctx.components[ti].toMap(), toPort);
            if (fbw != tbw) {
                QVariantMap err;
                err["type"] = "width_mismatch";
                err["msg"] = QString("%1：%2 位宽不匹配（源 %3 位，目标 %4 位）")
                                 .arg(label, w.value("id").toString()).arg(fbw).arg(tbw);
                m_errors.append(err);
            }
        }
    };
    checkCtx(m_root, "主电路");
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        checkCtx(it.value(), m_subNames.value(it.key(), it.key()));
    emit errorsChanged();
}

void Circuit::clear() {
    if (m_viewOnly) return;
    pushUndo();
    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_subCounter = 0;
    m_errors.clear();
    m_clockStates.clear();
    m_clockTickCount = 0;
    if (m_clockRunning) stopClock();
    if (m_simulationRunning) pauseSimulation();
    if (m_eventEngine) m_eventEngine->clear();
    m_engineDirty = true;
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
}

QVariantMap Circuit::toJson() const {
    QVariantMap result;
    result["version"] = 11;
    result["root"] = QVariantMap{
        {"components", m_root.components},
        {"wires", m_root.wires},
        {"compCounter", m_root.compCounter},
        {"wireCounter", m_root.wireCounter}
    };
    QVariantList subs;
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        QVariantMap s;
        s["id"] = it.key();
        s["name"] = m_subNames.value(it.key(), "");
        s["components"] = it.value().components;
        s["wires"] = it.value().wires;
        s["compCounter"] = it.value().compCounter;
        s["wireCounter"] = it.value().wireCounter;
        subs.append(s);
    }
    result["subcircuits"] = subs;
    result["subCounter"] = m_subCounter;
    result["clockFrequency"] = m_clockFrequency;
    result["clockTickCount"] = m_clockTickCount;
    result["engineType"] = m_engineType;
    result["globalGateDelay"] = QVariant::fromValue<qulonglong>(globalGateDelay());
    result["wireDelay"] = QVariant::fromValue<qulonglong>(wireDelay());
    result["simulationSpeed"] = QVariant::fromValue<qulonglong>(m_simulationSpeed);
    result["simulationSpeedExp"] = m_simulationSpeedExp;
    result["autoSpeed"] = m_autoSpeed;
    QVariantMap gateDelays;
    if (m_eventEngine) {
        auto map = m_eventEngine->gateTypeDelays();
        for (auto it = map.begin(); it != map.end(); ++it) {
            gateDelays[it.key()] = QVariant::fromValue<qulonglong>(it.value());
        }
    }
    result["gateTypeDelays"] = gateDelays;
    return result;
}

bool Circuit::fromJson(const QVariantMap& data) {
    CircuitUndoData* d = und();
    bool wasRestoring = d ? d->restoring : false;
    if (d) d->restoring = true;

    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_clockStates.clear();

    if (data.contains("root")) {
        QVariantMap r = data.value("root").toMap();
        m_root.components = r.value("components").toList();
        m_root.wires = r.value("wires").toList();
        m_root.compCounter = r.value("compCounter").toInt();
        m_root.wireCounter = r.value("wireCounter").toInt();
    }
    QVariantList subs = data.value("subcircuits").toList();
    for (auto& sv : subs) {
        QVariantMap s = sv.toMap();
        QString id = s.value("id").toString();
        Context c;
        c.components = s.value("components").toList();
        c.wires = s.value("wires").toList();
        c.compCounter = s.value("compCounter").toInt();
        c.wireCounter = s.value("wireCounter").toInt();
        m_subContexts[id] = c;
        m_subNames[id] = s.value("name").toString();
    }
    m_subCounter = data.value("subCounter").toInt();
    m_clockFrequency = qBound(1, data.value("clockFrequency").toInt(), 1000);
    if (m_clockFrequency < 1) m_clockFrequency = 2;
    m_clockTickCount = data.value("clockTickCount").toInt();
    if (m_clockTickCount < 0) m_clockTickCount = 0;

    m_engineType = data.value("engineType", "event").toString();
    if (m_engineType != "iter" && m_engineType != "event") m_engineType = "event";
    if (m_eventEngine) {
        qulonglong gd = data.value("globalGateDelay").toULongLong();
        if (gd < 1) gd = 1;
        m_eventEngine->setGlobalGateDelay(gd);
        qulonglong wd = data.value("wireDelay").toULongLong();
        if (wd < 1) wd = 1;
        m_eventEngine->setWireDelay(wd);
        m_eventEngine->clearGateTypeDelays();
        QVariantMap gmap = data.value("gateTypeDelays").toMap();
        for (auto it = gmap.begin(); it != gmap.end(); ++it) {
            qulonglong v = it.value().toULongLong();
            m_eventEngine->setGateTypeDelay(it.key(), v);
        }
    }
    m_simulationSpeed = data.value("simulationSpeed").toULongLong();
    if (m_simulationSpeed < 1) m_simulationSpeed = 1000;
    m_simulationSpeedExp = data.value("simulationSpeedExp").toInt();
    if (m_simulationSpeedExp < 0) m_simulationSpeedExp = 0;
    if (m_simulationSpeedExp > 12) m_simulationSpeedExp = 12;
    m_autoSpeed = data.value("autoSpeed", true).toBool();

    auto fixCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (!c.contains("displayBase")) c["displayBase"] = "bin";
            if (!c.contains("name")) c["name"] = "";
            if (c.value("type").toString() == "text" && !c.contains("content"))
                c["content"] = "文本";
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList bits = c.value("inputBits").toList();
            while (bits.size() < bw) bits.prepend(false);
            while (bits.size() > bw) bits.removeFirst();
            c["inputBits"] = bits;
            QString t = c.value("type").toString();
            if ((t == "splitter" || t == "hub") && !c.contains("outputSplits")) {
                QVariantList splits;
                for (int j = 0; j < bw; ++j) splits.append(1);
                c["outputSplits"] = splits;
            }
            if (t == "clock") m_clockStates[c.value("id").toString()] = false;
            ctx.components[i] = c;
        }
    };
    fixCtx(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) fixCtx(it.value());

    if (m_engineType == "event") {
        rebuildEventEngine();
    } else {
        evaluateAll();
    }

    if (!wasRestoring) {
        if (d) {
            d->undoStack.clear();
            d->redoStack.clear();
            d->lastMergeKey.clear();
            d->lastUndoTime = QDateTime();
        }
    }
    if (d) d->restoring = wasRestoring;

    if (wasRestoring) return true;

    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit engineChanged();
    emit engineSettingsChanged();
    emit changed();
    return true;
}

bool Circuit::saveToFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    addRecentFile(path);
    return true;
}

bool Circuit::loadFromFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;
    bool ok = fromJson(doc.object().toVariantMap());
    if (ok) addRecentFile(path);
    return ok;
}

void Circuit::pushUndo(const QString& mergeKey) {
    CircuitUndoData* d = und();
    if (!d) return;
    if (m_viewOnly) return;
    if (d->restoring) return;
    if (d->redoRunning) return;

    if (!mergeKey.isEmpty() && mergeKey == d->lastMergeKey) {
        if (d->lastUndoTime.isValid() &&
            d->lastUndoTime.msecsTo(QDateTime::currentDateTime()) < 500) {
            d->lastUndoTime = QDateTime::currentDateTime();
            return;
        }
    }

    QVariantMap snap = toJson();
    d->undoStack.push_back(snap);
    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());
    d->redoStack.clear();
    d->lastMergeKey = mergeKey;
    d->lastUndoTime = QDateTime::currentDateTime();
    emit undoRedoChanged();
}

void Circuit::breakUndoMerge() {
    CircuitUndoData* d = und();
    if (!d) return;
    d->lastMergeKey.clear();
    d->lastUndoTime = QDateTime();
}

void Circuit::undo() {
    CircuitUndoData* d = und();
    if (!d) return;
    if (m_viewOnly) return;
    if (d->redoRunning) return;
    if (d->undoStack.empty()) return;

    d->redoRunning = true;
    QVariantMap cur = toJson();
    QVariantMap prev = d->undoStack.back();
    d->undoStack.pop_back();
    d->redoStack.push_back(cur);
    while (d->redoStack.size() > static_cast<size_t>(d->maxUndo))
        d->redoStack.erase(d->redoStack.begin());
    breakUndoMerge();

    restoreSnapshot(prev, true);
    d->redoRunning = false;
    emit undoRedoChanged();
}

void Circuit::redo() {
    CircuitUndoData* d = und();
    if (!d) return;
    if (m_viewOnly) return;
    if (d->redoRunning) return;
    if (d->redoStack.empty()) return;

    d->redoRunning = true;
    QVariantMap cur = toJson();
    QVariantMap next = d->redoStack.back();
    d->redoStack.pop_back();
    d->undoStack.push_back(cur);
    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());
    breakUndoMerge();

    restoreSnapshot(next, true);
    d->redoRunning = false;
    emit undoRedoChanged();
}

void Circuit::restoreSnapshot(const QVariantMap& snap, bool emitSignals) {
    CircuitUndoData* d = und();
    if (!d) return;
    if (d->restoring) return;

    QString savedCtx = m_currentCtxId;
    bool savedView = m_viewOnly;

    d->restoring = true;
    fromJson(snap);
    d->restoring = false;

    if (!savedCtx.isEmpty() && !m_subContexts.contains(savedCtx)) {
        savedCtx = "";
        savedView = false;
    }
    bool ctxChanged = (savedCtx != m_currentCtxId) || (savedView != m_viewOnly);
    m_currentCtxId = savedCtx;
    m_viewOnly = savedView;

    if (!emitSignals) return;

    if (ctxChanged) emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
}