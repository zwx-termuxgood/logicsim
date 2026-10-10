#include "circuit.h"
#include "CircuitEvaluator.h"
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

void Circuit::enterSubcircuit(const QString& subId) {
    if (!m_subContexts.contains(subId)) return;
    m_currentCtxId = subId; m_viewOnly = true;

    // 关键：进入前先把父实例当前输入灌入子电路内部 input 元件
    syncSubStateFromParent(subId);

    // 用旧引擎迭代一次，让子电路内部值立刻反映出来
    {
        auto it = m_subContexts.find(subId);
        if (it != m_subContexts.end()) {
            CircuitEvaluator::evaluateStandalone(it.value(), m_subContexts);
        }
    }

    emit contextChanged();
    emit changed();
}

void Circuit::leaveSubcircuit() {
    if (m_currentCtxId.isEmpty() && !m_viewOnly) return;
    m_currentCtxId = ""; m_viewOnly = false;
    emit contextChanged(); emit changed();
}

void Circuit::switchEditContext(const QString& ctxId) {
    if (!ctxId.isEmpty() && !m_subContexts.contains(ctxId)) return;
    m_currentCtxId = ctxId; m_viewOnly = false;
    emit contextChanged(); emit changed();
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

    evaluateAll();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit changed();
}

// ============================================================
// 浏览子电路时：把父实例的输入值灌入子电路内部 input 元件
// ============================================================
void Circuit::syncSubStateFromParent(const QString& subId) {
    if (subId.isEmpty()) return;
    auto sit = m_subContexts.find(subId);
    if (sit == m_subContexts.end()) return;

    // 找 root 里第一个 subId == subId 的实例
    QVariantMap inst;
    bool found = false;
    for (const auto& cv : m_root.components) {
        QVariantMap c = cv.toMap();
        if (c.value("type").toString() == "sub" &&
            c.value("subId").toString() == subId) {
            inst = c;
            found = true;
            break;
        }
    }
    if (!found) return;

    QVariantList instIns = inst.value("inputPortValues").toList();

    Context& sub = sit.value();
    int idx = 0;
    for (int i = 0; i < sub.components.size(); ++i) {
        QVariantMap sc = sub.components[i].toMap();
        if (sc.value("type").toString() != "input") continue;
        if (idx >= instIns.size()) break;

        QString s = instIns[idx].toString();
        int bw = sc.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
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