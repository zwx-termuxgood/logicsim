#include "CircuitEvaluator.h"
#include "ComponentTraits.h"
#include "ValueCodec.h"
#include <QVector>
#include <QDebug>

bool CircuitEvaluator::evalOne(QVariantMap& c,
                               const QHash<QString, QPair<int,int>>& inputMap,
                               const QVector<QVariantMap>& comps) {
    const QString id = c.value("id").toString();
    const QString t = c.value("type").toString();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;

    if (t == "text") return false;

    int ic = ComponentTraits::inputCount(c);
    QVariantList newInList;
    newInList.reserve(ic);
    for (int p = 0; p < ic; ++p) {
        int bwp = ComponentTraits::inputBitWidth(c, p);
        QString bits(bwp, '0');
        auto it = inputMap.constFind(id + ':' + QString::number(p));
        if (it != inputMap.constEnd()) {
            int fi = it.value().first;
            int fp = it.value().second;
            const QVariantList fo = comps[fi].value("outputPorts").toList();
            if (fp >= 0 && fp < fo.size()) {
                const QString srcStr = fo[fp].toString();
                for (int b = 0; b < bwp; ++b)
                    bits[b] = (b < srcStr.length() && srcStr[b] == '1') ? '1' : '0';
            }
        }
        newInList.append(bits);
    }

    bool changed = false;
    if (c.value("inputPortValues").toList() != newInList) {
        c["inputPortValues"] = newInList;
        changed = true;
    }

    QVariantList newOutList;
    if (t == "input") {
        newOutList.append(ValueCodec::bitsToString(c.value("inputBits").toList(), bw));
    } else if (t == "clock") {
        newOutList = c.value("outputPorts").toList();
        if (newOutList.isEmpty()) newOutList.append(QString(bw, '0'));
    } else if (t == "not") {
        QString a = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) out += ValueCodec::bitAt(a, b) ? '0' : '1';
        newOutList.append(out);
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor") {
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool r = (t == "and" || t == "nand") ? true : false;
            for (int p = 0; p < ic; ++p) {
                QString pv = (p < newInList.size()) ? newInList[p].toString() : QString(bw, '0');
                bool v = ValueCodec::bitAt(pv, b);
                if (t == "and" || t == "nand") r = r && v;
                else if (t == "or" || t == "nor") r = r || v;
                else if (t == "xor" || t == "xnor") r = r != v;
            }
            if (t == "nand" || t == "nor" || t == "xnor") r = !r;
            out += r ? '1' : '0';
        }
        newOutList.append(out);
    } else if (t == "tgate") {
        QString d = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString e = newInList.size() > 1 ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool dv = ValueCodec::bitAt(d, b);
            bool ev = ValueCodec::bitAt(e, b);
            // 使能有效时输出 D，无效时输出高阻 Z
            if (ev) out += dv ? '1' : '0';
            else    out += 'Z';
        }
        newOutList.append(out);
    } else if (t == "ntran") {
        // NMOS：G=1 导通 D→S，G=0 高阻 Z
        // 【修改】端口 0 = G（左侧栅极），端口 1 = D（右侧漏极）
        QString g = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString d = newInList.size() > 1 ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool dv = ValueCodec::bitAt(d, b);
            bool gv = ValueCodec::bitAt(g, b);
            if (gv) out += dv ? '1' : '0';
            else    out += 'Z';
        }
        newOutList.append(out);
    } else if (t == "ptran") {
        // PMOS：G=0 导通 D→S，G=1 高阻 Z
        // 【修改】端口 0 = G（左侧栅极），端口 1 = D（右侧漏极）
        QString g = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString d = newInList.size() > 1 ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool dv = ValueCodec::bitAt(d, b);
            bool gv = ValueCodec::bitAt(g, b);
            if (!gv) out += dv ? '1' : '0';
            else     out += 'Z';
        }
        newOutList.append(out);
    } else if (t == "splitter") {
        QString mainIn = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        while (mainIn.length() < bw) mainIn += '0';
        mainIn = mainIn.left(bw);
        QVariantList splits = c.value("outputSplits").toList();
        int offset = bw;
        for (auto& sv : splits) {
            int len = sv.toInt();
            if (len < 1) len = 1;
            offset -= len;
            QString out;
            for (int b = 0; b < len; ++b)
                out += ValueCodec::bitAt(mainIn, offset + b) ? '1' : '0';
            newOutList.append(out);
        }
    } else if (t == "hub") {
        QString merged;
        for (int s = newInList.size() - 1; s >= 0; --s) {
            int len = ComponentTraits::inputBitWidth(c, s);
            QString sv = newInList[s].toString();
            while (sv.length() < len) sv += '0';
            merged += sv.left(len);
        }
        while (merged.length() < bw) merged += '0';
        newOutList.append(merged.left(bw));
    } else if (t == "led" || t == "output") {
        if (!newInList.isEmpty()) newOutList.append(newInList[0]);
        else newOutList.append(QString(bw, '0'));
    } else if (t == "sub") {
        newOutList = c.value("outputPorts").toList();
    }

    if (c.value("outputPorts").toList() != newOutList) {
        c["outputPorts"] = newOutList;
        changed = true;
    }
    return changed;
}

void CircuitEvaluator::evaluateContext(CircuitContext& ctx) {
    const int n = ctx.components.size();
    if (n == 0) return;

    QHash<QString, int> idToIdx;
    idToIdx.reserve(n * 2);
    for (int i = 0; i < n; ++i)
        idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);

    QVector<QVector<int>> succ(n);
    QVector<int> inDeg(n, 0);
    QHash<QString, QPair<int,int>> inputMap;
    inputMap.reserve(ctx.wires.size() * 2);

    for (const auto& wv : ctx.wires) {
        QVariantMap w = wv.toMap();
        int si = idToIdx.value(w.value("fromComp").toString(), -1);
        int di = idToIdx.value(w.value("toComp").toString(), -1);
        if (si < 0 || di < 0) continue;
        succ[si].append(di);
        inDeg[di]++;
        inputMap.insert(w.value("toComp").toString() + ':' +
                            QString::number(w.value("toPort").toInt()),
                        {si, w.value("fromPort").toInt()});
    }

    QVector<QVariantMap> comps(n);
    for (int i = 0; i < n; ++i) comps[i] = ctx.components[i].toMap();

    QVector<int> order;
    order.reserve(n);
    QVector<int> queue;
    queue.reserve(n);
    for (int i = 0; i < n; ++i) if (inDeg[i] == 0) queue.append(i);
    int qi = 0;
    while (qi < queue.size()) {
        int u = queue[qi++];
        order.append(u);
        for (int v : succ[u]) {
            if (--inDeg[v] == 0) queue.append(v);
        }
    }

    bool hasCycle = (order.size() < n);

    for (int idx : order) {
        evalOne(comps[idx], inputMap, comps);
    }

    if (hasCycle) {
        QVector<int> cycNodes;
        for (int i = 0; i < n; ++i) if (inDeg[i] > 0) cycNodes.append(i);
        // 【提示】晶体管/锁存器类结构需要更多迭代收敛；提高到 200 次。
        for (int iter = 0; iter < 200; ++iter) {
            bool anyChanged = false;
            for (int idx : cycNodes) {
                if (evalOne(comps[idx], inputMap, comps)) anyChanged = true;
            }
            if (!anyChanged) break;
        }
    }

    for (int i = 0; i < n; ++i) {
        ctx.components[i] = comps[i];
    }
}

void CircuitEvaluator::syncSubPins(CircuitContext& ctx,
                                   const QHash<QString, CircuitContext>& subContexts) {
    for (int i = 0; i < ctx.components.size(); ++i) {
        QVariantMap c = ctx.components[i].toMap();
        if (c.value("type").toString() != "sub") continue;
        QString subId = c.value("subId").toString();
        auto sit = subContexts.find(subId);
        if (sit == subContexts.end()) continue;

        QVariantList subInputNames, subOutputNames, subInputWidths, subOutputWidths;
        for (auto& scv : sit.value().components) {
            auto sc = scv.toMap();
            QString st = sc.value("type").toString();
            QString sn = sc.value("name").toString();
            int sw = sc.value("bitWidth").toInt();
            if (sw < 1) sw = 1;
            if (st == "input") {
                subInputNames.append(sn.isEmpty() ? sc.value("id").toString() : sn);
                subInputWidths.append(sw);
            } else if (st == "output") {
                subOutputNames.append(sn.isEmpty() ? sc.value("id").toString() : sn);
                subOutputWidths.append(sw);
            }
        }

        QVariantList inVals = c.value("inputPortValues").toList();
        while (inVals.size() < subInputNames.size()) {
            int idx2 = inVals.size();
            int w = subInputWidths.value(idx2).toInt();
            if (w < 1) w = 1;
            inVals.append(QString(w, '0'));
        }
        while (inVals.size() > subInputNames.size()) inVals.removeLast();

        QVariantList outVals = c.value("outputPorts").toList();
        while (outVals.size() < subOutputNames.size()) {
            int idx2 = outVals.size();
            int w = subOutputWidths.value(idx2).toInt();
            if (w < 1) w = 1;
            outVals.append(QString(w, '0'));
        }
        while (outVals.size() > subOutputNames.size()) outVals.removeLast();

        c["subInputNames"] = subInputNames;
        c["subOutputNames"] = subOutputNames;
        c["subInputWidths"] = subInputWidths;
        c["subOutputWidths"] = subOutputWidths;
        c["inputPortValues"] = inVals;
        c["outputPorts"] = outVals;
        ctx.components[i] = c;
    }
}

void CircuitEvaluator::evaluateAll(CircuitContext& root,
                                   QHash<QString, CircuitContext>& subContexts) {
    syncSubPins(root, subContexts);
    for (auto it = subContexts.begin(); it != subContexts.end(); ++it)
        syncSubPins(it.value(), subContexts);

    if (root.components.isEmpty() && subContexts.isEmpty()) return;

    for (int outer = 0; outer < 20; ++outer) {
        bool anyChanged = false;

        for (auto it = subContexts.begin(); it != subContexts.end(); ++it)
            evaluateContext(it.value());
        evaluateContext(root);

        for (int i = 0; i < root.components.size(); ++i) {
            QVariantMap c = root.components[i].toMap();
            if (c.value("type").toString() != "sub") continue;
            QString subId = c.value("subId").toString();
            auto sit = subContexts.find(subId);
            if (sit == subContexts.end()) continue;

            QVariantList inValues = c.value("inputPortValues").toList();
            int idx = 0;
            for (int j = 0; j < sit.value().components.size(); ++j) {
                QVariantMap sc = sit.value().components[j].toMap();
                if (sc.value("type").toString() != "input") continue;
                if (idx < inValues.size()) {
                    QString s = inValues[idx].toString();
                    QVariantList bits;
                    for (int k = 0; k < s.length(); ++k) bits.append(s[k] == '1');
                    sc["inputBits"] = bits;
                    sit.value().components[j] = sc;
                }
                idx++;
            }

            QVariantList newOut;
            for (int j = 0; j < sit.value().components.size(); ++j) {
                QVariantMap sc = sit.value().components[j].toMap();
                if (sc.value("type").toString() != "output") continue;
                QVariantList inVals = sc.value("inputPortValues").toList();
                QString v = inVals.isEmpty() ? "" : inVals[0].toString();
                int sw = sc.value("bitWidth").toInt();
                if (sw < 1) sw = 1;
                if (v.length() != sw) v = QString(sw, '0');
                newOut.append(v);
            }
            if (c.value("outputPorts").toList() != newOut) {
                c["outputPorts"] = newOut;
                root.components[i] = c;
                anyChanged = true;
            }
        }

        if (!anyChanged && outer > 0) break;
    }
}