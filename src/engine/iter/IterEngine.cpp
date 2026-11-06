#include "IterEngine.h"
// 引入本文件要实现的 IterEngine 类完整声明：定义其成员函数前必须看到类定义
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：输入/输出端口数量与位宽、组合逻辑计算都委托它，保证与事件引擎一致
#include "logic/WireNetworkSolver.h"
// 引入连线网络求解器：生成虚拟线（电气互联拓扑），供迭代引擎建图使用
#include "core/ValueCodec.h"
// 引入值编解码器：input/const 元件的 inputBits 转二进制串时使用
#include <QVector>
// 引入 QVector：引擎内部高频使用的轻量容器（输入/输出值、拓扑序等）
#include <QDebug>
// 引入 QDebug：调试输出用（本文件保留 include 以便后续加日志）

// 前置声明递归求值函数：evalInstance 会回调它，而它又调用 evalInstance，互相引用需先声明
static bool evalRecursive(CircuitContext& ctx,
                          const QHash<QString, CircuitContext>& subContexts,
                          int depth);

// 定义"多驱合并（含 E/Z 三态）"静态辅助函数：迭代引擎版多驱合并
static QString resolveMultiDriverE(const QVector<QString>& vals, int bw) {
    // 位宽下限钳到 1
    if (bw < 1) bw = 1;
    // 无驱动源：视为全 0
    if (vals.isEmpty()) return QString(bw, '0');
    // 只有一个驱动源：补齐/截断到目标位宽后直接返回
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
        if (hasE) result[pos] = 'E';
        else if (has0 && has1) result[pos] = 'E';
        else if (has1) result[pos] = '1';
        else if (has0) result[pos] = '0';
        else result[pos] = 'Z';
    }
    // 返回合并结果
    return result;
}

// 定义"计算单个元件"接口：收集输入、按类型算输出，仅在变化时写回并返回 true
bool IterEngine::evalOne(QVariantMap& c,
                         const QHash<QString, QVector<QPair<int,int>>>& inputMap,
                         const QVector<QVariantMap>& comps) {
    // 取元件 id
    const QString id = c.value("id").toString();
    // 取类型
    const QString t = c.value("type").toString();
    // 文本元件不参与求值
    if (t == "text") return false;

    // 取位宽，钳位到 [1,64]
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;

    // 输入端口数量
    const int ic = ComponentLogic::inputCount(c);
    // 旧输入端口值列表，用于比较是否变化
    const QVariantList oldInList = c.value("inputPortValues").toList();
    // inputMap 的 key 前缀："元件id:端口号"
    const QString idPrefix = id + ':';

    // 准备新输入端口值列表
    QVariantList newInList;
    // 预分配容量
    newInList.reserve(ic);
    // 输入是否发生变化
    bool inputChanged = false;

    // 逐个输入端口收集驱动值并合并
    for (int p = 0; p < ic; ++p) {
        // 该端口位宽，钳位到 [1,64]
        int bwp = ComponentLogic::inputBitWidth(c, p);
        if (bwp < 1) bwp = 1;
        if (bwp > 64) bwp = 64;

        // 该端口合并后的输入值
        QString bits;
        // 查该输入端口的驱动源列表
        auto it = inputMap.constFind(idPrefix + QString::number(p));
        if (it != inputMap.constEnd()) {
            // 收集各驱动源的当前输出值
            QVector<QString> driverVals;
            for (const auto& pair : it.value()) {
                // 源元件索引与源端口
                int fi = pair.first, fp = pair.second;
                // 源元件索引合法才取值
                if (fi >= 0 && fi < comps.size()) {
                    // 源元件的输出端口值列表
                    const QVariantList fo = comps[fi].value("outputPorts").toList();
                    // 源端口索引合法才取值
                    if (fp >= 0 && fp < fo.size()) {
                        // 源输出值
                        QString src = fo[fp].toString();
                        // 位宽与目标端口不符视为错误态 E（整串）
                        if (src.length() != bwp) src = QString(bwp, 'E');
                        // 追加驱动值
                        driverVals.append(src);
                    }
                }
            }
            // 有驱动值则多驱合并
            if (!driverVals.isEmpty()) bits = resolveMultiDriverE(driverVals, bwp);
        }
        // 无驱动：保留旧值，旧值也没有则置零串
        if (bits.isEmpty()) {
            if (p < oldInList.size()) bits = oldInList[p].toString();
            if (bits.isEmpty()) bits = QString(bwp, '0');
        }
        // 与旧值比较，不同则标记输入变化
        if (p >= oldInList.size() || oldInList[p].toString() != bits) inputChanged = true;
        // 追加到新输入列表
        newInList.append(bits);
    }

    // 本元件是否有任何变化
    bool changed = false;
    // 输入变化则写回
    if (inputChanged) {
        c["inputPortValues"] = newInList;
        changed = true;
    }

    // 准备新输出端口值列表
    QVariantList newOutList;
    // 数据源类型：取 outputOverride（含 Z/E 时用）或 inputBits
    if (t == "input" || t == "const") {
        // 取输出覆盖值
        QVariant ov = c.value("outputOverride");
        // 转成字符串（无效则为空）
        QString ovStr = ov.isValid() ? ov.toString() : QString();
        // 有覆盖值则优先使用
        if (!ovStr.isEmpty()) newOutList.append(ovStr);
        // 否则由 inputBits 生成二进制串
        else newOutList.append(ValueCodec::bitsToString(c.value("inputBits").toList(), bw));
        // 时钟：输出由外部写入 outputPorts，这里原样保留
    } else if (t == "clock") {
        newOutList = c.value("outputPorts").toList();
        if (newOutList.isEmpty()) newOutList.append(QString(bw, '0'));
        // LED/输出探针：输出就是第一个输入值
    } else if (t == "led" || t == "output") {
        if (!newInList.isEmpty()) newOutList.append(newInList[0]);
        else newOutList.append(QString(bw, '0'));
        // 子电路实例：输出由 evalInstance 写入 outputPorts，这里原样保留
    } else if (t == "sub") {
        newOutList = c.value("outputPorts").toList();
        // 纯组合逻辑：委托 ComponentLogic 统一计算
    } else {
        // 把输入列表转成 QVector<QString>
        QVector<QString> ins;
        for (const auto& v : newInList) ins.append(v.toString());
        // 计算组合逻辑输出
        QVector<QString> outs = ComponentLogic::computeCombinationalOutputs(
            t, ins, bw, c.value("outputSplits").toList());
        // 转回 QVariantList
        for (const auto& s : outs) newOutList.append(s);
    }
    // 取旧输出列表
    const QVariantList oldOutList = c.value("outputPorts").toList();
    // 输出有变化则写回并标记
    if (oldOutList != newOutList) {
        c["outputPorts"] = newOutList;
        changed = true;
    }
    // 返回本元件是否变化
    return changed;
}

// 定义"对单个上下文做一轮求值"接口
bool IterEngine::evaluateContext(CircuitContext& ctx) {
    // 元件数量
    const int n = ctx.components.size();
    // 空上下文无需求值
    if (n == 0) return false;

    // 本地 ID → 索引映射
    QHash<QString, int> idToIdx;
    // 预分配容量
    idToIdx.reserve(n * 2);
    // 建立映射
    for (int i = 0; i < n; ++i)
        idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);

    // 关键：先生成虚拟 wire（电气互联），注入到临时 wire 列表
    QVariantList virtualWires = WireNetworkSolver::generateVirtualWires(
        ctx.components, ctx.wires);
    // 合并真实连线与虚拟线，统一建图
    QVariantList allWires = ctx.wires;
    for (const auto& vw : virtualWires) allWires.append(vw);

    // 后继表：用于拓扑排序
    QVector<QVector<int>> succ(n);
    // 入度表
    QVector<int> inDeg(n, 0);
    // "目标端口 → 驱动源列表"映射：key 为 "元件id:端口号"
    QHash<QString, QVector<QPair<int,int>>> inputMap;
    // 预分配容量
    inputMap.reserve(allWires.size() * 2);

    // 遍历所有线（含虚拟线）建图
    for (const auto& wv : allWires) {
        // 线转 map
        QVariantMap w = wv.toMap();
        // 源元件索引
        int si = idToIdx.value(w.value("fromComp").toString(), -1);
        // 目标元件索引
        int di = idToIdx.value(w.value("toComp").toString(), -1);
        // 任一端不在本上下文则跳过
        if (si < 0 || di < 0) continue;
        // 记录后继
        succ[si].append(di);
        // 目标入度 +1
        inDeg[di]++;
        // 构造目标端口 key
        QString key = w.value("toComp").toString() + ':' +
                      QString::number(w.value("toPort").toInt());
        // 记录该端口的驱动源
        inputMap[key].append({si, w.value("fromPort").toInt()});
    }

    // 元件 map 的工作副本：本轮求值在副本上累积，最后统一写回
    QVector<QVariantMap> comps(n);
    for (int i = 0; i < n; ++i) comps[i] = ctx.components[i].toMap();

    // 拓扑序结果
    QVector<int> order; order.reserve(n);
    // 拓扑排序队列
    QVector<int> queue; queue.reserve(n);
    // 入度为 0 的节点先入队
    for (int i = 0; i < n; ++i) if (inDeg[i] == 0) queue.append(i);
    // 队列头指针
    int qi = 0;
    // 逐层处理
    while (qi < queue.size()) {
        // 取队头节点
        int u = queue[qi++];
        // 加入拓扑序
        order.append(u);
        // 所有后继入度减 1，减到 0 入队
        for (int v : succ[u]) if (--inDeg[v] == 0) queue.append(v);
    }

    // 拓扑序长度小于节点数说明存在环
    const bool hasCycle = (order.size() < n);
    // 本轮是否有任何元件变化
    bool anyChanged = false;
    // 各元件是否变化标记
    QVector<bool> changed(n, false);

    // 按拓扑序求值（无环时一次即可收敛）
    for (int idx : order) {
        if (evalOne(comps[idx], inputMap, comps)) {
            anyChanged = true;
            changed[idx] = true;
        }
    }

    // 有环：对环内节点做迭代求值，最多 200 轮直到收敛
    if (hasCycle) {
        // 收集入度仍 > 0 的节点（即处于环中的节点）
        QVector<int> cycNodes;
        for (int i = 0; i < n; ++i) if (inDeg[i] > 0) cycNodes.append(i);
        // 迭代求值
        for (int iter = 0; iter < 200; ++iter) {
            // 本轮是否有变化
            bool iterChanged = false;
            // 逐个环内节点求值
            for (int idx : cycNodes) {
                if (evalOne(comps[idx], inputMap, comps)) {
                    iterChanged = true;
                    changed[idx] = true;
                }
            }
            // 本轮有变化则累计
            if (iterChanged) anyChanged = true;
            // 本轮无变化说明已收敛，提前结束
            else break;
        }
    }

    // 有任何变化才把变化的元件写回上下文，减少不必要的拷贝
    if (anyChanged) {
        for (int i = 0; i < n; ++i)
            if (changed[i]) ctx.components[i] = comps[i];
    }
    // 返回是否有变化
    return anyChanged;
}

// 定义"同步子电路实例端口元数据与端口值数组"接口
void IterEngine::syncSubPins(CircuitContext& ctx,
                             const QHash<QString, CircuitContext>& subContexts) {
    // 遍历上下文元件
    for (int i = 0; i < ctx.components.size(); ++i) {
        // 元件转 map
        QVariantMap c = ctx.components[i].toMap();
        // 只处理 sub 实例
        if (c.value("type").toString() != "sub") continue;
        // 取引用的子电路 id
        QString subId = c.value("subId").toString();
        // 查子电路定义
        auto sit = subContexts.find(subId);
        // 定义不存在则跳过
        if (sit == subContexts.end()) continue;

        // 准备端口名与端口位宽列表
        QVariantList subInputNames, subOutputNames, subInputWidths, subOutputWidths;
        // 遍历子电路定义元件，收集 input/output 元件信息
        for (const auto& scv : sit.value().components) {
            // 元件转 map
            auto sc = scv.toMap();
            // 取类型
            QString st = sc.value("type").toString();
            // 取名称
            QString sn = sc.value("name").toString();
            // 取位宽，下限钳到 1
            int sw = sc.value("bitWidth").toInt();
            if (sw < 1) sw = 1;
            // input 元件：记录名称（空则用 id）与位宽
            if (st == "input") {
                subInputNames.append(sn.isEmpty() ? sc.value("id").toString() : sn);
                subInputWidths.append(sw);
                // output 元件：记录名称（空则用 id）与位宽
            } else if (st == "output") {
                subOutputNames.append(sn.isEmpty() ? sc.value("id").toString() : sn);
                subOutputWidths.append(sw);
            }
        }
        // 是否有字段需要更新
        bool changed = false;
        // 逐字段比较并更新端口元数据
        if (c.value("subInputNames").toList() != subInputNames) { c["subInputNames"] = subInputNames; changed = true; }
        if (c.value("subOutputNames").toList() != subOutputNames) { c["subOutputNames"] = subOutputNames; changed = true; }
        if (c.value("subInputWidths").toList() != subInputWidths) { c["subInputWidths"] = subInputWidths; changed = true; }
        if (c.value("subOutputWidths").toList() != subOutputWidths) { c["subOutputWidths"] = subOutputWidths; changed = true; }

        // 取实例输入端口值列表
        QVariantList inVals = c.value("inputPortValues").toList();
        // 数量与端口名数量不符：补零/截断对齐
        if (inVals.size() != subInputNames.size()) {
            // 准备新列表
            QVariantList newIn;
            // 逐个端口对齐
            for (int k = 0; k < subInputNames.size(); ++k) {
                // 已有值则沿用
                if (k < inVals.size()) newIn.append(inVals[k]);
                // 缺失则按该端口位宽补零串
                else {
                    int w = subInputWidths.value(k).toInt(); if (w < 1) w = 1;
                    newIn.append(QString(w, '0'));
                }
            }
            // 写回
            c["inputPortValues"] = newIn;
            changed = true;
        }
        // 取实例输出端口值列表
        QVariantList outVals = c.value("outputPorts").toList();
        // 数量与端口名数量不符：补零/截断对齐
        if (outVals.size() != subOutputNames.size()) {
            // 准备新列表
            QVariantList newOut;
            // 逐个端口对齐
            for (int k = 0; k < subOutputNames.size(); ++k) {
                // 已有值则沿用
                if (k < outVals.size()) newOut.append(outVals[k]);
                // 缺失则按该端口位宽补零串
                else {
                    int w = subOutputWidths.value(k).toInt(); if (w < 1) w = 1;
                    newOut.append(QString(w, '0'));
                }
            }
            // 写回
            c["outputPorts"] = newOut;
            changed = true;
        }
        // 有变化才写回上下文
        if (changed) ctx.components[i] = c;
    }
}

// 定义"求值单个子电路实例"静态辅助函数
static bool evalInstance(QVariantMap& subInst,
                         const QHash<QString, CircuitContext>& subContexts,
                         int depth) {
    // 递归深度保护
    if (depth > 32) return false;
    // 取引用的子电路 id
    const QString subId = subInst.value("subId").toString();
    // 查子电路定义
    auto sit = subContexts.constFind(subId);
    // 定义不存在则无法求值
    if (sit == subContexts.constEnd()) return false;
    // 复制一份子电路上下文作为实例工作副本（不修改定义本身）
    CircuitContext instCtx = sit.value();
    // 取实例输入端口值列表
    const QVariantList inVals = subInst.value("inputPortValues").toList();
    // 子电路内 input 元件的序号
    int idx = 0;
    // 把实例输入值写入子电路内 input 元件
    for (int j = 0; j < instCtx.components.size(); ++j) {
        // 元件转 map
        QVariantMap sc = instCtx.components[j].toMap();
        // 只处理 input 元件
        if (sc.value("type").toString() != "input") continue;
        // 实例输入值用尽则停止
        if (idx >= inVals.size()) break;
        // 对应输入值
        QString s = inVals[idx].toString();
        // 取该 input 元件位宽，下限钳到 1
        int sw = sc.value("bitWidth").toInt(); if (sw < 1) sw = 1;
        // 长度不符则置零串
        if (s.length() != sw) s = QString(sw, '0');
        // 准备位列表
        QVariantList bits;
        // 逐位转成 bool
        for (int k = 0; k < s.length(); ++k) bits.append(s[k] == '1');
        // 写入 inputBits
        sc["inputBits"] = bits;
        // 含 Z/E 则记录 outputOverride，否则清空
        if (s.contains('Z') || s.contains('E')) sc["outputOverride"] = s;
        else sc["outputOverride"] = QVariant();
        // 写回子电路工作副本
        instCtx.components[j] = sc;
        // 序号推进
        ++idx;
    }
    // 递归求值子电路内部（含更深层子电路）
    evalRecursive(instCtx, subContexts, depth + 1);
    // 收集子电路内部 output 元件的值作为实例输出
    QVariantList newOut;
    for (int j = 0; j < instCtx.components.size(); ++j) {
        // 元件转 map
        QVariantMap sc = instCtx.components[j].toMap();
        // 只处理 output 元件
        if (sc.value("type").toString() != "output") continue;
        // 取 output 元件的输入端口值列表
        QVariantList inv = sc.value("inputPortValues").toList();
        // 输出值即其第一个输入值
        QString v = inv.isEmpty() ? QString() : inv[0].toString();
        // 取位宽，下限钳到 1
        int sw = sc.value("bitWidth").toInt(); if (sw < 1) sw = 1;
        // 长度不符则置零串
        if (v.length() != sw) v = QString(sw, '0');
        // 追加实例输出
        newOut.append(v);
    }
    // 实例输出有变化则写回并返回 true
    if (subInst.value("outputPorts").toList() != newOut) {
        subInst["outputPorts"] = newOut;
        return true;
    }
    // 无变化
    return false;
}

// 定义"递归求值"静态辅助函数：交替执行上下文内迭代与子电路实例求值
static bool evalRecursive(CircuitContext& ctx,
                          const QHash<QString, CircuitContext>& subContexts,
                          int depth) {
    // 递归深度保护
    if (depth > 32) return false;
    // 是否有任何变化
    bool anyChanged = false;
    // 最多 8 轮外层迭代，处理跨层反馈
    for (int iter = 0; iter < 8; ++iter) {
        // 本轮是否有变化
        bool iterChanged = false;
        // 先做一次本上下文内求值
        if (IterEngine::evaluateContext(ctx)) iterChanged = true;
        // 再逐个求值子电路实例
        for (int i = 0; i < ctx.components.size(); ++i) {
            // 元件转 map
            QVariantMap c = ctx.components[i].toMap();
            // 只处理 sub 实例
            if (c.value("type").toString() != "sub") continue;
            // 求值实例，有变化则写回并标记
            if (evalInstance(c, subContexts, depth)) {
                ctx.components[i] = c;
                iterChanged = true;
            }
        }
        // 实例输出可能影响本层，需再求值一次本上下文
        if (iterChanged) {
            if (IterEngine::evaluateContext(ctx)) iterChanged = true;
        }
        // 本轮有变化则累计
        if (iterChanged) anyChanged = true;
        // 本轮无变化说明已收敛，提前结束
        else break;
    }
    // 返回是否有变化
    return anyChanged;
}

// 定义"全量求值"接口：先同步所有子电路引脚，再反复递归求值直到收敛
void IterEngine::evaluateAll(CircuitContext& root,
                             QHash<QString, CircuitContext>& subContexts) {
    // 同步根上下文的子电路实例引脚
    syncSubPins(root, subContexts);
    // 同步所有子电路上下文的子电路实例引脚
    for (auto it = subContexts.begin(); it != subContexts.end(); ++it)
        syncSubPins(it.value(), subContexts);
    // 根为空则无事可做
    if (root.components.isEmpty()) return;
    // 最多 50 轮整体迭代，直到无变化
    for (int round = 0; round < 50; ++round) {
        if (!evalRecursive(root, subContexts, 0)) break;
    }
}

// 定义"对单个上下文求值"接口：含其引用的子电路，不修改传入的 subContexts
bool IterEngine::evaluateStandalone(
    CircuitContext& ctx,
    const QHash<QString, CircuitContext>& subContexts)
{
    // 直接对该上下文递归求值
    return evalRecursive(ctx, subContexts, 0);
}