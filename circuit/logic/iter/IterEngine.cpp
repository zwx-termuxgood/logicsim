#include "IterEngine.h"
#include "../ComponentLogic.h"
#include "../../core/ValueCodec.h"
#include <QVector>
#include <QDebug>

static bool evalRecursive(CircuitContext& ctx,
                          const QHash<QString, CircuitContext>& subContexts,
                          int depth);

static QString resolveMultiDriverE(const QVector<QString>& vals, int bw) {
    if (bw < 1) bw = 1;
    if (vals.isEmpty()) return QString(bw, '0');

    if (vals.size() == 1) {
        QString s = vals[0];
        if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
        else if (s.length() > bw) s = s.right(bw);
        return s;
    }

    QString result(bw, '0');
    for (int pos = 0; pos < bw; ++pos) {
        bool has0 = false, has1 = false, hasE = false;
        for (const auto& v : vals) {
            QString s = v;
            if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
            else if (s.length() > bw) s = s.right(bw);
            QChar c = (pos < s.length()) ? s[pos] : QChar('0');
            if (c == '0') has0 = true;
            else if (c == '1') has1 = true;
            else if (c == 'E') hasE = true;
        }
        if (hasE)              result[pos] = 'E';
        else if (has0 && has1) result[pos] = 'E';
        else if (has1)         result[pos] = '1';
        else if (has0)         result[pos] = '0';
        else                   result[pos] = 'Z';
    }
    return result;
}

bool IterEngine::evalOne(QVariantMap& c,
                         const QHash<QString, QVector<QPair<int,int>>>& inputMap,
                         const QVector<QVariantMap>& comps) {
    const QString id = c.value("id").toString();
    const QString t  = c.value("type").toString();

    if (t == "text") return false;

    int bw = c.value("bitWidth").toInt();
    if (bw < 1)  bw = 1;
    if (bw > 64) bw = 64;

    const int ic = ComponentLogic::inputCount(c);
    const QVariantList oldInList = c.value("inputPortValues").toList();
    const QString idPrefix = id + ':';

    QVariantList newInList;
    newInList.reserve(ic);
    bool inputChanged = false;

    for (int p = 0; p < ic; ++p) {
        int bwp = ComponentLogic::inputBitWidth(c, p);
        if (bwp < 1)  bwp = 1;
        if (bwp > 64) bwp = 64;

        QString bits;

        auto it = inputMap.constFind(idPrefix + QString::number(p));
        if (it != inputMap.constEnd()) {
            QVector<QString> driverVals;
            for (const auto& pair : it.value()) {
                int fi = pair.first;
                int fp = pair.second;
                if (fi >= 0 && fi < comps.size()) {
                    const QVariantList fo = comps[fi].value("outputPorts").toList();
                    if (fp >= 0 && fp < fo.size()) {
                        QString src = fo[fp].toString();
                        if (src.length() != bwp) src = QString(bwp, 'E');
                        driverVals.append(src);
                    }
                }
            }
            if (!driverVals.isEmpty()) {
                bits = resolveMultiDriverE(driverVals, bwp);
            }
        }
        if (bits.isEmpty()) bits = QString(bwp, '0');

        if (p >= oldInList.size() || oldInList[p].toString() != bits)
            inputChanged = true;

        newInList.append(bits);
    }

    bool changed = false;
    if (inputChanged) {
        c["inputPortValues"] = newInList;
        changed = true;
    }

    QVariantList newOutList;

    if (t == "input" || t == "const") {
        QVariant ov = c.value("outputOverride");
        QString ovStr = ov.isValid() ? ov.toString() : QString();
        if (!ovStr.isEmpty())
            newOutList.append(ovStr);
        else
            newOutList.append(ValueCodec::bitsToString(c.value("inputBits").toList(), bw));
    }
    else if (t == "clock") {
        newOutList = c.value("outputPorts").toList();
        if (newOutList.isEmpty()) newOutList.append(QString(bw, '0'));
    }
    else if (t == "not") {
        QString a = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString out; out.reserve(bw);
        const int alen = a.length();
        for (int b = 0; b < bw; ++b) {
            QChar ch = (b < alen) ? a[b] : QChar('0');
            if (ch == 'Z' || ch == 'E') out += 'E';
            else                        out += (ch == '1') ? '0' : '1';
        }
        newOutList.append(out);
    }
    else if (t == "and" || t == "or" || t == "nand" ||
             t == "nor" || t == "xor" || t == "xnor") {
        QString out; out.reserve(bw);
        const bool isAnd   = (t == "and"   || t == "nand");
        const bool isOr    = (t == "or"    || t == "nor");
        const bool needNot = (t == "nand"  || t == "nor" || t == "xnor");

        QVector<QString> inStrs(ic);
        for (int p = 0; p < ic; ++p)
            inStrs[p] = (p < newInList.size())
                            ? newInList[p].toString()
                            : QString(bw, '0');

        for (int b = 0; b < bw; ++b) {
            bool hasErr = false;
            for (int p = 0; p < ic; ++p) {
                const QString& pv = inStrs[p];
                QChar ch = (b < pv.length()) ? pv[b] : QChar('0');
                if (ch == 'Z' || ch == 'E') { hasErr = true; break; }
            }
            if (hasErr) { out += 'E'; continue; }

            bool r = isAnd ? true : false;
            for (int p = 0; p < ic; ++p) {
                const QString& pv = inStrs[p];
                bool v = (b < pv.length() && pv[b] == '1');
                if (isAnd)      r = r && v;
                else if (isOr)  r = r || v;
                else            r = r != v;
            }
            if (needNot) r = !r;
            out += r ? '1' : '0';
        }
        newOutList.append(out);
    }
    else if (t == "tgate") {
        QString d = newInList.isEmpty()    ? QString(bw, '0') : newInList[0].toString();
        QString e = newInList.size() > 1   ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            QChar ev = (b < e.length()) ? e[b] : QChar('0');
            if (ev == 'Z' || ev == 'E') out += 'E';
            else if (ev == '1')         out += dv;
            else                        out += 'Z';
        }
        newOutList.append(out);
    }
    else if (t == "ntran") {
        QString g = newInList.isEmpty()    ? QString(bw, '0') : newInList[0].toString();
        QString d = newInList.size() > 1   ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar gv = (b < g.length()) ? g[b] : QChar('0');
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            if (gv == 'Z' || gv == 'E') out += 'E';
            else if (gv == '1')         out += dv;
            else                        out += 'Z';
        }
        newOutList.append(out);
    }
    else if (t == "ptran") {
        QString g = newInList.isEmpty()    ? QString(bw, '0') : newInList[0].toString();
        QString d = newInList.size() > 1   ? newInList[1].toString() : QString(bw, '0');
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar gv = (b < g.length()) ? g[b] : QChar('0');
            QChar dv = (b < d.length()) ? d[b] : QChar('0');
            if (gv == 'Z' || gv == 'E') out += 'E';
            else if (gv == '0')         out += dv;
            else                        out += 'Z';
        }
        newOutList.append(out);
    }
    else if (t == "splitter") {
        QString mainIn = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        while (mainIn.length() < bw) mainIn += '0';
        if (mainIn.length() > bw) mainIn = mainIn.left(bw);
        QVariantList splits = c.value("outputSplits").toList();
        int offset = bw;
        for (auto& sv : splits) {
            int len = sv.toInt();
            if (len < 1) len = 1;
            offset -= len;
            if (offset < 0) offset = 0;
            QString out;
            out.reserve(len);
            for (int b = 0; b < len; ++b)
                out += mainIn[offset + b];
            newOutList.append(out);
        }
    }
    else if (t == "hub") {
        QString merged;
        merged.reserve(bw + 8);
        for (int s = newInList.size() - 1; s >= 0; --s) {
            int len = ComponentLogic::inputBitWidth(c, s);
            QString sv = newInList[s].toString();
            while (sv.length() < len) sv += '0';
            merged += sv.left(len);
        }
        while (merged.length() < bw) merged += '0';
        newOutList.append(merged.left(bw));
    }
    else if (t == "led" || t == "output") {
        if (!newInList.isEmpty()) newOutList.append(newInList[0]);
        else                      newOutList.append(QString(bw, '0'));
    }
    else if (t == "sub") {
        newOutList = c.value("outputPorts").toList();
    }

    const QVariantList oldOutList = c.value("outputPorts").toList();
    if (oldOutList != newOutList) {
        c["outputPorts"] = newOutList;
        changed = true;
    }
    return changed;
}

bool IterEngine::evaluateContext(CircuitContext& ctx) {
    const int n = ctx.components.size();
    if (n == 0) return false;

    QHash<QString, int> idToIdx;
    idToIdx.reserve(n * 2);
    for (int i = 0; i < n; ++i)
        idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);

    QVector<QVector<int>> succ(n);
    QVector<int> inDeg(n, 0);

    QHash<QString, QVector<QPair<int,int>>> inputMap;
    inputMap.reserve(ctx.wires.size() * 2);

    for (const auto& wv : ctx.wires) {
        QVariantMap w = wv.toMap();
        int si = idToIdx.value(w.value("fromComp").toString(), -1);
        int di = idToIdx.value(w.value("toComp").toString(), -1);
        if (si < 0 || di < 0) continue;
        succ[si].append(di);
        inDeg[di]++;
        QString key = w.value("toComp").toString() + ':' +
                      QString::number(w.value("toPort").toInt());
        inputMap[key].append({si, w.value("fromPort").toInt()});
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
        for (int v : succ[u])
            if (--inDeg[v] == 0) queue.append(v);
    }

    const bool hasCycle = (order.size() < n);
    bool anyChanged = false;
    QVector<bool> changed(n, false);

    for (int idx : order) {
        if (evalOne(comps[idx], inputMap, comps)) {
            anyChanged = true;
            changed[idx] = true;
        }
    }

    if (hasCycle) {
        QVector<int> cycNodes;
        cycNodes.reserve(n - order.size());
        for (int i = 0; i < n; ++i) if (inDeg[i] > 0) cycNodes.append(i);
        for (int iter = 0; iter < 200; ++iter) {
            bool iterChanged = false;
            for (int idx : cycNodes) {
                if (evalOne(comps[idx], inputMap, comps)) {
                    iterChanged = true;
                    changed[idx] = true;
                }
            }
            if (iterChanged) anyChanged = true;
            else break;
        }
    }

    if (anyChanged) {
        for (int i = 0; i < n; ++i)
            if (changed[i]) ctx.components[i] = comps[i];
    }
    return anyChanged;
}

void IterEngine::syncSubPins(CircuitContext& ctx,
                             const QHash<QString, CircuitContext>& subContexts) {
    for (int i = 0; i < ctx.components.size(); ++i) {
        QVariantMap c = ctx.components[i].toMap();
        if (c.value("type").toString() != "sub") continue;
        QString subId = c.value("subId").toString();
        auto sit = subContexts.find(subId);
        if (sit == subContexts.end()) continue;

        QVariantList subInputNames, subOutputNames, subInputWidths, subOutputWidths;
        for (const auto& scv : sit.value().components) {
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

        bool changed = false;
        if (c.value("subInputNames").toList()   != subInputNames)   { c["subInputNames"]   = subInputNames;   changed = true; }
        if (c.value("subOutputNames").toList()  != subOutputNames)  { c["subOutputNames"]  = subOutputNames;  changed = true; }
        if (c.value("subInputWidths").toList()  != subInputWidths)  { c["subInputWidths"]  = subInputWidths;  changed = true; }
        if (c.value("subOutputWidths").toList() != subOutputWidths) { c["subOutputWidths"] = subOutputWidths; changed = true; }

        QVariantList inVals = c.value("inputPortValues").toList();
        if (inVals.size() != subInputNames.size()) {
            QVariantList newIn;
            newIn.reserve(subInputNames.size());
            for (int k = 0; k < subInputNames.size(); ++k) {
                if (k < inVals.size()) {
                    newIn.append(inVals[k]);
                } else {
                    int w = subInputWidths.value(k).toInt();
                    if (w < 1) w = 1;
                    newIn.append(QString(w, '0'));
                }
            }
            c["inputPortValues"] = newIn;
            changed = true;
        }

        QVariantList outVals = c.value("outputPorts").toList();
        if (outVals.size() != subOutputNames.size()) {
            QVariantList newOut;
            newOut.reserve(subOutputNames.size());
            for (int k = 0; k < subOutputNames.size(); ++k) {
                if (k < outVals.size()) {
                    newOut.append(outVals[k]);
                } else {
                    int w = subOutputWidths.value(k).toInt();
                    if (w < 1) w = 1;
                    newOut.append(QString(w, '0'));
                }
            }
            c["outputPorts"] = newOut;
            changed = true;
        }

        if (changed) ctx.components[i] = c;
    }
}

static bool evalInstance(QVariantMap& subInst,
                         const QHash<QString, CircuitContext>& subContexts,
                         int depth)
{
    if (depth > 32) return false;
    const QString subId = subInst.value("subId").toString();
    auto sit = subContexts.constFind(subId);
    if (sit == subContexts.constEnd()) return false;

    CircuitContext instCtx = sit.value();

    const QVariantList inVals = subInst.value("inputPortValues").toList();
    int idx = 0;
    for (int j = 0; j < instCtx.components.size(); ++j) {
        QVariantMap sc = instCtx.components[j].toMap();
        if (sc.value("type").toString() != "input") continue;
        if (idx >= inVals.size()) break;

        QString s = inVals[idx].toString();
        int sw = sc.value("bitWidth").toInt();
        if (sw < 1) sw = 1;
        if (s.length() != sw) s = QString(sw, '0');

        QVariantList bits;
        bits.reserve(s.length());
        for (int k = 0; k < s.length(); ++k) bits.append(s[k] == '1');
        sc["inputBits"] = bits;

        if (s.contains('Z') || s.contains('E')) sc["outputOverride"] = s;
        else                                    sc["outputOverride"] = QVariant();

        instCtx.components[j] = sc;
        ++idx;
    }

    evalRecursive(instCtx, subContexts, depth + 1);

    QVariantList newOut;
    for (int j = 0; j < instCtx.components.size(); ++j) {
        QVariantMap sc = instCtx.components[j].toMap();
        if (sc.value("type").toString() != "output") continue;
        QVariantList inv = sc.value("inputPortValues").toList();
        QString v = inv.isEmpty() ? QString() : inv[0].toString();
        int sw = sc.value("bitWidth").toInt();
        if (sw < 1) sw = 1;
        if (v.length() != sw) v = QString(sw, '0');
        newOut.append(v);
    }

    if (subInst.value("outputPorts").toList() != newOut) {
        subInst["outputPorts"] = newOut;
        return true;
    }
    return false;
}

static bool evalRecursive(CircuitContext& ctx,
                          const QHash<QString, CircuitContext>& subContexts,
                          int depth)
{
    if (depth > 32) return false;
    bool anyChanged = false;

    for (int iter = 0; iter < 8; ++iter) {
        bool iterChanged = false;

        if (IterEngine::evaluateContext(ctx)) iterChanged = true;

        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (c.value("type").toString() != "sub") continue;
            if (evalInstance(c, subContexts, depth)) {
                ctx.components[i] = c;
                iterChanged = true;
            }
        }

        if (iterChanged) {
            if (IterEngine::evaluateContext(ctx)) iterChanged = true;
        }

        if (iterChanged) anyChanged = true;
        else break;
    }
    return anyChanged;
}

void IterEngine::evaluateAll(CircuitContext& root,
                             QHash<QString, CircuitContext>& subContexts) {
    syncSubPins(root, subContexts);
    for (auto it = subContexts.begin(); it != subContexts.end(); ++it)
        syncSubPins(it.value(), subContexts);

    if (root.components.isEmpty()) return;

    evalRecursive(root, subContexts, 0);
}

bool IterEngine::evaluateStandalone(
    CircuitContext& ctx,
    const QHash<QString, CircuitContext>& subContexts)
{
    return evalRecursive(ctx, subContexts, 0);
}
