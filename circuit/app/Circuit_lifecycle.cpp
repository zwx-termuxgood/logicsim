#include "../Circuit.h"
#include "../core/Component.h"
#include "../core/ValueCodec.h"
#include "../logic/ComponentLogic.h"
#include "../logic/WireNetworkSolver.h"
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
        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
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

    if (m_viewOnly && !m_currentCtxId.isEmpty())
        syncSubStateFromParent(m_currentCtxId);

    if (m_engineType == "event") {
        if (m_simulationRunning && m_eventEngine) {
            rebuildEventEngine();
            {
                quint64 wd = m_eventEngine->wireDelay();
                quint64 gd = m_eventEngine->globalGateDelay();
                runEventEngineFully((wd + gd) * 64 + 200, 500000);
            }
            applyEngineOutputsToContexts();
        } else {
            m_engineDirty = true;
            IterEngine::evaluateAll(m_root, m_subContexts);
        }
    } else {
        IterEngine::evaluateAll(m_root, m_subContexts);
    }

    bool eventSimOnCurrent = !m_viewOnly && m_engineType == "event" && m_simulationRunning;
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

    QHash<QString, QVariantList> subComps, subWires;
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        subComps[it.key()] = it.value().components;
        subWires[it.key()] = it.value().wires;
    }

    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    if (editingSubAsRoot) {
        auto it = m_subContexts.find(m_currentCtxId);
        if (it != m_subContexts.end()) {
            QVariantList allWires = it.value().wires;
            QVariantList vw = WireNetworkSolver::generateVirtualWires(
                it.value().components, it.value().wires);
            for (const auto& w : vw) allWires.append(w);
            m_eventEngine->build(it.value().components, allWires, subComps, subWires);
            m_engineDirty = false;
            m_needFullApply = true;
            m_ctxSubInstCache.clear();
            return;
        }
    }

    QVariantList allWires = m_root.wires;
    QVariantList vw = WireNetworkSolver::generateVirtualWires(
        m_root.components, m_root.wires);
    for (const auto& w : vw) allWires.append(w);
    m_eventEngine->build(m_root.components, allWires, subComps, subWires);
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

        if (type == "input" || type == "const") {
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QString s = outs[0];
            if (s.length() != bw) {
                if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
                else                 s = s.right(bw);
            }
            QVariantList bits;
            bits.reserve(bw);
            for (int i = 0; i < bw; ++i) bits.append(s[i] == '1');
            c["inputBits"] = bits;
        }
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
    if (m_engineDirty) {
        rebuildEventEngine();
    }

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

    QSet<QString> instanceIdsToUpdate;
    for (const QString& qid : dirty) {
        int slash = qid.indexOf('/');
        QString topId = (slash < 0) ? qid : qid.left(slash);
        if (idToIdx.contains(topId))
            instanceIdsToUpdate.insert(topId);
    }

    for (const QString& id : instanceIdsToUpdate) {
        int i = idToIdx.value(id, -1);
        if (i < 0) continue;
        QVariantMap c = targetCtx.components[i].toMap();
        updateOneComponent(c, m_eventEngine, m_subContexts);
        targetCtx.components[i] = c;
    }

    if (m_viewOnly) refreshCurrentSubContext();
}

QStringList Circuit::engineQidsFor(const QString& ctxId, const QString& localCompId) const {
    if (!m_currentCtxId.isEmpty() && !m_viewOnly)
        return QStringList{ localCompId };
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

// ★ 修复：起始 advance 至少 10000（原值 ~200 会导致大电路需要一路翻倍，
//   每次翻倍都受 budget=50万 上限约束，最终跑不完）
void Circuit::runEventEngineFully(quint64 advance, quint64 budget) {
    if (!m_eventEngine) return;
    if (budget < 1) budget = 200000;
    quint64 a = qMax<quint64>(advance, 10000ULL);
    for (int round = 0; round < 200; ++round) {
        if (m_eventEngine->isIdle()) break;
        bool ok = m_eventEngine->run(a, budget);
        if (m_eventEngine->isIdle()) break;
        if (ok) {
            if (a < 100000000ULL) a *= 2;
            else a = 100000000ULL;
        }
    }
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
    m_importCounter = 0;
    m_errors.clear();
    m_clockStates.clear();
    m_clockTickCount = 0;
    m_clipboardCache.clear();
    m_needFullApply = true;
    m_importedFiles.clear();
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
            QString wt = w.value("wireType").toString();
            if (wt == "ortho" || wt == "virtual") continue;
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
    m_importedFiles.clear();

    if (m_eventEngine) {
        m_eventEngine->clearGateTypeDelays();
        for (auto it = dec.gateTypeDelays.begin(); it != dec.gateTypeDelays.end(); ++it)
            m_eventEngine->setGateTypeDelay(it.key(), it.value());
    }

    if (m_engineType == "event") rebuildEventEngine();
    else evaluateAll();

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
            if (c == '"') { inStr = false; }
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
        if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) return false;
    }
    return true;
}

bool Circuit::saveToFile(const QString& path) {
    if (path.isEmpty()) return false;
    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (bytes.isEmpty()) return false;
    QFile f(path);
    if (path.startsWith("content://")) {
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    } else {
        QString local = path;
        if (local.startsWith("file://")) local = QUrl(local).toLocalFile();
        f.setFileName(local);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    }
    qint64 n = f.write(bytes);
    if (n != bytes.size()) { f.close(); return false; }
    f.close();
    addRecentFile(path);
    return true;
}

bool Circuit::loadFromFile(const QString& path) {
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    QByteArray cleanData;
    bool needsHeal = false;
    if (err.error != QJsonParseError::NoError) {
        cleanData = extractFirstJsonObject(data);
        if (cleanData.isEmpty()) return false;
        if (cleanData.size() == data.size()) return false;
        int endPos = data.indexOf(cleanData) + cleanData.size();
        needsHeal = !isOnlyWhitespaceFrom(data, endPos);
        QJsonParseError err2;
        QJsonDocument doc2 = QJsonDocument::fromJson(cleanData, &err2);
        if (err2.error != QJsonParseError::NoError) return false;
        if (!doc2.isObject()) return false;
        doc = doc2;
    }
    if (!doc.isObject()) return false;
    bool ok = fromJson(doc.object().toVariantMap());
    if (!ok) return false;
    if (needsHeal) {
        QFile wf(path);
        bool openOk = path.startsWith("content://")
                          ? wf.open(QIODevice::WriteOnly | QIODevice::Text)
                          : wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        if (openOk) { wf.write(cleanData); wf.close(); }
    }
    addRecentFile(path);
    return ok;
}

void Circuit::removeSubContextsByPrefix(const QString& prefix) {
    if (prefix.isEmpty()) return;
    QSet<QString> subIdSet;
    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) subIdSet.insert(it.key());
    }
    if (subIdSet.isEmpty()) return;
    for (const auto& sid : subIdSet) {
        m_subContexts.remove(sid);
        m_subNames.remove(sid);
    }
    auto removeInstances = [&](Context& ctx) {
        for (int i = ctx.components.size() - 1; i >= 0; --i) {
            auto c = ctx.components[i].toMap();
            if (c.value("type").toString() != "sub") continue;
            if (!subIdSet.contains(c.value("subId").toString())) continue;
            QString id = c.value("id").toString();
            for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                auto w = ctx.wires[j].toMap();
                if (w.value("fromComp").toString() == id ||
                    w.value("toComp").toString() == id)
                    ctx.wires.removeAt(j);
            }
            ctx.components.removeAt(i);
        }
    };
    removeInstances(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        removeInstances(it.value());
    if (subIdSet.contains(m_currentCtxId)) {
        m_currentCtxId = "";
        m_viewOnly = false;
        m_currentInstancePath.clear();
        m_browseStack.clear();
        emit contextChanged();
    }
    m_ctxSubInstCache.clear();
}

bool Circuit::importFromFile(const QString& path) {
    if (path.isEmpty()) return false;
    if (m_viewOnly) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    if (data.isEmpty()) return false;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        QByteArray clean = extractFirstJsonObject(data);
        if (clean.isEmpty()) return false;
        QJsonParseError err2;
        QJsonDocument doc2 = QJsonDocument::fromJson(clean, &err2);
        if (err2.error != QJsonParseError::NoError || !doc2.isObject()) return false;
        doc = doc2;
    }
    if (!doc.isObject()) return false;

    JsonCodec::DecodedData dec = JsonCodec::decode(doc.object().toVariantMap());
    QString baseName = QFileInfo(path).completeBaseName();
    if (baseName.isEmpty()) baseName = "外部电路";

    if (m_importedFiles.contains(path)) {
        QString oldPrefix = m_importedFiles.value(path).value("prefix").toString();
        removeSubContextsByPrefix(oldPrefix);
        m_importedFiles.remove(path);
    } else {
        QString suffix = QString("（%1）").arg(baseName);
        QStringList stale;
        for (auto it = m_subNames.constBegin(); it != m_subNames.constEnd(); ++it) {
            if (it.value().endsWith(suffix)) stale.append(it.key());
        }
        if (!stale.isEmpty()) {
            pushUndoAll();
            QSet<QString> staleSet;
            for (const auto& sid : stale) staleSet.insert(sid);
            for (const auto& sid : stale) {
                m_subContexts.remove(sid);
                m_subNames.remove(sid);
            }
            auto removeInstances = [&](Context& ctx) {
                for (int i = ctx.components.size() - 1; i >= 0; --i) {
                    auto c = ctx.components[i].toMap();
                    if (c.value("type").toString() != "sub") continue;
                    if (!staleSet.contains(c.value("subId").toString())) continue;
                    QString id = c.value("id").toString();
                    for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                        auto w = ctx.wires[j].toMap();
                        if (w.value("fromComp").toString() == id ||
                            w.value("toComp").toString() == id)
                            ctx.wires.removeAt(j);
                    }
                    ctx.components.removeAt(i);
                }
            };
            removeInstances(m_root);
            for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
                removeInstances(it.value());
            m_ctxSubInstCache.clear();
        } else {
            pushUndoAll();
        }
    }

    QString prefix;
    do {
        prefix = QString("IMP%1_").arg(++m_importCounter);
    } while (m_subContexts.contains(prefix + "ROOT"));

    QHash<QString, QString> subIdMap;
    subIdMap.insert(QString(), prefix + "ROOT");
    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it)
        subIdMap.insert(it.key(), prefix + it.key());

    auto processCtx = [&](CircuitContext ctx) -> CircuitContext {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            QString oldId = c.value("id").toString();
            c["id"] = prefix + oldId;
            if (c.value("type").toString() == "sub") {
                QString oldSubId = c.value("subId").toString();
                if (subIdMap.contains(oldSubId))
                    c["subId"] = subIdMap.value(oldSubId);
            }
            ctx.components[i] = c;
        }
        for (int i = 0; i < ctx.wires.size(); ++i) {
            QVariantMap w = ctx.wires[i].toMap();
            w["id"] = prefix + w.value("id").toString();
            if (w.contains("fromComp"))
                w["fromComp"] = prefix + w.value("fromComp").toString();
            if (w.contains("toComp"))
                w["toComp"] = prefix + w.value("toComp").toString();
            ctx.wires[i] = w;
        }
        ctx.compCounter = ctx.components.size();
        ctx.wireCounter = ctx.wires.size();
        return ctx;
    };

    QStringList newSubIds;
    {
        CircuitContext ctx = processCtx(dec.root);
        QString newId = subIdMap.value(QString());
        m_subContexts[newId] = ctx;
        m_subNames[newId] = QString("主电路（%1）").arg(baseName);
        newSubIds.append(newId);
    }
    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it) {
        CircuitContext ctx = processCtx(it.value());
        QString newId = subIdMap.value(it.key());
        m_subContexts[newId] = ctx;
        QString oldName = dec.subNames.value(it.key(), it.key());
        m_subNames[newId] = QString("%1（%2）").arg(oldName, baseName);
        newSubIds.append(newId);
    }

    QVariantMap info;
    info["path"] = path;
    info["prefix"] = prefix;
    info["baseName"] = baseName;
    info["subIds"] = newSubIds;
    m_importedFiles[path] = info;

    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        for (const auto& cv : it.value().components) {
            QVariantMap c = cv.toMap();
            if (c.value("type").toString() == "clock") {
                if (!m_clockStates.contains(c.value("id").toString()))
                    m_clockStates[c.value("id").toString()] = false;
            }
        }
    }

    m_ctxSubInstCache.clear();
    evaluateAll();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
    emit structureChanged();
    return true;
}

bool Circuit::removeImportedFile(const QString& path) {
    if (m_viewOnly) return false;
    if (!m_importedFiles.contains(path)) return false;
    pushUndoAll();
    QString prefix = m_importedFiles.value(path).value("prefix").toString();
    removeSubContextsByPrefix(prefix);
    m_importedFiles.remove(path);
    evaluateAll();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
    emit structureChanged();
    return true;
}

QVariantList Circuit::importedFiles() const {
    QVariantList out;
    for (auto it = m_importedFiles.constBegin(); it != m_importedFiles.constEnd(); ++it) {
        QVariantMap m;
        m["path"] = it.value().value("path");
        m["prefix"] = it.value().value("prefix");
        m["baseName"] = it.value().value("baseName");
        m["subCount"] = it.value().value("subIds").toList().size();
        out.append(m);
    }
    return out;
}

UndoEntry Circuit::captureUndoEntry(UndoScope scope, const QString& ctxId,
                                    const QString& mergeKey) const
{
    UndoEntry e;
    e.scope = scope;
    e.mergeKey = mergeKey;
    e.time = QDateTime::currentDateTime();

    if (scope == UndoScope::CurrentCtx) {
        e.ctxId = ctxId;
        if (ctxId.isEmpty()) e.singleCtx = m_root;
        else {
            auto it = m_subContexts.constFind(ctxId);
            if (it != m_subContexts.constEnd()) e.singleCtx = it.value();
        }
    } else if (scope == UndoScope::Full) {
        e.allContexts.insert(QString(), m_root);
        for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it)
            e.allContexts.insert(it.key(), it.value());
        e.subNames = m_subNames;
        e.subCounter = m_subCounter;
        e.clockStates = m_clockStates;
        e.importedFiles = m_importedFiles;
    } else {
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
        else m_subContexts[e.ctxId] = e.singleCtx;
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
        m_importedFiles = e.importedFiles;
    } else {
        m_subNames = e.subNames;
        m_subCounter = e.subCounter;
    }

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
        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
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

void Circuit::loadRecentFiles() { m_recentFiles = RecentStore::load(); }
void Circuit::saveRecentFiles() { RecentStore::save(m_recentFiles); }

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
    QMetaObject::invokeMethod(this, [this]() {
        emit recentFilesChanged();
    }, Qt::QueuedConnection);
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