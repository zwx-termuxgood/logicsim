#include "../Circuit.h"
#include "../core/Component.h"
#include "../core/ValueCodec.h"
#include "../logic/ComponentLogic.h"
#include "../logic/iter/IterEngine.h"
#include "../logic/event/EventEngine.h"
#include "../io/JsonCodec.h"
#include "../io/RecentStore.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

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

void Circuit::evaluateAll() {
    IterEngine::syncSubPins(m_root, m_subContexts);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        IterEngine::syncSubPins(it.value(), m_subContexts);

    if (m_viewOnly && !m_currentCtxId.isEmpty()) {
        syncSubStateFromParent(m_currentCtxId);
    }

    if (m_engineType == "event") {
        m_engineDirty = true;
        emit subcircuitsChanged();
        return;
    }

    IterEngine::evaluateAll(m_root, m_subContexts);

    if (!m_currentCtxId.isEmpty()) {
        auto it = m_subContexts.find(m_currentCtxId);
        if (it != m_subContexts.end())
            IterEngine::evaluateStandalone(it.value(), m_subContexts);
    }

    emit subcircuitsChanged();
}

void Circuit::evaluate() {
    evaluateAll();
}

void Circuit::rebuildEventEngine() {
    if (!m_eventEngine) return;

    IterEngine::syncSubPins(m_root, m_subContexts);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        IterEngine::syncSubPins(it.value(), m_subContexts);

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
            int fbw = ComponentLogic::outputBitWidth(ctx.components[fi].toMap(), fromPort);
            int tbw = ComponentLogic::inputBitWidth(ctx.components[ti].toMap(), toPort);
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

QVariantMap Circuit::toJson() const {
    QHash<QString, quint64> gateDelays;
    if (m_eventEngine) gateDelays = m_eventEngine->gateTypeDelays();
    return JsonCodec::encode(m_root, m_subContexts, m_subNames,
                             m_subCounter, m_clockFrequency, m_clockTickCount,
                             m_engineType, globalGateDelay(), wireDelay(),
                             m_simulationSpeed, m_simulationSpeedExp, m_autoSpeed,
                             gateDelays);
}

bool Circuit::fromJson(const QVariantMap& data) {
    CircuitUndoData* d = und();
    bool wasRestoring = d ? d->restoring : false;
    if (d) d->restoring = true;

    JsonCodec::DecodedData dec = JsonCodec::decode(data);

    m_root = dec.root;
    m_subContexts = dec.subContexts;
    m_subNames = dec.subNames;
    m_currentCtxId = "";
    m_viewOnly = false;
    m_clockStates = dec.clockStates;
    m_subCounter = dec.subCounter;
    m_clockFrequency = dec.clockFrequency;
    m_clockTickCount = dec.clockTickCount;
    m_engineType = dec.engineType;

    if (m_eventEngine) {
        m_eventEngine->setGlobalGateDelay(dec.globalGateDelay);
        m_eventEngine->setWireDelay(dec.wireDelay);
        m_eventEngine->clearGateTypeDelays();
        for (auto it = dec.gateTypeDelays.begin(); it != dec.gateTypeDelays.end(); ++it)
            m_eventEngine->setGateTypeDelay(it.key(), it.value());
    }
    m_simulationSpeed = dec.simulationSpeed;
    m_simulationSpeedExp = dec.simulationSpeedExp;
    m_autoSpeed = dec.autoSpeed;

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

// 从字节流里提取第一个完整的 { ... } 块
// 用于读取被写坏的文件（末尾有旧内容残留）
static QByteArray extractFirstJsonObject(const QByteArray& data) {
    int depth = 0;
    int start = -1;
    bool inStr = false;
    bool esc = false;
    for (int i = 0; i < data.size(); ++i) {
        char c = data[i];
        if (esc) { esc = false; continue; }
        if (inStr) {
            if (c == '\\') { esc = true; continue; }
            if (c == '"')   { inStr = false; }
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start >= 0)
                return data.mid(start, i - start + 1);
        }
    }
    return QByteArray();
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

    if (err.error != QJsonParseError::NoError) {
        // 末尾有垃圾（旧内容残留），提取第一个完整的 { ... }
        QByteArray clean = extractFirstJsonObject(data);
        if (clean.isEmpty() || clean == data) return false;
        QJsonParseError err2;
        QJsonDocument doc2 = QJsonDocument::fromJson(clean, &err2);
        if (err2.error != QJsonParseError::NoError || !doc2.isObject())
            return false;
        doc = doc2;
    }

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

void Circuit::loadRecentFiles() {
    m_recentFiles = RecentStore::load();
}

void Circuit::saveRecentFiles() {
    RecentStore::save(m_recentFiles);
}

void Circuit::addRecentFile(const QString& path) {
    if (path.isEmpty()) return;
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    QVariantMap m;
    m["path"] = path;
    QFileInfo fi(path);
    m["name"] = fi.fileName();
    m["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    m_recentFiles.prepend(m);
    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::removeRecentFile(const QString& path) {
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::clearRecentFiles() {
    m_recentFiles.clear();
    saveRecentFiles();
    emit recentFilesChanged();
}
