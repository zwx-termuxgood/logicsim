#include "EventEngine.h"
// 引入本文件要实现的 EventEngine 类完整声明：定义其成员函数前必须看到类定义
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：组合逻辑输出计算委托它，保证与迭代引擎行为一致
#include <algorithm>
// 引入算法库：本文件按需使用（最值/遍历工具），当前为保持与其它实现文件一致依赖

// 定义"多驱合并"静态函数：把某输入端口的多个驱动源值按位合并成单值
QString EventEngine::resolveMultiDriver(const QVector<QString>& vals, int bw) {
    // 位宽下限钳到 1
    if (bw < 1) bw = 1;
    // 无驱动源：视为全 0
    if (vals.isEmpty()) return QString(bw, '0');

    // 只有一个驱动源：直接补齐/截断到目标位宽后返回
    if (vals.size() == 1) {
        // 取唯一驱动值
        QString s = vals[0];
        // 长度不足：左侧补 0
        if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
        // 长度超出：只保留低位
        else if (s.length() > bw) s = s.right(bw);
        // 返回
        return s;
    }

    // 多驱：逐位合并
    QString result(bw, '0');
    // 遍历每一位
    for (int pos = 0; pos < bw; ++pos) {
        // 该位是否出现过 0、1、E
        bool has0 = false, has1 = false, hasE = false;
        // 遍历每个驱动值
        for (const auto& v : vals) {
            // 补齐/截断到目标位宽，便于按位比较
            QString s = v;
            if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
            else if (s.length() > bw) s = s.right(bw);
            // 取该位字符，缺失按 '0' 处理
            QChar c = (pos < s.length()) ? s[pos] : QChar('0');
            // 记录出现过的电平
            if (c == '0') has0 = true;
            else if (c == '1') has1 = true;
            else if (c == 'E') hasE = true;
        }
        // 合并优先级：含 E 或 0/1 冲突 → E；否则有 1 → 1；有 0 → 0；全 Z → Z
        if (hasE)              result[pos] = 'E';
        else if (has0 && has1) result[pos] = 'E';
        else if (has1)         result[pos] = '1';
        else if (has0)         result[pos] = '0';
        else                   result[pos] = 'Z';
    }
    // 返回合并结果
    return result;
}

// 定义"逐位取反并处理三态"静态函数
QString EventEngine::mapUnary(const QString& src, int bw) {
    // 准备输出串
    QString out; out.reserve(bw);
    // 源串长度
    int n = src.length();
    // 逐位处理
    for (int b = 0; b < bw; ++b) {
        // 取源位，缺失按 '0' 处理
        QChar c = (b < n) ? src[b] : QChar('0');
        // Z/E 映射为 E，0↔1
        if (c == 'Z' || c == 'E') out += 'E';
        else                       out += (c == '1') ? '0' : '1';
    }
    // 返回取反结果
    return out;
}

// 定义"调度一次 Recalc 事件"辅助函数：at 时刻重算节点 idx
void EventEngine::scheduleRecalc(int idx, quint64 at) {
    // 准备事件
    Event ev;
    // 类型为重算
    ev.kind = Event::Recalc;
    // 事件时间
    ev.time = at;
    // 分配事件序号，保证同刻稳定顺序
    ev.seq = ++m_seq;
    // 待重算节点索引
    ev.nodeIdx = idx;
    // Recalc 不使用端口字段
    ev.port = 0;
    // Recalc 不使用目标字段
    ev.toIdx = -1;
    ev.toPort = -1;
    // 入队
    m_queue.push(ev);
}

// 定义"调度一次 Wire 传播事件"辅助函数：at 时刻把源端口值传播到目标输入端口
void EventEngine::scheduleWirePropagation(int fromIdx, int fromPort,
                                          int toIdx, int toPort,
                                          const QString& value, quint64 at) {
    // 准备事件
    Event ev;
    // 类型为传播
    ev.kind = Event::Wire;
    // 事件时间
    ev.time = at;
    // 分配事件序号
    ev.seq = ++m_seq;
    // 源节点索引
    ev.nodeIdx = fromIdx;
    // 源输出端口
    ev.port = fromPort;
    // 目标节点索引
    ev.toIdx = toIdx;
    // 目标输入端口
    ev.toPort = toPort;
    // 携带的输出值
    ev.value = value;
    // 入队
    m_queue.push(ev);
}

// 定义"单步推进"接口：处理队列中一个最早事件，返回是否还有事件可处理
bool EventEngine::step() {
    // 队列空则无事可做
    if (m_queue.empty()) return false;
    // 取最早事件
    Event ev = m_queue.top();
    // 弹出
    m_queue.pop();
    // 时间推进到事件时间（事件时间只会 >= 当前时间）
    if (ev.time > m_time) m_time = ev.time;
    // 处理事件
    processEvent(ev);
    // 累计处理计数
    ++m_totalEventsProcessed;
    // 返回 true 表示处理成功
    return true;
}

// 定义"只处理当前时刻事件"接口：不推进仿真时间，用于 rebuild 后立即收敛
int EventEngine::drainNow(quint64 budget) {
    // 预算为 0 时使用默认预算
    if (budget == 0) budget = m_opts.runBudget;
    // 已处理事件数
    int processed = 0;
    // 记录起始时间：只处理时间等于起始时间的事件
    quint64 startTime = m_time;
    // 队列非空时循环
    while (!m_queue.empty()) {
        // 队首事件已超过当前时刻则停止（留给后续 run 推进）
        if (m_queue.top().time > startTime) break;
        // 达到预算上限则停止
        if (quint64(processed) >= budget) break;
        // 取最早事件
        Event ev = m_queue.top();
        // 弹出
        m_queue.pop();
        // 处理事件
        processEvent(ev);
        // 计数
        ++processed;
        // 累计处理计数
        ++m_totalEventsProcessed;
    }
    // 返回本次处理的事件数
    return processed;
}

// 定义"推进仿真时间"接口：处理所有 <= endTime 的事件，返回是否已处理到 endTime 或队列清空
bool EventEngine::run(quint64 units, quint64 budget) {
    // 预算为 0 时使用默认预算
    if (budget == 0) budget = m_opts.runBudget;
    // 目标结束时间
    quint64 endTime = m_time + units;
    // 已处理事件数
    quint64 processed = 0;

    // 队列非空时循环
    while (!m_queue.empty()) {
        // 队首事件超过结束时间：时间推进到 endTime，返回 true（已处理到目标时间）
        if (m_queue.top().time > endTime) {
            if (m_time < endTime) m_time = endTime;
            return true;
        }
        // 达到预算上限：返回 false 表示未处理完，需调用方再次 run
        if (processed >= budget) {
            return false;
        }
        // 取最早事件
        Event ev = m_queue.top();
        // 弹出
        m_queue.pop();
        // 时间推进到事件时间
        if (ev.time > m_time) m_time = ev.time;
        // 处理事件
        processEvent(ev);
        // 计数
        ++processed;
        // 累计处理计数
        ++m_totalEventsProcessed;
    }

    // 队列提前清空：时间仍推进到 endTime，返回 true
    if (m_time < endTime) m_time = endTime;
    return true;
}

// 定义"处理单个事件"接口：按事件类型分派到重算或传播逻辑
void EventEngine::processEvent(const Event& ev) {
    // Recalc 事件：直接重算目标节点
    if (ev.kind == Event::Recalc) {
        recalcNode(ev.nodeIdx);
        return;
    }

    // 以下是 Wire 传播事件
    // 目标节点索引
    int ti = ev.toIdx;
    // 目标端口索引
    int tp = ev.toPort;
    // 目标节点索引非法则丢弃该事件
    if (ti < 0 || ti >= m_nodes.size()) return;
    // 取目标节点引用
    Node& tn = m_nodes[ti];
    // 目标端口索引非法则丢弃
    if (tp < 0 || tp >= tn.inVals.size()) return;

    // 目标输入端口键
    quint64 tkey = pkey(ti, tp);
    // 源驱动源键
    qint64  dk   = dkey(ev.nodeIdx, ev.port);
    // 更新该驱动源最近输出值
    QHash<qint64, QString>& dv = m_portDriverValues[tkey];
    dv[dk] = ev.value;

    // 复用成员缓冲区收集该端口所有驱动值，避免每事件堆分配
    QVector<QString>& driverVals = m_driverValBuf;
    driverVals.clear();
    // 查目标端口的驱动源列表
    auto it = m_inDrivers.constFind(tkey);
    if (it != m_inDrivers.constEnd()) {
        // 预分配容量
        driverVals.reserve(it.value().size());
        // 逐个驱动源取值
        for (const auto& d : it.value()) {
            // 驱动源键
            qint64 k = dkey(d.fromIdx, d.fromPort);
            // 先查驱动值表
            QString v = dv.value(k, QString());
            // 表中缺失则回退读源节点输出
            if (v.isEmpty()) {
                const Node& fn = m_nodes[d.fromIdx];
                if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                    v = fn.outVals[d.fromPort];
            }
            // 追加驱动值
            driverVals.append(v);
        }
    } else {
        // 无驱动表记录（理论上不会），直接用事件携带值
        driverVals.append(ev.value);
    }

    // 端口位宽
    int bw = tn.inVals[tp].length();
    // 位宽无效则回退节点位宽
    if (bw < 1) bw = tn.bitWidth;
    // 多驱合并得到新输入值
    QString merged = resolveMultiDriver(driverVals, bw);

    // 输入值未变化则无需重算下游
    if (tn.inVals[tp] == merged) return;
    // 写入新输入值
    tn.inVals[tp] = merged;
    // 输入变化，重算该节点
    recalcNode(ti);
}

// 定义"重算节点"接口：与旧值比较，变化则记录脏节点并按门延迟调度下游传播
void EventEngine::recalcNode(int idx) {
    // 取节点引用
    Node& n = m_nodes[idx];
    // 按当前输入计算新输出
    QVector<QString> newOuts = computeOutputs(n, n.inVals);
    // 是否发生变化
    bool changed = false;
    // 输出端口数量变化：视为整体变化
    if (newOuts.size() != n.outVals.size()) {
        n.outVals = newOuts;
        changed = true;
        // 数量相同则逐端口比较
    } else {
        for (int i = 0; i < newOuts.size(); ++i) {
            if (n.outVals[i] != newOuts[i]) { n.outVals[i] = newOuts[i]; changed = true; }
        }
    }
    // 无变化则不传播
    if (!changed) return;

    // 记录输出变化的节点，供 Circuit 侧增量更新（去重）
    markDirty(n.qid);

    // 下游事件时间 = 当前时间 + 该类型门延迟
    quint64 at = m_time + gateDelayFor(n.type);
    // 逐个输出端口调度传播
    for (int p = 0; p < n.outVals.size(); ++p) {
        // 查该端口的目标列表
        auto it = m_outDests.constFind(pkey(idx, p));
        // 无目标则跳过
        if (it == m_outDests.constEnd()) continue;
        // 当前输出值
        const QString v = n.outVals[p];
        // 对每个目标输入端口调度 Wire 传播
        for (const auto& d : it.value()) {
            scheduleWirePropagation(idx, p, d.first, d.second, v, at);
        }
    }
}

// 定义"计算节点输出"纯函数：数据源类型自行处理，组合逻辑委托 ComponentLogic
QVector<QString> EventEngine::computeOutputs(const Node& n,
                                             const QVector<QString>& inputs) const
{
    // 节点位宽
    const int bw = n.bitWidth;
    // 节点类型
    const QString& t = n.type;

    // ---- 数据源相关类型：各自处理 ----
    // input/const/output/led：输出就是其输入值（补齐/截断到位宽）
    if (t == "input" || t == "const" || t == "output" || t == "led") {
        // 取第一个输入，缺省全 0
        QString s = inputs.isEmpty() ? QString(bw, '0') : inputs[0];
        // 长度不符则补齐/截断
        if (s.length() != bw) {
            if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
            else                 s = s.right(bw);
        }
        // 返回单端口输出
        return { s };
    }
    // clock：输出由外部 trigger 直接写入，这里原样返回当前值
    if (t == "clock") {
        if (!n.outVals.isEmpty()) return { n.outVals[0] };
        return { QString(bw, '0') };
    }

    // ---- 纯组合逻辑类型：委托 ComponentLogic 统一实现 ----
    // 覆盖 not/and/or/nand/nor/xor/xnor/tgate/ntran/ptran/splitter/hub
    // 未识别类型（如 sub）返回空，保持与旧行为一致（sub 节点输出由外部驱动）
    return ComponentLogic::computeCombinationalOutputs(
        t, inputs, bw, n.raw.value("outputSplits").toList());
}