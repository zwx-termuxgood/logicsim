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
#include <QSaveFile>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
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
    saveSettings();

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
        if (m_simulationRunning && m_eventEngine) {
            rebuildEventEngine();
            applyEngineOutputsToContexts();
        } else {
            m_engineDirty = true;
        }
    } else {
        IterEngine::evaluateAll(m_root, m_subContexts);
    }

    bool eventSimOnCurrent = !m_viewOnly &&
                             m_engineType == "event" &&
                             m_simulationRunning;
    if (!m_currentCtxId.isEmpty() && !eventSimOnCurrent) {
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

    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    if (editingSubAsRoot) {
        auto it = m_subContexts.find(m_currentCtxId);
        if (it != m_subContexts.end()) {
            m_eventEngine->build(it.value().components, it.value().wires,
                                 subComps, subWires);
            m_engineDirty = false;
            m_needFullApply = true;
            m_ctxSubInstCache.clear();
            return;
        }
    }

    m_eventEngine->build(m_root.components, m_root.wires, subComps, subWires);
    m_engineDirty = false;
    m_needFullApply = true;
    m_ctxSubInstCache.clear();
}

static void updateOneComponent(QVariantMap& c,
                               EventEngine* eng,
                               const QHash<QString, CircuitContext>& subContexts)
{
    QString localId = c.value("id").toString();
    QString type = c.value("type").toString();

    if (type == "sub") {
        QString subId = c.value("subId").toString();
        auto sit = subContexts.find(subId);
        if (sit == subContexts.end()) return;

        QVariantList outVals;
        for (const auto& scv : sit.value().components) {
            QVariantMap sc = scv.toMap();
            if (sc.value("type").toString() != "output") continue;
            QString innerQid = localId + "/" + sc.value("id").toString();
            QStringList outs = eng->getOutputs(innerQid);
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
            QStringList outs = eng->getOutputs(innerQid);
            QString v = outs.isEmpty() ? QString() : outs[0];
            if (v.isEmpty()) {
                int w = sc.value("bitWidth").toInt();
                if (w < 1) w = 1;
                v = QString(w, '0');
            }
            inVals.append(v);
        }
        if (!inVals.isEmpty()) c["inputPortValues"] = inVals;
        return;
    }

    QStringList outs = eng->getOutputs(localId);
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
}

void Circuit::applyEngineOutputsToContexts() {
    if (!m_eventEngine) return;

    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    Context* targetPtr = editingSubAsRoot ? ctxById(m_currentCtxId) : &m_root;
    if (!targetPtr) targetPtr = &m_root;
    Context& targetCtx = *targetPtr;

    if (m_needFullApply) {
        m_needFullApply = false;
        for (int i = 0; i < targetCtx.components.size(); ++i) {
            QVariantMap c = targetCtx.components[i].toMap();
            updateOneComponent(c, m_eventEngine, m_subContexts);
            targetCtx.components[i] = c;
        }
        m_eventEngine->clearDirtyQids();
        return;
    }

    QVector<QString> dirty = m_eventEngine->takeDirtyQids();
    if (dirty.isEmpty()) return;

    QHash<QString, int> idToIdx;
    idToIdx.reserve(targetCtx.components.size() * 2);
    for (int i = 0; i < targetCtx.components.size(); ++i)
        idToIdx.insert(targetCtx.components[i].toMap().value("id").toString(), i);

    for (const QString& qid : dirty) {
        if (qid.contains('/')) continue;
        int i = idToIdx.value(qid, -1);
        if (i < 0) continue;
        QVariantMap c = targetCtx.components[i].toMap();
        updateOneComponent(c, m_eventEngine, m_subContexts);
        targetCtx.components[i] = c;
    }

    if (m_viewOnly) {
        refreshCurrentSubContext();
    }
}

QStringList Circuit::engineQidsFor(const QString& ctxId, const QString& localCompId) const {
    if (!m_currentCtxId.isEmpty() && !m_viewOnly) {
        return QStringList{ localCompId };
    }

    if (ctxId.isEmpty()) return QStringList{ localCompId };

    auto it = m_ctxSubInstCache.constFind(ctxId);
    QStringList instIds;
    if (it != m_ctxSubInstCache.constEnd()) {
        instIds = it.value();
    } else {
        for (const auto& cv : m_root.components) {
            QVariantMap c = cv.toMap();
            if (c.value("type").toString() != "sub") continue;
            if (c.value("subId").toString() == ctxId)
                instIds.append(c.value("id").toString());
        }
        m_ctxSubInstCache.insert(ctxId, instIds);
    }

    QStringList result;
    result.reserve(instIds.size());
    for (const QString& instId : instIds)
        result.append(instId + "/" + localCompId);
    return result;
}

void Circuit::clear() {
    qWarning() << "[FPTRACE] Circuit::clear() ENTER";
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
    m_clipboardCache.clear();
    m_needFullApply = true;
    if (m_clockRunning) stopClock();
    if (m_simulationRunning) pauseSimulation();
    if (m_eventEngine) m_eventEngine->clear();
    m_engineDirty = true;
    m_ctxSubInstCache.clear();
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
        QHash<QString, int> idToIdx;
        idToIdx.reserve(ctx.components.size() * 2);
        for (int i = 0; i < ctx.components.size(); ++i)
            idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);

        for (auto& wv : ctx.wires) {
            QVariantMap w = wv.toMap();
            QString fromId = w.value("fromComp").toString();
            QString toId = w.value("toComp").toString();
            int fromPort = w.value("fromPort").toInt();
            int toPort = w.value("toPort").toInt();
            int fi = idToIdx.value(fromId, -1);
            int ti = idToIdx.value(toId, -1);
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
    m_clockTickCount = dec.clockTickCount;
    m_needFullApply = true;

    // ★ 全局设置不随电路文件恢复（保留 QSettings 中当前配置）
    //   m_clockFrequency  = dec.clockFrequency;
    //   m_engineType      = dec.engineType;
    //   m_simulationSpeed = dec.simulationSpeed;
    //   m_simulationSpeedExp = dec.simulationSpeedExp;
    //   m_autoSpeed       = dec.autoSpeed;

    if (m_eventEngine) {
        // ★ 全局延迟不随电路恢复
        //   m_eventEngine->setGlobalGateDelay(dec.globalGateDelay);
        //   m_eventEngine->setWireDelay(dec.wireDelay);
        m_eventEngine->clearGateTypeDelays();
        for (auto it = dec.gateTypeDelays.begin(); it != dec.gateTypeDelays.end(); ++it)
            m_eventEngine->setGateTypeDelay(it.key(), it.value());
    }

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
    emit structureChanged();
    emit changed();
    return true;
}

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
            if (c == '"')  { inStr = false; }
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

static bool isOnlyWhitespaceFrom(const QByteArray& data, int pos) {
    for (int i = pos; i < data.size(); ++i) {
        char c = data[i];
        if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)
            return false;
    }
    return true;
}

bool Circuit::saveToFile(const QString& path) {
    qWarning() << "[FPTRACE] saveToFile ENTER path=" << path;
    if (path.isEmpty()) {
        qWarning() << "[FPTRACE] saveToFile EXIT: empty";
        return false;
    }

    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (bytes.isEmpty()) return false;

    QFile f(path);

    if (path.startsWith("content://")) {
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[saveToFile] content:// open failed:" << path
                       << "reason:" << f.errorString();
            return false;
        }
    } else {
        QString local = path;
        if (local.startsWith("file://")) {
            local = QUrl(local).toLocalFile();
        }
        f.setFileName(local);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qWarning() << "[saveToFile] open failed:" << local
                       << "reason:" << f.errorString();
            return false;
        }
    }

    qint64 n = f.write(bytes);
    if (n != bytes.size()) {
        qWarning() << "[saveToFile] short write:" << n << "/" << bytes.size()
        << "reason:" << f.errorString();
        f.close();
        return false;
    }
    if (!f.flush()) {
        qWarning() << "[saveToFile] flush failed:" << f.errorString();
    }
    f.close();
    qWarning() << "[FPTRACE] saveToFile: file written, about to addRecentFile path=" << path;
    addRecentFile(path);
    qWarning() << "[FPTRACE] saveToFile EXIT OK path=" << path;
    return true;
}

bool Circuit::loadFromFile(const QString& path) {
    qWarning() << "[FPTRACE] loadFromFile ENTER path=" << path;
    if (path.isEmpty()) {
        qWarning() << "[FPTRACE] loadFromFile EXIT: empty";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[loadFromFile] open failed:" << path
                   << "reason:" << f.errorString();
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    qWarning() << "[FPTRACE] loadFromFile: read" << data.size() << "bytes";

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    QByteArray cleanData;
    bool needsHeal = false;

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[loadFromFile] parse error:" << err.errorString()
        << "at offset" << err.offset
        << "(file size" << data.size() << ")";

        cleanData = extractFirstJsonObject(data);
        if (cleanData.isEmpty()) {
            qWarning() << "[loadFromFile] extract returned empty, cannot recover";
            return false;
        }
        if (cleanData.size() == data.size()) {
            qWarning() << "[loadFromFile] extracted size == file size,"
                          " not a tail-garbage case";
            return false;
        }

        int endPos = data.indexOf(cleanData) + cleanData.size();
        needsHeal = !isOnlyWhitespaceFrom(data, endPos);

        qWarning() << "[loadFromFile] extracted" << cleanData.size()
                   << "of" << data.size() << "bytes, needsHeal=" << needsHeal;

        QJsonParseError err2;
        QJsonDocument doc2 = QJsonDocument::fromJson(cleanData, &err2);
        if (err2.error != QJsonParseError::NoError) {
            qWarning() << "[loadFromFile] extracted part invalid:"
                       << err2.errorString() << "at offset" << err2.offset;
            return false;
        }
        if (!doc2.isObject()) {
            qWarning() << "[loadFromFile] extracted part is not object";
            return false;
        }
        doc = doc2;
    }

    if (!doc.isObject()) {
        qWarning() << "[FPTRACE] loadFromFile EXIT: not object";
        return false;
    }

    qWarning() << "[FPTRACE] loadFromFile: about to call fromJson";
    bool ok = fromJson(doc.object().toVariantMap());
    qWarning() << "[FPTRACE] loadFromFile: fromJson returned ok=" << ok;
    if (!ok) {
        qWarning() << "[FPTRACE] loadFromFile EXIT: fromJson failed";
        return false;
    }

    if (needsHeal) {
        qWarning() << "[loadFromFile] healing: writing" << cleanData.size()
        << "bytes back to" << path;
        QFile wf(path);
        bool openOk = false;
        if (path.startsWith("content://")) {
            openOk = wf.open(QIODevice::WriteOnly | QIODevice::Text);
        } else {
            openOk = wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        }
        if (openOk) {
            qint64 n = wf.write(cleanData);
            wf.close();
            if (n == cleanData.size())
                qWarning() << "[loadFromFile] heal OK";
            else
                qWarning() << "[loadFromFile] heal short write:" << n
                           << "/" << cleanData.size();
        } else {
            qWarning() << "[loadFromFile] heal open failed:"
                       << wf.errorString();
        }
    }

    qWarning() << "[FPTRACE] loadFromFile: about to addRecentFile path=" << path;
    addRecentFile(path);
    qWarning() << "[FPTRACE] loadFromFile EXIT path=" << path << "ok=" << ok;
    return ok;
}

// ============================================================
// 撤销 / 重做：基于 UndoEntry（增量）
// ============================================================

UndoEntry Circuit::captureUndoEntry(UndoScope scope, const QString& ctxId,
                                    const QString& mergeKey) const
{
    UndoEntry e;
    e.scope = scope;
    e.mergeKey = mergeKey;
    e.time = QDateTime::currentDateTime();

    if (scope == UndoScope::CurrentCtx) {
        e.ctxId = ctxId;
        if (ctxId.isEmpty()) {
            e.singleCtx = m_root;
        } else {
            auto it = m_subContexts.constFind(ctxId);
            if (it != m_subContexts.constEnd())
                e.singleCtx = it.value();
        }
    } else if (scope == UndoScope::Full) {
        e.allContexts.insert(QString(), m_root);
        for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it)
            e.allContexts.insert(it.key(), it.value());
        e.subNames = m_subNames;
        e.subCounter = m_subCounter;
        e.clockStates = m_clockStates;
    } else { // MetaOnly
        e.subNames = m_subNames;
        e.subCounter = m_subCounter;
    }
    return e;
}

UndoEntry Circuit::captureSameScopeAs(const UndoEntry& ref) const {
    return captureUndoEntry(ref.scope, ref.ctxId, QString());
}

void Circuit::applyUndoEntry(const UndoEntry& e) {
    QString savedCtx = m_currentCtxId;
    bool savedView = m_viewOnly;

    if (e.scope == UndoScope::CurrentCtx) {
        if (e.ctxId.isEmpty()) m_root = e.singleCtx;
        else                   m_subContexts[e.ctxId] = e.singleCtx;
    } else if (e.scope == UndoScope::Full) {
        auto it = e.allContexts.constFind(QString());
        if (it != e.allContexts.constEnd()) m_root = it.value();
        m_subContexts.clear();
        for (auto it2 = e.allContexts.constBegin(); it2 != e.allContexts.constEnd(); ++it2) {
            if (!it2.key().isEmpty())
                m_subContexts.insert(it2.key(), it2.value());
        }
        m_subNames = e.subNames;
        m_subCounter = e.subCounter;
        m_clockStates = e.clockStates;
    } else { // MetaOnly
        m_subNames = e.subNames;
        m_subCounter = e.subCounter;
    }

    // 恢复后当前上下文可能已不存在
    if (!savedCtx.isEmpty() && !m_subContexts.contains(savedCtx)) {
        savedCtx = "";
        savedView = false;
    }
    bool ctxChanged = (savedCtx != m_currentCtxId) || (savedView != m_viewOnly);
    m_currentCtxId = savedCtx;
    m_viewOnly = savedView;

    m_engineDirty = true;
    if (m_engineType == "event") {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    } else {
        evaluateAll();
    }

    if (ctxChanged) emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit structureChanged();
    emit changed();
}

void Circuit::pushUndoInternal(UndoScope scope, const QString& ctxId,
                               const QString& mergeKey)
{
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

    UndoEntry e = captureUndoEntry(scope, ctxId, mergeKey);
    d->undoStack.push_back(e);
    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());
    d->redoStack.clear();
    d->lastMergeKey = mergeKey;
    d->lastUndoTime = QDateTime::currentDateTime();
    emit undoRedoChanged();
}

void Circuit::pushUndo(const QString& mergeKey) {
    pushUndoInternal(UndoScope::CurrentCtx, m_currentCtxId, mergeKey);
}

void Circuit::pushUndoAll(const QString& mergeKey) {
    pushUndoInternal(UndoScope::Full, QString(), mergeKey);
}

void Circuit::pushUndoMeta(const QString& mergeKey) {
    pushUndoInternal(UndoScope::MetaOnly, QString(), mergeKey);
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

    UndoEntry toUndo = d->undoStack.back();
    d->undoStack.pop_back();

    UndoEntry cur = captureSameScopeAs(toUndo);
    d->redoStack.push_back(cur);
    while (d->redoStack.size() > static_cast<size_t>(d->maxUndo))
        d->redoStack.erase(d->redoStack.begin());

    breakUndoMerge();

    d->restoring = true;
    applyUndoEntry(toUndo);
    d->restoring = false;

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

    UndoEntry toRedo = d->redoStack.back();
    d->redoStack.pop_back();

    UndoEntry cur = captureSameScopeAs(toRedo);
    d->undoStack.push_back(cur);
    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());

    breakUndoMerge();

    d->restoring = true;
    applyUndoEntry(toRedo);
    d->restoring = false;

    d->redoRunning = false;
    emit undoRedoChanged();
}

// ============================================================
// 最近文件
// ============================================================

void Circuit::loadRecentFiles() {
    m_recentFiles = RecentStore::load();
}

void Circuit::saveRecentFiles() {
    RecentStore::save(m_recentFiles);
}

void Circuit::addRecentFile(const QString& path) {
    qWarning() << "[FPTRACE] addRecentFile ENTER path=" << path
               << "old_size=" << m_recentFiles.size();
    if (path.isEmpty()) {
        qWarning() << "[FPTRACE] addRecentFile EXIT: empty";
        return;
    }
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
    QMetaObject::invokeMethod(this, [this]() {
        emit recentFilesChanged();
    }, Qt::QueuedConnection);
    qWarning() << "[FPTRACE] addRecentFile EXIT path=" << path;
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