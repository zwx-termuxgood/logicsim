#include "../Circuit.h"
#include "../logic/iter/IterEngine.h"
#include "../logic/event/EventEngine.h"
#include <QVariantMap>

QString Circuit::addSubcircuit(const QString& name) {
    if (m_viewOnly) return QString();
    pushUndoMeta();
    QString id = QString("S%1").arg(++m_subCounter);
    Context ctx;
    m_subContexts[id] = ctx;
    m_subNames[id] = name.isEmpty() ? ("子电路" + id) : name;
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
    return id;
}

void Circuit::deleteSubcircuit(const QString& subId) {
    if (m_viewOnly) return;
    if (!m_subContexts.contains(subId)) return;
    pushUndoAll();
    m_subContexts.remove(subId);
    m_subNames.remove(subId);
    auto removeFrom = [&](Context& ctx) {
        for (int i = ctx.components.size() - 1; i >= 0; --i) {
            auto c = ctx.components[i].toMap();
            if (c.value("type").toString() == "sub" && c.value("subId").toString() == subId)
                ctx.components.removeAt(i);
        }
    };
    removeFrom(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        removeFrom(it.value());
    if (m_currentCtxId == subId) {
        m_currentCtxId = "";
        m_viewOnly = false;
        m_currentInstancePath.clear();
        m_browseStack.clear();
        emit contextChanged();
    }
    evaluateAll();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
}

void Circuit::setSubcircuitName(const QString& subId, const QString& name) {
    if (m_viewOnly) return;
    if (!m_subNames.contains(subId)) return;
    if (m_subNames.value(subId) == name) return;
    pushUndoMeta();
    m_subNames[subId] = name;
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
}

QString Circuit::getSubcircuitName(const QString& subId) const {
    return m_subNames.value(subId, "");
}

bool Circuit::hasSubCycle(const QString& fromCtx, const QString& targetId,
                          QSet<QString>& visited) const {
    if (fromCtx == targetId) return true;
    if (visited.contains(fromCtx)) return false;
    visited.insert(fromCtx);

    const Context* ctx = nullptr;
    if (fromCtx.isEmpty()) ctx = &m_root;
    else {
        auto it = m_subContexts.find(fromCtx);
        if (it != m_subContexts.end()) ctx = &it.value();
    }
    if (!ctx) return false;

    for (const auto& cv : ctx->components) {
        auto c = cv.toMap();
        if (c.value("type").toString() != "sub") continue;
        QString sid = c.value("subId").toString();
        if (hasSubCycle(sid, targetId, visited)) return true;
    }
    return false;
}

bool Circuit::canAddSubInstance(const QString& targetSubId) const {
    if (m_currentCtxId == targetSubId) return false;
    QSet<QString> visited;
    return !hasSubCycle(targetSubId, m_currentCtxId, visited);
}

void Circuit::refreshCurrentSubContext() {
    if (m_currentCtxId.isEmpty()) return;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return;
    if (m_viewOnly) syncSubStateFromParent(m_currentCtxId);
    IterEngine::evaluateStandalone(it.value(), m_subContexts);
}

void Circuit::enterSubcircuit(const QString& subId) {
    if (!m_subContexts.contains(subId)) return;
    QString newPath;
    auto findInstInCtx = [&](const Context* ctx, const QString& wantSubId) -> QString {
        if (!ctx) return QString();
        for (const auto& cv : ctx->components) {
            QVariantMap c = cv.toMap();
            if (c.value("type").toString() == "sub" &&
                c.value("subId").toString() == wantSubId)
                return c.value("id").toString();
        }
        return QString();
    };
    if (m_viewOnly && !m_currentInstancePath.isEmpty()) {
        QString instId = findInstInCtx(ctxById(m_currentCtxId), subId);
        if (!instId.isEmpty()) newPath = m_currentInstancePath + "/" + instId;
    } else if (m_currentCtxId.isEmpty()) {
        newPath = findInstInCtx(&m_root, subId);
    } else {
        QString aInstId = findInstInCtx(&m_root, m_currentCtxId);
        if (!aInstId.isEmpty()) {
            QString bInstId = findInstInCtx(ctxById(m_currentCtxId), subId);
            if (!bInstId.isEmpty()) newPath = aInstId + "/" + bInstId;
        }
    }
    if (m_engineType == "event" && m_eventEngine && m_engineDirty) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    }
    BrowseFrame f;
    f.ctxId = m_currentCtxId;
    f.viewOnly = m_viewOnly;
    f.instancePath = m_currentInstancePath;
    m_browseStack.append(f);
    m_currentCtxId = subId;
    m_viewOnly = true;
    m_currentInstancePath = newPath;
    if (!newPath.isEmpty()) syncSubStateFromParent(subId);
    m_engineDirty = true;
    auto it = m_subContexts.find(subId);
    if (it != m_subContexts.end())
        IterEngine::evaluateStandalone(it.value(), m_subContexts);
    emit contextChanged();
    emit changed();
}

void Circuit::leaveSubcircuit() {
    if (!m_browseStack.isEmpty()) {
        BrowseFrame f = m_browseStack.takeLast();
        m_currentCtxId = f.ctxId;
        m_viewOnly = f.viewOnly;
        m_currentInstancePath = f.instancePath;
        m_engineDirty = true;
        if (m_viewOnly && !m_currentCtxId.isEmpty()) {
            if (!m_currentInstancePath.isEmpty()) syncSubStateFromParent(m_currentCtxId);
            auto it = m_subContexts.find(m_currentCtxId);
            if (it != m_subContexts.end())
                IterEngine::evaluateStandalone(it.value(), m_subContexts);
        }
        emit contextChanged();
        emit changed();
        return;
    }
    if (m_currentCtxId.isEmpty() && !m_viewOnly) return;
    m_currentCtxId = "";
    m_viewOnly = false;
    m_currentInstancePath.clear();
    m_engineDirty = true;
    emit contextChanged();
    emit changed();
}

void Circuit::switchEditContext(const QString& ctxId) {
    if (!ctxId.isEmpty() && !m_subContexts.contains(ctxId)) return;
    m_currentCtxId = ctxId;
    m_viewOnly = false;
    m_currentInstancePath.clear();
    m_browseStack.clear();
    m_engineDirty = true;
    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    } else {
        if (!m_currentCtxId.isEmpty()) {
            auto it = m_subContexts.find(m_currentCtxId);
            if (it != m_subContexts.end())
                IterEngine::evaluateStandalone(it.value(), m_subContexts);
        }
    }
    emit contextChanged();
    emit changed();
}

void Circuit::setSubcircuitAsRoot(const QString& subId) {
    if (m_viewOnly) return;
    if (subId.isEmpty()) return;
    if (!m_subContexts.contains(subId)) return;
    pushUndoAll();
    Context oldRoot = m_root;
    m_root = m_subContexts[subId];
    m_subContexts[subId] = oldRoot;
    m_currentCtxId = "";
    m_viewOnly = false;
    m_currentInstancePath.clear();
    m_browseStack.clear();
    m_ctxSubInstCache.clear();
    evaluateAll();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
}

void Circuit::syncSubStateFromParent(const QString& subId) {
    if (subId.isEmpty()) return;
    auto sit = m_subContexts.find(subId);
    if (sit == m_subContexts.end()) return;
    const QString instPath = m_currentInstancePath;
    if (instPath.isEmpty()) return;
    bool fromEngine = (m_engineType == "event" && m_eventEngine && !m_engineDirty);
    QVariantList instIns;
    if (!fromEngine) {
        const Context* curCtx = &m_root;
        QVariantMap inst;
        QStringList parts = instPath.split('/', Qt::SkipEmptyParts);
        for (int p = 0; p < parts.size(); ++p) {
            QVariantMap found;
            for (const auto& cv : curCtx->components) {
                QVariantMap c = cv.toMap();
                if (c.value("id").toString() == parts[p]) { found = c; break; }
            }
            if (found.isEmpty()) { inst.clear(); break; }
            if (found.value("type").toString() != "sub") { inst.clear(); break; }
            if (p == parts.size() - 1) {
                inst = found;
            } else {
                QString sid = found.value("subId").toString();
                auto it2 = m_subContexts.find(sid);
                if (it2 == m_subContexts.end()) { inst.clear(); break; }
                curCtx = &it2.value();
            }
        }
        if (!inst.isEmpty()) instIns = inst.value("inputPortValues").toList();
    }
    Context& sub = sit.value();
    int idx = 0;
    for (int i = 0; i < sub.components.size(); ++i) {
        QVariantMap sc = sub.components[i].toMap();
        if (sc.value("type").toString() != "input") continue;
        int bw = sc.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
        QString s;
        if (fromEngine) {
            QString qid = instPath + "/" + sc.value("id").toString();
            QStringList ins = m_eventEngine->getInputs(qid);
            s = ins.isEmpty() ? QString() : ins[0];
        } else {
            if (idx < instIns.size()) s = instIns[idx].toString();
        }
        while (s.length() < bw) s = "0" + s;
        if (s.length() > bw) s = s.right(bw);
        QVariantList bits;
        for (int k = 0; k < bw; ++k) bits.append(s[k] == '1');
        sc["inputBits"] = bits;
        if (s.contains('E') || s.contains('Z')) sc["outputOverride"] = s;
        else sc["outputOverride"] = QVariant();
        sub.components[i] = sc;
        idx++;
    }
}