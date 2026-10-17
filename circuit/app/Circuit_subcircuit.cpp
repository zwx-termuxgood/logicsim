#include "../Circuit.h"
#include "../logic/iter/IterEngine.h"
#include "../logic/event/EventEngine.h"
#include <QVariantMap>

QString Circuit::addSubcircuit(const QString& name) {
    if (m_viewOnly) return QString();
    pushUndo();
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
    pushUndo();
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
        m_currentCtxId = ""; m_viewOnly = false;
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
    pushUndo();
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

// ============================================================
// 刷新当前子电路视图
// ============================================================
// 语义：
// - 浏览模式（m_viewOnly）：从父实例的输入同步进子电路 input，然后独立求值
// - 编辑模式：独立求值（子电路自己作为独立电路的行为）
void Circuit::refreshCurrentSubContext() {
    if (m_currentCtxId.isEmpty()) return;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return;

    if (m_viewOnly) {
        syncSubStateFromParent(m_currentCtxId);
    }

    IterEngine::evaluateStandalone(it.value(), m_subContexts);
}

// ============================================================
// 从主电路进入子电路（浏览模式）
// ============================================================
// ★ 支持递归浏览：每次进入都把当前状态压栈，leaveSubcircuit 弹栈即可返回上一级
void Circuit::enterSubcircuit(const QString& subId) {
    if (!m_subContexts.contains(subId)) return;

    // 在"当前上下文"里找 subId 的第一个 sub 实例
    const Context* curCtx = ctxById(m_currentCtxId);
    QString instId;
    if (curCtx) {
        for (const auto& cv : curCtx->components) {
            QVariantMap c = cv.toMap();
            if (c.value("type").toString() == "sub" &&
                c.value("subId").toString() == subId) {
                instId = c.value("id").toString();
                break;
            }
        }
    }

    // 完整实例路径（从 root 展开）：父路径 + 本实例 localId
    QString newPath;
    if (!instId.isEmpty()) {
        newPath = m_currentInstancePath.isEmpty()
        ? instId
        : (m_currentInstancePath + "/" + instId);
    }
    // instId 为空（例如从左侧面板"进入浏览"却没有画布实例）→ 不绑定，newPath 留空

    // 保存进入前的状态
    BrowseFrame f;
    f.ctxId = m_currentCtxId;
    f.viewOnly = m_viewOnly;
    f.instancePath = m_currentInstancePath;
    m_browseStack.append(f);

    m_currentCtxId = subId;
    m_viewOnly = true;
    m_currentInstancePath = newPath;
    m_engineDirty = true;

    // 从父实例同步输入 + 独立求值
    if (!newPath.isEmpty()) {
        syncSubStateFromParent(subId);
    }
    auto it = m_subContexts.find(subId);
    if (it != m_subContexts.end())
        IterEngine::evaluateStandalone(it.value(), m_subContexts);

    emit contextChanged();
    emit changed();
}

void Circuit::leaveSubcircuit() {
    // ★ 有浏览栈：返回上一级
    if (!m_browseStack.isEmpty()) {
        BrowseFrame f = m_browseStack.takeLast();
        m_currentCtxId = f.ctxId;
        m_viewOnly = f.viewOnly;
        m_currentInstancePath = f.instancePath;
        m_engineDirty = true;

        if (m_viewOnly && !m_currentCtxId.isEmpty()) {
            if (!m_currentInstancePath.isEmpty()) {
                syncSubStateFromParent(m_currentCtxId);
            }
            auto it = m_subContexts.find(m_currentCtxId);
            if (it != m_subContexts.end())
                IterEngine::evaluateStandalone(it.value(), m_subContexts);
        }

        emit contextChanged();
        emit changed();
        return;
    }

    // 无栈：直接回主电路编辑
    if (m_currentCtxId.isEmpty() && !m_viewOnly) return;
    m_currentCtxId = "";
    m_viewOnly = false;
    m_currentInstancePath.clear();
    m_engineDirty = true;
    emit contextChanged();
    emit changed();
}

// ============================================================
// 从右上角切换电路（编辑模式）
// ============================================================
void Circuit::switchEditContext(const QString& ctxId) {
    if (!ctxId.isEmpty() && !m_subContexts.contains(ctxId)) return;
    m_currentCtxId = ctxId;
    m_viewOnly = false;
    m_currentInstancePath.clear();
    m_browseStack.clear();   // ★ 切换编辑上下文即重置导航
    m_engineDirty = true;

    // 仿真中切换电路：立即按新根重建引擎
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

    pushUndo();

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

// ============================================================
// 从父实例同步输入到当前子电路的 input 元件
// ============================================================
// ★ 现在使用 m_currentInstancePath 作为实例在 root 中的完整路径，
//   支持任意层级的递归浏览（不再只找 root 里第一个匹配实例）。
void Circuit::syncSubStateFromParent(const QString& subId) {
    if (subId.isEmpty()) return;
    auto sit = m_subContexts.find(subId);
    if (sit == m_subContexts.end()) return;

    const QString instPath = m_currentInstancePath;
    if (instPath.isEmpty()) {
        // 没有绑定（例如从左侧面板进入但 root 无该实例）
        return;
    }

    bool fromEngine = (m_engineType == "event" && m_eventEngine && !m_engineDirty);
    QVariantList instIns;

    if (!fromEngine) {
        // 非引擎路径：沿实例路径在 m_root 中展开，找到最终的 sub 元件
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
        if (!inst.isEmpty())
            instIns = inst.value("inputPortValues").toList();
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
            // 事件引擎里子电路已展开，直接用完整路径读父侧驱动值
            QString qid = instPath + "/" + sc.value("id").toString();
            QStringList ins = m_eventEngine->getInputs(qid);
            s = ins.isEmpty() ? QString() : ins[0];
        } else {
            if (idx >= instIns.size()) break;
            s = instIns[idx].toString();
        }

        while (s.length() < bw) s = "0" + s;
        if (s.length() > bw) s = s.right(bw);

        QVariantList bits;
        for (int k = 0; k < bw; ++k) bits.append(s[k] == '1');
        sc["inputBits"] = bits;

        if (s.contains('E') || s.contains('Z')) sc["outputOverride"] = s;
        else                                    sc["outputOverride"] = QVariant();

        sub.components[i] = sc;
        idx++;
    }
}