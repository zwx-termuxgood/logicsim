#include "EventEngine.h"
// 引入本文件要实现的 EventEngine 类完整声明：定义其成员函数前必须看到类定义
#include "../ComponentLogic.h"
// 引入元件逻辑库：查询端口数量/位宽（inputCount/inputBitWidth/outputCount/outputBitWidth）的唯一来源
#include <algorithm>
// 引入算法库：本文件按需使用（排序/最值等工具，当前用于保持与其它 build 文件一致的依赖）

// 定义"构建引擎"入口：清空旧状态后递归展开子电路，初始化值并预传播
void EventEngine::build(const QVariantList& rootComps,
                        const QVariantList& rootWires,
                        const QHash<QString, QVariantList>& subComps,
                        const QHash<QString, QVariantList>& subWires)
{
    // 先完全清空旧的节点、索引、表、队列与计数器
    clear();
    // 从根上下文开始递归建图：根前缀为空串、深度 0
    buildRecursive(rootComps, rootWires, subComps, subWires, QString(), 0);
    // 初始化各节点初值：input 取 inputBits，clock 取 outputPorts，其余置零
    initValues();
    // 初始化驱动表与输入端口值，并做预传播使 build 后状态接近稳定
    initDriverValues();
}

// 定义"递归建图"辅助函数：先建本层节点，再递归展开 sub 实例，最后处理本层连线
void EventEngine::buildRecursive(const QVariantList& comps,
                                 const QVariantList& wires,
                                 const QHash<QString, QVariantList>& subComps,
                                 const QHash<QString, QVariantList>& subWires,
                                 const QString& prefix,
                                 int depth)
{
    // 递归深度保护：超过 32 层视为异常引用链，直接停止展开，避免栈溢出
    if (depth > 32) return;

    // 本层"本地 ID → 节点索引"映射，供本层连线解析两端元件
    QHash<QString, int> localIdx;
    // 预分配容量，减少哈希表反复扩容
    localIdx.reserve(comps.size() * 2);

    // 第一遍：为本层每个元件建节点
    for (const auto& cv : comps) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 取类型
        QString type = c.value("type").toString();
        // 文本元件没有端口、不参与求值，跳过不建节点
        if (type == "text") continue;

        // 取本地 ID
        QString localId = c.value("id").toString();
        // 拼接限定 ID：根上下文即本地 ID，子电路内加实例路径前缀
        QString qid = prefix.isEmpty() ? localId : (prefix + "/" + localId);
        // 同一 qid 已建过节点则跳过，防止重复建节点导致索引错乱
        if (m_idToIdx.contains(qid)) continue;

        // 准备节点
        Node n;
        // 记录限定 ID
        n.qid = qid;
        // 记录本地 ID
        n.localId = localId;
        // 记录类型
        n.type = type;
        // 取位宽，下限钳到 1
        n.bitWidth = c.value("bitWidth").toInt();
        if (n.bitWidth < 1) n.bitWidth = 1;
        // 保存元件原始 map，供 computeOutputs 读取 outputSplits 等字段
        n.raw = c;
        // 标记数据源类型
        if (type == "input" || type == "const") n.isInput  = true;
        // 标记时钟
        else if (type == "clock")  n.isClock  = true;
        // 标记输出探针
        else if (type == "output") n.isOutput = true;
        // 标记 LED
        else if (type == "led")    n.isLed    = true;

        // 数据源/输出探针：固定 1 进 1 出（各自一个位宽宽度的值）
        if (type == "input" || type == "const" || type == "output") {
            // 一个输入端口占位
            n.inVals.append(QString(n.bitWidth, '0'));
            // 一个输出端口占位
            n.outVals.append(QString(n.bitWidth, '0'));
            // LED 只有输入端口，没有输出
        } else if (type == "led") {
            // 取输入端口数量
            int ic = ComponentLogic::inputCount(c);
            // 按各端口位宽初始化输入占位值
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentLogic::inputBitWidth(c, i), '0'));
            // 其余类型：按输入/输出端口数量与各自位宽初始化占位值
        } else {
            // 输入端口数量
            int ic = ComponentLogic::inputCount(c);
            // 输出端口数量
            int oc = ComponentLogic::outputCount(c);
            // 初始化输入端口占位值
            for (int i = 0; i < ic; ++i)
                n.inVals.append(QString(ComponentLogic::inputBitWidth(c, i), '0'));
            // 初始化输出端口占位值
            for (int i = 0; i < oc; ++i)
                n.outVals.append(QString(ComponentLogic::outputBitWidth(c, i), '0'));
        }

        // 新节点索引即当前节点数
        int idx = m_nodes.size();
        // 登记 qid → 索引
        m_idToIdx[qid] = idx;
        // 登记本层本地 ID → 索引
        localIdx[localId] = idx;
        // 追加节点
        m_nodes.append(n);
    }

    // 第二遍：递归展开本层的 sub 实例
    for (const auto& cv : comps) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 只处理 sub 类型
        if (c.value("type").toString() != "sub") continue;
        // 取实例本地 ID
        QString localId = c.value("id").toString();
        // 拼接实例限定 ID，作为子电路内部节点的前缀
        QString qid = prefix.isEmpty() ? localId : (prefix + "/" + localId);
        // 取引用的子电路 id
        QString subId = c.value("subId").toString();
        // 查子电路定义
        auto sit = subComps.constFind(subId);
        // 定义不存在则无法展开，跳过
        if (sit == subComps.constEnd()) continue;
        // 递归展开：子电路元件与连线以实例 qid 为前缀，深度 +1
        buildRecursive(sit.value(), subWires.value(subId),
                       subComps, subWires, qid, depth + 1);
    }

    // 第三遍：处理本层连线，建立驱动表与目标表
    for (const auto& wv : wires) {
        // 连线转 map
        QVariantMap w = wv.toMap();
        // 起点本地 ID
        QString fromLocal = w.value("fromComp").toString();
        // 终点本地 ID
        QString toLocal   = w.value("toComp").toString();
        // 起点端口索引
        int fromPort = w.value("fromPort").toInt();
        // 终点端口索引
        int toPort   = w.value("toPort").toInt();

        // 起点在本层的节点索引（未绑定端口的 T 型线为 -1）
        int fi = localIdx.value(fromLocal, -1);
        // 终点在本层的节点索引
        int ti = localIdx.value(toLocal, -1);
        // 任一端不在本层（悬空端或跨层），跳过该线
        if (fi < 0 || ti < 0) continue;

        // 起点是 sub 实例：把连线重定向到子电路内部对应的 output 探针节点
        if (m_nodes[fi].type == "sub") {
            // 取引用的子电路 id
            QString subId = m_nodes[fi].raw.value("subId").toString();
            // 取实例限定 ID 作为内部节点前缀
            QString subQid = m_nodes[fi].qid;
            // 查子电路定义
            auto sit = subComps.constFind(subId);
            // 定义不存在则跳过
            if (sit == subComps.constEnd()) continue;
            // 计数器：定位第 fromPort 个 output 元件
            int cnt = 0;
            // 内部 output 元件的限定 ID
            QString innerQid;
            // 遍历子电路元件，找第 fromPort 个 output
            for (const auto& scv : sit.value()) {
                // 元件转 map
                QVariantMap sc = scv.toMap();
                // 只关心 output 类型
                if (sc.value("type").toString() != "output") continue;
                // 命中目标序号则记录内部 qid 并跳出
                if (cnt == fromPort) {
                    innerQid = subQid + "/" + sc.value("id").toString();
                    break;
                }
                // 序号推进
                cnt++;
            }
            // 没找到对应 output 则跳过该线
            if (innerQid.isEmpty()) continue;
            // 查内部 output 节点索引
            int innerIdx = idxOf(innerQid);
            // 节点不存在则跳过
            if (innerIdx < 0) continue;
            // 起点重定向为内部 output 节点
            fi = innerIdx;
            // output 探针只有端口 0
            fromPort = 0;
        }
        // 终点是 sub 实例：把连线重定向到子电路内部对应的 input 元件节点
        if (m_nodes[ti].type == "sub") {
            // 取引用的子电路 id
            QString subId = m_nodes[ti].raw.value("subId").toString();
            // 取实例限定 ID 作为内部节点前缀
            QString subQid = m_nodes[ti].qid;
            // 查子电路定义
            auto sit = subComps.constFind(subId);
            // 定义不存在则跳过
            if (sit == subComps.constEnd()) continue;
            // 计数器：定位第 toPort 个 input 元件
            int cnt = 0;
            // 内部 input 元件的限定 ID
            QString innerQid;
            // 遍历子电路元件，找第 toPort 个 input
            for (const auto& scv : sit.value()) {
                // 元件转 map
                QVariantMap sc = scv.toMap();
                // 只关心 input 类型
                if (sc.value("type").toString() != "input") continue;
                // 命中目标序号则记录内部 qid 并跳出
                if (cnt == toPort) {
                    innerQid = subQid + "/" + sc.value("id").toString();
                    break;
                }
                // 序号推进
                cnt++;
            }
            // 没找到对应 input 则跳过该线
            if (innerQid.isEmpty()) continue;
            // 查内部 input 节点索引
            int innerIdx = idxOf(innerQid);
            // 节点不存在则跳过
            if (innerIdx < 0) continue;
            // 终点重定向为内部 input 节点
            ti = innerIdx;
            // input 探针只有端口 0
            toPort = 0;
        }

        // 登记输出端口 → 目标列表
        m_outDests[pkey(fi, fromPort)].append(qMakePair(ti, toPort));
        // 登记输入端口 → 驱动源列表
        m_inDrivers[pkey(ti, toPort)].append({fi, fromPort});
    }
}

// 定义"初始化节点初值"辅助函数
void EventEngine::initValues() {
    // 遍历所有节点
    for (int i = 0; i < m_nodes.size(); ++i) {
        // 取节点引用
        Node& n = m_nodes[i];
        // 数据源：从 inputBits 位列表生成二进制串作为初值
        if (n.isInput) {
            // 取位列表
            QVariantList bits = n.raw.value("inputBits").toList();
            // 准备二进制串
            QString s;
            // 预分配位宽容量
            s.reserve(n.bitWidth);
            // 逐位生成，长度固定为位宽
            for (int b = 0; b < n.bitWidth; ++b)
                s += (b < bits.size() && bits[b].toBool()) ? '1' : '0';
            // 写入输出端口初值
            if (!n.outVals.isEmpty()) n.outVals[0] = s;
            // 写入输入端口初值（数据源输入端口与输出同值）
            if (!n.inVals.isEmpty())  n.inVals[0]  = s;
            // 时钟：从 outputPorts 读初值
        } else if (n.isClock) {
            // 取输出端口值列表
            QVariantList ops = n.raw.value("outputPorts").toList();
            // 有值则取第一个，否则置零串
            QString s = ops.isEmpty() ? QString(n.bitWidth, '0')
                                      : ops[0].toString();
            // 长度不符则重置为零串，保证与位宽一致
            if (s.length() != n.bitWidth) s = QString(n.bitWidth, '0');
            // 写入输出端口初值
            if (!n.outVals.isEmpty()) n.outVals[0] = s;
            // 写入输入端口初值
            if (!n.inVals.isEmpty())  n.inVals[0]  = s;
        }
        // 其余类型保持占位值（全 0），由后续预传播计算
    }
}

// 定义"初始化驱动值并预传播"辅助函数：建立多驱合并结果，迭代 32 轮使状态接近稳定
void EventEngine::initDriverValues() {
    // 第一遍：为每个输入端口的每个驱动源建立"驱动源键 → 当前输出值"记录
    for (auto it = m_inDrivers.constBegin(); it != m_inDrivers.constEnd(); ++it) {
        // 目标输入端口键
        quint64 tkey = it.key();
        // 该输入端口的驱动源值表
        QHash<qint64, QString> dv;
        // 遍历该端口的驱动源
        for (const auto& d : it.value()) {
            // 源节点索引非法则跳过
            if (d.fromIdx < 0 || d.fromIdx >= m_nodes.size()) continue;
            // 取源节点
            const Node& fn = m_nodes[d.fromIdx];
            // 驱动值
            QString v;
            // 源端口索引合法则读取其当前输出值
            if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                v = fn.outVals[d.fromPort];
            // 值为空（如未初始化）则按该端口位宽补零串
            if (v.isEmpty()) {
                int w = ComponentLogic::outputBitWidth(fn.raw, d.fromPort);
                if (w < 1) w = 1;
                v = QString(w, '0');
            }
            // 记录该驱动源当前值
            dv[dkey(d.fromIdx, d.fromPort)] = v;
        }
        // 写入端口驱动值表
        m_portDriverValues[tkey] = dv;
    }

    // 第二遍：对每个输入端口做多驱合并，算出初始输入值
    for (auto it = m_inDrivers.constBegin(); it != m_inDrivers.constEnd(); ++it) {
        // 目标输入端口键
        quint64 tkey = it.key();
        // 取低 32 位用于拆解节点索引与端口号
        quint32 lo = quint32(tkey & 0xFFFFFFFF);
        // 目标节点索引
        int ti = int(lo >> 16);
        // 目标端口索引
        int tp = int(tkey & 0xFFFF);
        // 目标节点索引非法则跳过
        if (ti < 0 || ti >= m_nodes.size()) continue;
        // 取目标节点引用
        Node& tn = m_nodes[ti];
        // 目标端口索引非法则跳过
        if (tp < 0 || tp >= tn.inVals.size()) continue;

        // 收集该端口所有驱动源当前值
        QVector<QString> driverVals;
        // 预分配容量
        driverVals.reserve(it.value().size());
        // 逐个驱动源取值
        for (const auto& d : it.value()) {
            // 先查驱动值表
            QString v = m_portDriverValues[tkey].value(
                dkey(d.fromIdx, d.fromPort), QString());
            // 表中缺失则回退读源节点输出
            if (v.isEmpty()) {
                const Node& fn = m_nodes[d.fromIdx];
                if (d.fromPort >= 0 && d.fromPort < fn.outVals.size())
                    v = fn.outVals[d.fromPort];
            }
            // 追加驱动值
            driverVals.append(v);
        }
        // 端口位宽
        int bw = tn.inVals[tp].length();
        // 位宽无效则回退节点位宽
        if (bw < 1) bw = tn.bitWidth;
        // 多驱合并后写入输入端口初始值
        tn.inVals[tp] = resolveMultiDriver(driverVals, bw);
    }

    // 第三遍：迭代预传播，最多 32 轮，使各节点输出尽量收敛
    for (int pass = 0; pass < 32; ++pass) {
        // 本轮是否有任何节点输出变化
        bool anyChanged = false;
        // 遍历所有节点
        for (int i = 0; i < m_nodes.size(); ++i) {
            // 取节点引用
            Node& n = m_nodes[i];
            // 数据源与时钟由其自身状态决定，不参与组合求值
            if (n.isInput || n.isClock) continue;

            // 按当前输入计算新输出
            QVector<QString> newOuts = computeOutputs(n, n.inVals);
            // 与旧输出比较，判断是否变化
            bool changed = (newOuts != n.outVals);
            // 无变化则跳过传播
            if (!changed) continue;

            // 更新输出值
            n.outVals = newOuts;
            // 标记本轮有变化
            anyChanged = true;

            // 把变化的每个输出端口值传播到下游输入端口
            for (int p = 0; p < newOuts.size(); ++p) {
                // 查该输出端口的目标列表
                auto it = m_outDests.constFind(pkey(i, p));
                // 无目标则跳过
                if (it == m_outDests.constEnd()) continue;
                // 遍历每个目标输入端口
                for (const auto& d : it.value()) {
                    // 目标节点索引
                    int ti = d.first;
                    // 目标端口索引
                    int tp = d.second;
                    // 目标节点索引非法则跳过
                    if (ti < 0 || ti >= m_nodes.size()) continue;
                    // 取目标节点引用
                    Node& tn = m_nodes[ti];
                    // 目标端口索引非法则跳过
                    if (tp < 0 || tp >= tn.inVals.size()) continue;

                    // 目标输入端口键
                    quint64 tkey = pkey(ti, tp);
                    // 源驱动源键
                    qint64  dk   = dkey(i, p);
                    // 更新该驱动源最近输出值
                    m_portDriverValues[tkey][dk] = newOuts[p];

                    // 重新收集该输入端口的全部驱动值
                    QVector<QString> driverVals;
                    // 预分配容量
                    driverVals.reserve(it.value().size());
                    // 查目标端口的驱动源列表
                    auto it2 = m_inDrivers.constFind(tkey);
                    if (it2 != m_inDrivers.constEnd()) {
                        // 逐个驱动源取值
                        for (const auto& dd : it2.value()) {
                            QString v = m_portDriverValues[tkey].value(
                                dkey(dd.fromIdx, dd.fromPort), QString());
                            // 表中缺失则回退读源节点输出
                            if (v.isEmpty()) {
                                const Node& fn = m_nodes[dd.fromIdx];
                                if (dd.fromPort >= 0 && dd.fromPort < fn.outVals.size())
                                    v = fn.outVals[dd.fromPort];
                            }
                            // 追加驱动值
                            driverVals.append(v);
                        }
                    }
                    // 端口位宽
                    int bw = tn.inVals[tp].length();
                    // 位宽无效则回退节点位宽
                    if (bw < 1) bw = tn.bitWidth;
                    // 多驱合并后更新目标输入端口值
                    tn.inVals[tp] = resolveMultiDriver(driverVals, bw);
                }
            }
        }
        // 本轮无任何变化说明已收敛，提前结束迭代
        if (!anyChanged) break;
    }

    // 第四遍：为所有非数据源/非时钟节点调度一次当前时刻的 Recalc 事件，
    // 让引擎进入事件驱动阶段后能从一致状态继续
    for (int i = 0; i < m_nodes.size(); ++i) {
        // 取节点
        const Node& n = m_nodes[i];
        // 数据源与时钟跳过
        if (n.isInput || n.isClock) continue;
        // 调度 Recalc 事件
        scheduleRecalc(i, m_time);
    }
}