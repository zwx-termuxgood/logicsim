#include "model/Circuit.h"
// 引入本文件要实现的 Circuit 类完整声明：定义成员函数前必须看到类定义
#include "engine/iter/IterEngine.h"
// 引入迭代引擎：对子电路上下文做独立求值（evaluateStandalone）
#include "engine/event/EventEngine.h"
// 引入事件引擎：syncSubStateFromParent 中查询引擎输入（getInputs）需要完整类型
#include <QVariantMap>
// 引入 QVariantMap：遍历/修改元件 map 时使用

// 定义"新建子电路"接口：分配 S 编号、注册上下文与名称，返回新子电路 id
QString Circuit::addSubcircuit(const QString& name) {
    // 只读浏览模式禁止新建，返回空 id 表示失败
    if (m_viewOnly) return QString();
    // 压元信息撤销快照：仅涉及子电路名与计数器，不涉及元件数据
    pushUndoMeta();
    // 自增计数器并拼接 S 编号作为子电路 id
    QString id = QString("S%1").arg(++m_subCounter);
    // 准备空的子电路上下文
    Context ctx;
    // 把空上下文登记到子电路表
    m_subContexts[id] = ctx;
    // 名称为空则用默认名"子电路+编号"，否则用传入名称
    m_subNames[id] = name.isEmpty() ? ("子电路" + id) : name;
    // 通知 QML 子电路列表变化
    emit subcircuitsChanged();
    // 通知可编辑上下文列表变化
    emit editContextsChanged();
    // 通知数据变化
    emit changed();
    // 返回新子电路 id
    return id;
}

// 定义"删除子电路"接口：移除上下文与名称，并清理所有上下文里引用它的 sub 实例
void Circuit::deleteSubcircuit(const QString& subId) {
    // 只读模式禁止删除
    if (m_viewOnly) return;
    // 子电路不存在则无需处理
    if (!m_subContexts.contains(subId)) return;
    // 压全量撤销快照：跨上下文删除需要整体可撤销
    pushUndoAll();
    // 从子电路表移除上下文
    m_subContexts.remove(subId);
    // 从名称表移除名称
    m_subNames.remove(subId);
    // 定义内部 lambda：从某上下文中倒序移除引用该子电路的 sub 实例
    auto removeFrom = [&](Context& ctx) {
        // 倒序遍历元件，便于原地删除
        for (int i = ctx.components.size() - 1; i >= 0; --i) {
            // 取出元件 map
            auto c = ctx.components[i].toMap();
            // 是 sub 类型且引用的正是被删子电路则移除
            if (c.value("type").toString() == "sub" && c.value("subId").toString() == subId)
                ctx.components.removeAt(i);
        }
    };
    // 先从根上下文清理
    removeFrom(m_root);
    // 再遍历所有子电路上下文清理
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        removeFrom(it.value());
    // 若当前正停在被删子电路上下文里，需要退回根上下文
    if (m_currentCtxId == subId) {
        // 上下文 id 置空回到根
        m_currentCtxId = "";
        // 取消只读浏览标记
        m_viewOnly = false;
        // 清空实例路径
        m_currentInstancePath.clear();
        // 清空浏览栈
        m_browseStack.clear();
        // 通知上下文切换
        emit contextChanged();
    }
    // 删除后全量求值，让引用被清理后的电路收敛
    evaluateAll();
    // 通知子电路列表变化
    emit subcircuitsChanged();
    // 通知可编辑上下文列表变化
    emit editContextsChanged();
    // 通知数据变化
    emit changed();
}

// 定义"修改子电路名称"接口
void Circuit::setSubcircuitName(const QString& subId, const QString& name) {
    // 只读模式禁止修改
    if (m_viewOnly) return;
    // 子电路不存在则返回
    if (!m_subNames.contains(subId)) return;
    // 名称未变则直接返回，避免无意义撤销
    if (m_subNames.value(subId) == name) return;
    // 压元信息撤销快照
    pushUndoMeta();
    // 写入新名称
    m_subNames[subId] = name;
    // 通知子电路列表变化（名称参与展示）
    emit subcircuitsChanged();
    // 通知可编辑上下文列表变化
    emit editContextsChanged();
    // 通知数据变化
    emit changed();
}

// 定义"查询子电路名称"接口
QString Circuit::getSubcircuitName(const QString& subId) const {
    // 从名称表取值，不存在返回空串
    return m_subNames.value(subId, "");
}

// 定义"深度优先检测子电路引用环"辅助函数：从 fromCtx 出发能否到达 targetId
bool Circuit::hasSubCycle(const QString& fromCtx, const QString& targetId,
                          QSet<QString>& visited) const {
    // 起点就是目标，构成自引用环
    if (fromCtx == targetId) return true;
    // 已访问过该上下文，说明此前路径已判过，避免死循环
    if (visited.contains(fromCtx)) return false;
    // 标记当前上下文已访问
    visited.insert(fromCtx);

    // 准备当前上下文指针
    const Context* ctx = nullptr;
    // 空 id 表示根上下文
    if (fromCtx.isEmpty()) ctx = &m_root;
    // 否则从子电路表里查
    else {
        auto it = m_subContexts.find(fromCtx);
        if (it != m_subContexts.end()) ctx = &it.value();
    }
    // 上下文不存在则此路不通
    if (!ctx) return false;

    // 遍历该上下文所有元件
    for (const auto& cv : ctx->components) {
        // 元件转 map
        auto c = cv.toMap();
        // 非 sub 实例不参与引用图
        if (c.value("type").toString() != "sub") continue;
        // 取被引用的子电路 id
        QString sid = c.value("subId").toString();
        // 沿该引用递归，能到达目标即存在环
        if (hasSubCycle(sid, targetId, visited)) return true;
    }
    // 所有引用都走完仍未到达目标，无环
    return false;
}

// 定义"判断能否在当前上下文放置目标子电路实例"接口：防止自包含与循环引用
bool Circuit::canAddSubInstance(const QString& targetSubId) const {
    // 目标就是当前上下文，属自包含，禁止
    if (m_currentCtxId == targetSubId) return false;
    // 已访问集合，供 DFS 使用
    QSet<QString> visited;
    // 从目标子电路出发若能回到当前上下文则形成环，禁止
    return !hasSubCycle(targetSubId, m_currentCtxId, visited);
}

// 定义"刷新当前子电路上下文"接口：浏览模式先从父实例同步输入，再独立求值
void Circuit::refreshCurrentSubContext() {
    // 根上下文无需刷新，直接返回
    if (m_currentCtxId.isEmpty()) return;
    // 查当前子电路上下文
    auto it = m_subContexts.find(m_currentCtxId);
    // 不存在则返回
    if (it == m_subContexts.end()) return;
    // 只读浏览模式下需要先从父实例同步输入状态
    if (m_viewOnly) syncSubStateFromParent(m_currentCtxId);
    // 对子电路上下文做独立迭代求值
    IterEngine::evaluateStandalone(it.value(), m_subContexts);
}

// 定义"进入子电路浏览"接口：压栈当前状态，计算实例路径，从父实例同步输入
void Circuit::enterSubcircuit(const QString& subId) {
    // 子电路不存在则无法进入
    if (!m_subContexts.contains(subId)) return;
    // 准备新的实例路径字符串
    QString newPath;
    // 内部 lambda：在给定上下文里查找引用 wantSubId 的 sub 实例 id
    auto findInstInCtx = [&](const Context* ctx, const QString& wantSubId) -> QString {
        // 上下文为空返回空串
        if (!ctx) return QString();
        // 遍历上下文元件
        for (const auto& cv : ctx->components) {
            // 元件转 map
            QVariantMap c = cv.toMap();
            // 是 sub 类型且引用目标子电路则返回其实例 id
            if (c.value("type").toString() == "sub" &&
                c.value("subId").toString() == wantSubId)
                return c.value("id").toString();
        }
        // 没找到返回空串
        return QString();
    };
    // 已在浏览模式且已有实例路径：从当前上下文继续向下找，路径追加
    if (m_viewOnly && !m_currentInstancePath.isEmpty()) {
        QString instId = findInstInCtx(ctxById(m_currentCtxId), subId);
        if (!instId.isEmpty()) newPath = m_currentInstancePath + "/" + instId;
        // 从根上下文进入：路径就是根里找到的实例 id
    } else if (m_currentCtxId.isEmpty()) {
        newPath = findInstInCtx(&m_root, subId);
        // 从编辑中的子电路进入：先找根里的实例，再找该实例上下文里的实例，拼成两段路径
    } else {
        QString aInstId = findInstInCtx(&m_root, m_currentCtxId);
        if (!aInstId.isEmpty()) {
            QString bInstId = findInstInCtx(ctxById(m_currentCtxId), subId);
            if (!bInstId.isEmpty()) newPath = aInstId + "/" + bInstId;
        }
    }
    // 事件引擎已存在且脏：进入前先重建并回写，保证浏览到的状态一致
    if (m_engineType == "event" && m_eventEngine && m_engineDirty) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
    }
    // 准备浏览栈帧，记录进入前的三项状态
    BrowseFrame f;
    f.ctxId = m_currentCtxId;
    f.viewOnly = m_viewOnly;
    f.instancePath = m_currentInstancePath;
    // 压入浏览栈
    m_browseStack.append(f);
    // 切换到目标子电路上下文
    m_currentCtxId = subId;
    // 浏览子电路一律只读
    m_viewOnly = true;
    // 更新实例路径
    m_currentInstancePath = newPath;
    // 有实例路径则从父实例同步输入状态
    if (!newPath.isEmpty()) syncSubStateFromParent(subId);
    // 引擎状态失效，下次仿真前需重建
    m_engineDirty = true;
    // 对进入的子电路做一次独立求值，界面立即可显示
    auto it = m_subContexts.find(subId);
    if (it != m_subContexts.end())
        IterEngine::evaluateStandalone(it.value(), m_subContexts);
    // 通知上下文切换
    emit contextChanged();
    // 通知数据变化
    emit changed();
}

// 定义"退出子电路浏览"接口：弹栈恢复上一状态
void Circuit::leaveSubcircuit() {
    // 浏览栈非空：弹栈恢复进入前的状态
    if (!m_browseStack.isEmpty()) {
        // 弹出栈顶帧
        BrowseFrame f = m_browseStack.takeLast();
        // 恢复上下文 id
        m_currentCtxId = f.ctxId;
        // 恢复只读标记
        m_viewOnly = f.viewOnly;
        // 恢复实例路径
        m_currentInstancePath = f.instancePath;
        // 引擎状态失效
        m_engineDirty = true;
        // 恢复后仍是浏览模式且非根：需要重新同步并独立求值
        if (m_viewOnly && !m_currentCtxId.isEmpty()) {
            if (!m_currentInstancePath.isEmpty()) syncSubStateFromParent(m_currentCtxId);
            auto it = m_subContexts.find(m_currentCtxId);
            if (it != m_subContexts.end())
                IterEngine::evaluateStandalone(it.value(), m_subContexts);
        }
        // 通知上下文切换
        emit contextChanged();
        // 通知数据变化
        emit changed();
        // 弹栈路径处理完毕直接返回
        return;
    }
    // 栈为空且已在根编辑模式，无需再做任何事
    if (m_currentCtxId.isEmpty() && !m_viewOnly) return;
    // 兜底：直接退回根上下文
    m_currentCtxId = "";
    // 取消只读
    m_viewOnly = false;
    // 清空实例路径
    m_currentInstancePath.clear();
    // 引擎状态失效
    m_engineDirty = true;
    // 通知上下文切换
    emit contextChanged();
    // 通知数据变化
    emit changed();
}

// 定义"直接切换编辑上下文"接口：清空浏览栈，按引擎状态重建或独立求值
void Circuit::switchEditContext(const QString& ctxId) {
    // 目标非根但子电路不存在，直接忽略
    if (!ctxId.isEmpty() && !m_subContexts.contains(ctxId)) return;
    // 设置当前上下文
    m_currentCtxId = ctxId;
    // 编辑模式非只读
    m_viewOnly = false;
    // 清空实例路径
    m_currentInstancePath.clear();
    // 清空浏览栈
    m_browseStack.clear();
    // 引擎状态失效
    m_engineDirty = true;
    // 事件引擎运行中：重建并回写，保证编辑上下文与引擎一致
    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        rebuildEventEngine();
        applyEngineOutputsToContexts();
        // 否则：对目标子电路做一次独立求值
    } else {
        if (!m_currentCtxId.isEmpty()) {
            auto it = m_subContexts.find(m_currentCtxId);
            if (it != m_subContexts.end())
                IterEngine::evaluateStandalone(it.value(), m_subContexts);
        }
    }
    // 通知上下文切换
    emit contextChanged();
    // 通知数据变化
    emit changed();
}

// 定义"把子电路设为主电路"接口：与根上下文互换
void Circuit::setSubcircuitAsRoot(const QString& subId) {
    // 只读模式禁止该操作
    if (m_viewOnly) return;
    // 空 id 无意义
    if (subId.isEmpty()) return;
    // 子电路不存在则返回
    if (!m_subContexts.contains(subId)) return;
    // 压全量撤销快照：根与子电路互换涉及整体数据
    pushUndoAll();
    // 暂存旧根上下文
    Context oldRoot = m_root;
    // 子电路上下文升为根
    m_root = m_subContexts[subId];
    // 旧根降为该编号的子电路
    m_subContexts[subId] = oldRoot;
    // 当前上下文回到根
    m_currentCtxId = "";
    // 取消只读
    m_viewOnly = false;
    // 清空实例路径
    m_currentInstancePath.clear();
    // 清空浏览栈
    m_browseStack.clear();
    // 上下文结构已变，子实例缓存失效
    m_ctxSubInstCache.clear();
    // 全量求值让互换后的电路收敛
    evaluateAll();
    // 通知上下文切换
    emit contextChanged();
    // 通知子电路列表变化
    emit subcircuitsChanged();
    // 通知可编辑上下文列表变化
    emit editContextsChanged();
    // 通知数据变化
    emit changed();
}

// 定义"从父实例同步输入状态到子电路上下文"接口：浏览时保持状态一致
void Circuit::syncSubStateFromParent(const QString& subId) {
    // 空 id 直接返回
    if (subId.isEmpty()) return;
    // 查子电路上下文
    auto sit = m_subContexts.find(subId);
    // 不存在则返回
    if (sit == m_subContexts.end()) return;
    // 取当前实例路径
    const QString instPath = m_currentInstancePath;
    // 无实例路径说明没有父实例可同步
    if (instPath.isEmpty()) return;
    // 事件引擎可用且未脏时，输入值可直接从引擎查询
    bool fromEngine = (m_engineType == "event" && m_eventEngine && !m_engineDirty);
    // 准备父实例的输入端口值列表（非引擎路径用）
    QVariantList instIns;
    // 非引擎路径：沿实例路径逐层找到最末实例，取其 inputPortValues
    if (!fromEngine) {
        // 从根上下文开始逐层下钻
        const Context* curCtx = &m_root;
        // 最终找到的实例 map
        QVariantMap inst;
        // 路径按 '/' 切分成各层实例 id
        QStringList parts = instPath.split('/', Qt::SkipEmptyParts);
        // 逐层遍历
        for (int p = 0; p < parts.size(); ++p) {
            // 当前层找到的元件
            QVariantMap found;
            // 在当前上下文里按 id 找元件
            for (const auto& cv : curCtx->components) {
                QVariantMap c = cv.toMap();
                if (c.value("id").toString() == parts[p]) { found = c; break; }
            }
            // 找不到或不是 sub 类型，路径失效，清空结果
            if (found.isEmpty()) { inst.clear(); break; }
            if (found.value("type").toString() != "sub") { inst.clear(); break; }
            // 最后一层就是目标实例
            if (p == parts.size() - 1) {
                inst = found;
                // 中间层：下钻到该实例引用的子电路上下文继续找
            } else {
                QString sid = found.value("subId").toString();
                auto it2 = m_subContexts.find(sid);
                if (it2 == m_subContexts.end()) { inst.clear(); break; }
                curCtx = &it2.value();
            }
        }
        // 找到有效实例则取其输入端口值列表
        if (!inst.isEmpty()) instIns = inst.value("inputPortValues").toList();
    }
    // 取子电路上下文引用
    Context& sub = sit.value();
    // 子电路内部 input 元件的序号，用于按顺序对应父实例输入端口
    int idx = 0;
    // 遍历子电路所有元件
    for (int i = 0; i < sub.components.size(); ++i) {
        // 元件转 map
        QVariantMap sc = sub.components[i].toMap();
        // 只处理 input 元件
        if (sc.value("type").toString() != "input") continue;
        // 取该 input 元件位宽，下限钳到 1
        int bw = sc.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
        // 准备同步的二进制串
        QString s;
        // 引擎路径：用实例路径 + 本地 id 拼 qid，从引擎查该输入端口值
        if (fromEngine) {
            QString qid = instPath + "/" + sc.value("id").toString();
            QStringList ins = m_eventEngine->getInputs(qid);
            s = ins.isEmpty() ? QString() : ins[0];
            // 非引擎路径：按顺序取父实例对应输入端口值
        } else {
            if (idx < instIns.size()) s = instIns[idx].toString();
        }
        // 左侧补 0 到目标位宽
        while (s.length() < bw) s = "0" + s;
        // 超出位宽从左侧截断，只保留低位
        if (s.length() > bw) s = s.right(bw);
        // 准备位列表
        QVariantList bits;
        // 逐位转成 bool
        for (int k = 0; k < bw; ++k) bits.append(s[k] == '1');
        // 写入 inputBits
        sc["inputBits"] = bits;
        // 含未知/高阻态时记录 outputOverride，否则清空
        if (s.contains('E') || s.contains('Z')) sc["outputOverride"] = s;
        else sc["outputOverride"] = QVariant();
        // 写回子电路元件
        sub.components[i] = sc;
        // 序号推进，对应下一个父实例输入端口
        idx++;
    }
}