#include "ComponentLogic.h"
#include <QtGlobal>

namespace ComponentLogic {

// ============================================================
// 基础属性
// ============================================================

int inputCount(const QVariantMap& comp) {
    const QString t = comp.value("type").toString();
    if (t == "input" || t == "clock" || t == "const" || t == "text") return 0;
    if (t == "not") return 1;
    if (t == "and" || t == "or" || t == "nand" || t == "nor" || t == "xor" || t == "xnor") {
        int n = comp.value("inputCount").toInt();
        return n < 2 ? 2 : qMin(n, 64);
    }
    if (t == "led" || t == "output") return 1;
    if (t == "splitter") return 1;
    if (t == "hub") return qMax(1, comp.value("outputSplits").toList().size());
    if (t == "sub") return comp.value("subInputNames").toList().size();
    if (t == "tgate" || t == "ntran" || t == "ptran") return 2;
    return 0;
}

int outputCount(const QVariantMap& comp) {
    const QString t = comp.value("type").toString();
    if (t == "input" || t == "clock" || t == "const") return 1;
    if (t == "text") return 0;
    if (t == "not" || t == "and" || t == "or" || t == "nand" ||
        t == "nor" || t == "xor" || t == "xnor") return 1;
    if (t == "splitter") return qMax(1, comp.value("outputSplits").toList().size());
    if (t == "hub") return 1;
    if (t == "led" || t == "output") return 0;
    if (t == "sub") return comp.value("subOutputNames").toList().size();
    if (t == "tgate" || t == "ntran" || t == "ptran") return 1;
    return 0;
}

int outputBitWidth(const QVariantMap& comp, int portIdx) {
    const QString t = comp.value("type").toString();
    int bw = comp.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (t == "input" || t == "const" || t == "clock" || t == "not" ||
        t == "and" || t == "or" || t == "nand" || t == "nor" ||
        t == "xor" || t == "xnor") return bw;
    if (t == "tgate" || t == "ntran" || t == "ptran") return bw;
    if (t == "splitter") {
        QVariantList splits = comp.value("outputSplits").toList();
        if (portIdx >= 0 && portIdx < splits.size()) {
            int v = splits[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
        return 1;
    }
    if (t == "hub") return bw;
    if (t == "sub") {
        QVariantList ws = comp.value("subOutputWidths").toList();
        if (portIdx >= 0 && portIdx < ws.size()) {
            int v = ws[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
    }
    return bw;
}

int inputBitWidth(const QVariantMap& comp, int portIdx) {
    const QString t = comp.value("type").toString();
    int bw = comp.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (t == "splitter") return bw;
    if (t == "hub") {
        QVariantList splits = comp.value("outputSplits").toList();
        if (portIdx >= 0 && portIdx < splits.size()) {
            int v = splits[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
        return 1;
    }
    if (t == "sub") {
        QVariantList ws = comp.value("subInputWidths").toList();
        if (portIdx >= 0 && portIdx < ws.size()) {
            int v = ws[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
    }
    return bw;
}

// ============================================================
// 端口几何
// ============================================================

static void rotatePoint(double& x, double& y, int rot) {
    rot = ((rot % 4) + 4) % 4;
    double nx = x, ny = y;
    switch (rot) {
    case 1: nx = -y; ny =  x; break;
    case 2: nx = -x; ny = -y; break;
    case 3: nx =  y; ny = -x; break;
    default: break;
    }
    x = nx; y = ny;
}

QVariantList outputPorts(const QVariantMap& comp) {
    QVariantList result;
    const QString t = comp.value("type").toString();
    int n = outputCount(comp);
    int rot = comp.value("rotation").toInt();

    double W = 80.0, H = 60.0;

    if (t == "input" || t == "clock" || t == "const") {
        W = qMax(60.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "splitter") {
        W = 80.0;
        QVariantList splits = comp.value("outputSplits").toList();
        int segN = qMax(1, splits.size());
        H = qMax(60.0, (double)segN * 24.0);
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = outputBitWidth(comp, i);
            result.append(p);
        }
    } else if (t == "hub") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = -H/4;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = H/4;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "not" || t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub" ||
               t == "tgate") {
        W = (t == "sub") ? 110.0 : 80.0;
        int ic = inputCount(comp);
        int oc = n; if (oc < 1) oc = 1;
        H = qMax(60.0, (double)qMax(ic, oc) * 24.0);
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = outputBitWidth(comp, i);
            result.append(p);
        }
    }

    for (int i = 0; i < result.size(); ++i) {
        QVariantMap p = result[i].toMap();
        double px = p.value("x").toDouble();
        double py = p.value("y").toDouble();
        rotatePoint(px, py, rot);
        p["x"] = px; p["y"] = py;
        result[i] = p;
    }
    return result;
}

QVariantList inputPorts(const QVariantMap& comp) {
    QVariantList result;
    const QString t = comp.value("type").toString();
    int n = inputCount(comp);
    int rot = comp.value("rotation").toInt();

    double W = 80.0, H = 60.0;

    if (t == "led" || t == "output") {
        W = qMax(t == "output" ? 60.0 : 50.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "splitter") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "hub") {
        W = 80.0;
        QVariantList splits = comp.value("outputSplits").toList();
        int segN = qMax(1, splits.size());
        H = qMax(60.0, (double)segN * 24.0);
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = inputBitWidth(comp, i);
            result.append(p);
        }
    } else if (t == "not") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = inputBitWidth(comp, 0);
        result.append(pg);

        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = H/4;
        pd["bitWidth"] = inputBitWidth(comp, 1);
        result.append(pd);
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = inputBitWidth(comp, 0);
        result.append(pg);

        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = -H/4;
        pd["bitWidth"] = inputBitWidth(comp, 1);
        result.append(pd);
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub" ||
               t == "tgate") {
        W = (t == "sub") ? 110.0 : 80.0;
        H = qMax(60.0, (double)n * 24.0);
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = inputBitWidth(comp, i);
            result.append(p);
        }
    }

    for (int i = 0; i < result.size(); ++i) {
        QVariantMap p = result[i].toMap();
        double px = p.value("x").toDouble();
        double py = p.value("y").toDouble();
        rotatePoint(px, py, rot);
        p["x"] = px; p["y"] = py;
        result[i] = p;
    }
    return result;
}

// ============================================================
// 组合逻辑输出计算（两个引擎共用）
// ============================================================
QVector<QString> computeCombinationalOutputs(const QString& type,
                                             const QVector<QString>& inputs,
                                             int bitWidth,
                                             const QVariantList& outputSplits)
{
    QVector<QString> outs;
    int bw = bitWidth < 1 ? 1 : bitWidth;

    auto bitAt = [](const QString& s, int b) -> QChar {
        return (b < s.length()) ? s[b] : QChar('0');
    };
    auto inStr = [&](int i) -> QString {
        if (i < inputs.size()) return inputs[i];
        return QString(bw, '0');
    };

    // ---- NOT ----
    if (type == "not") {
        const QString a = inStr(0);
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar c = bitAt(a, b);
            if (c == 'Z' || c == 'E') out += 'E';
            else                       out += (c == '1') ? '0' : '1';
        }
        outs.append(out);
        return outs;
    }

    // ---- AND / OR / NAND / NOR / XOR / XNOR ----
    if (type == "and" || type == "or" || type == "nand" ||
        type == "nor" || type == "xor" || type == "xnor") {
        const int  ic      = inputs.size();
        const bool isAnd   = (type == "and"  || type == "nand");
        const bool isOr    = (type == "or"   || type == "nor");
        const bool needNot = (type == "nand" || type == "nor" || type == "xnor");

        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool hasErr = false;
            for (int p = 0; p < ic; ++p) {
                QChar c = bitAt(inputs[p], b);
                if (c == 'Z' || c == 'E') { hasErr = true; break; }
            }
            if (hasErr) { out += 'E'; continue; }

            bool r = isAnd ? true : false;
            for (int p = 0; p < ic; ++p) {
                bool v = (b < inputs[p].length() && inputs[p][b] == '1');
                if (isAnd)      r = r && v;
                else if (isOr)  r = r || v;
                else            r = r != v;
            }
            if (needNot) r = !r;
            out += r ? '1' : '0';
        }
        outs.append(out);
        return outs;
    }

    // ---- TGATE / NTRAN / PTRAN ----
    if (type == "tgate" || type == "ntran" || type == "ptran") {
        const QString in0 = inStr(0);
        const QString in1 = inStr(1);
        const bool isT = (type == "tgate");
        const bool isN = (type == "ntran");
        // else: ptran

        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            QChar v0 = bitAt(in0, b);
            QChar v1 = bitAt(in1, b);
            if (isT) {
                // tgate：in0=data，in1=enable；enable=1 时输出 data，否则 Z
                if (v1 == 'Z' || v1 == 'E') out += 'E';
                else if (v1 == '1')         out += v0;
                else                        out += 'Z';
            } else if (isN) {
                // ntran：in0=gate，in1=data；gate=1 时导通
                if (v0 == 'Z' || v0 == 'E') out += 'E';
                else if (v0 == '1')         out += v1;
                else                        out += 'Z';
            } else {
                // ptran：in0=gate，in1=data；gate=0 时导通
                if (v0 == 'Z' || v0 == 'E') out += 'E';
                else if (v0 == '0')         out += v1;
                else                        out += 'Z';
            }
        }
        outs.append(out);
        return outs;
    }

    // ---- SPLITTER ----
    if (type == "splitter") {
        QString mainIn = inStr(0);
        while (mainIn.length() < bw) mainIn += '0';
        if (mainIn.length() > bw) mainIn = mainIn.left(bw);

        int offset = bw;
        for (auto& sv : outputSplits) {
            int len = sv.toInt();
            if (len < 1) len = 1;
            offset -= len;
            if (offset < 0) offset = 0;
            QString out; out.reserve(len);
            for (int b = 0; b < len; ++b)
                out += (offset + b < mainIn.length()) ? mainIn[offset + b] : QChar('0');
            outs.append(out);
        }
        return outs;
    }

    // ---- HUB ----
    if (type == "hub") {
        QString merged; merged.reserve(bw + 8);
        for (int s = inputs.size() - 1; s >= 0; --s) {
            int len = (s < outputSplits.size()) ? outputSplits[s].toInt() : 1;
            if (len < 1) len = 1;
            QString sv = inputs[s];
            while (sv.length() < len) sv += '0';
            if (sv.length() > len) sv = sv.left(len);
            merged += sv;
        }
        while (merged.length() < bw) merged += '0';
        if (merged.length() > bw) merged = merged.left(bw);
        outs.append(merged);
        return outs;
    }

    // 未识别类型（如 sub）由调用者处理
    return outs;
}

} // namespace ComponentLogic