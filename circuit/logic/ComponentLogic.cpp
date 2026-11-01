#include "ComponentLogic.h"
// 引入本文件要实现的 ComponentLogic 命名空间声明：实现其函数前必须看到函数原型
#include <QtGlobal>
// 引入 Qt 全局定义：qMin/qMax 等工具函数在此声明

namespace ComponentLogic {
// 打开 ComponentLogic 命名空间，与头文件声明保持一致

// ============================================================
// 基础属性
// ============================================================

// 定义"返回元件输入端口数量"接口
int inputCount(const QVariantMap& comp) {
    // 取类型
    const QString t = comp.value("type").toString();
    // 数据源与文本没有输入端口
    if (t == "input" || t == "clock" || t == "const" || t == "text") return 0;
    // 非门固定 1 个输入
    if (t == "not") return 1;
    // 多输入门：取 inputCount 字段，钳位到 [2,64]
    if (t == "and" || t == "or" || t == "nand" || t == "nor" || t == "xor" || t == "xnor") {
        int n = comp.value("inputCount").toInt();
        return n < 2 ? 2 : qMin(n, 64);
    }
    // 指示灯/输出探针只有 1 个输入
    if (t == "led" || t == "output") return 1;
    // 分线器 1 个输入
    if (t == "splitter") return 1;
    // 汇线器输入数等于分段数，至少 1
    if (t == "hub") return qMax(1, comp.value("outputSplits").toList().size());
    // 子电路实例输入数等于其 input 元件数
    if (t == "sub") return comp.value("subInputNames").toList().size();
    // 晶体管类固定 2 个输入
    if (t == "tgate" || t == "ntran" || t == "ptran") return 2;
    // 未知类型视为 0
    return 0;
}

// 定义"返回元件输出端口数量"接口
int outputCount(const QVariantMap& comp) {
    // 取类型
    const QString t = comp.value("type").toString();
    // 数据源各有 1 个输出
    if (t == "input" || t == "clock" || t == "const") return 1;
    // 文本没有输出
    if (t == "text") return 0;
    // 所有逻辑门都有 1 个输出
    if (t == "not" || t == "and" || t == "or" || t == "nand" ||
        t == "nor" || t == "xor" || t == "xnor") return 1;
    // 分线器输出数等于分段数，至少 1
    if (t == "splitter") return qMax(1, comp.value("outputSplits").toList().size());
    // 汇线器 1 个输出
    if (t == "hub") return 1;
    // 指示灯/输出探针没有输出
    if (t == "led" || t == "output") return 0;
    // 子电路实例输出数等于其 output 元件数
    if (t == "sub") return comp.value("subOutputNames").toList().size();
    // 晶体管类 1 个输出
    if (t == "tgate" || t == "ntran" || t == "ptran") return 1;
    // 未知类型视为 0
    return 0;
}

// 定义"返回某输出端口位宽"接口
int outputBitWidth(const QVariantMap& comp, int portIdx) {
    // 取类型
    const QString t = comp.value("type").toString();
    // 取元件位宽，下限钳到 1
    int bw = comp.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 这些类型输出位宽就是元件位宽
    if (t == "input" || t == "const" || t == "clock" || t == "not" ||
        t == "and" || t == "or" || t == "nand" || t == "nor" ||
        t == "xor" || t == "xnor") return bw;
    // 晶体管类输出位宽就是元件位宽
    if (t == "tgate" || t == "ntran" || t == "ptran") return bw;
    // 分线器：按端口索引取对应分段位宽
    if (t == "splitter") {
        QVariantList splits = comp.value("outputSplits").toList();
        if (portIdx >= 0 && portIdx < splits.size()) {
            int v = splits[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
        return 1;
    }
    // 汇线器输出位宽就是元件位宽
    if (t == "hub") return bw;
    // 子电路实例：按端口索引取 subOutputWidths
    if (t == "sub") {
        QVariantList ws = comp.value("subOutputWidths").toList();
        if (portIdx >= 0 && portIdx < ws.size()) {
            int v = ws[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
    }
    // 其它类型回退元件位宽
    return bw;
}

// 定义"返回某输入端口位宽"接口
int inputBitWidth(const QVariantMap& comp, int portIdx) {
    // 取类型
    const QString t = comp.value("type").toString();
    // 取元件位宽，下限钳到 1
    int bw = comp.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 分线器输入位宽就是元件位宽
    if (t == "splitter") return bw;
    // 汇线器：按端口索引取对应分段位宽
    if (t == "hub") {
        QVariantList splits = comp.value("outputSplits").toList();
        if (portIdx >= 0 && portIdx < splits.size()) {
            int v = splits[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
        return 1;
    }
    // 子电路实例：按端口索引取 subInputWidths
    if (t == "sub") {
        QVariantList ws = comp.value("subInputWidths").toList();
        if (portIdx >= 0 && portIdx < ws.size()) {
            int v = ws[portIdx].toInt();
            return v < 1 ? 1 : v;
        }
    }
    // 其它类型回退元件位宽
    return bw;
}

// ============================================================
// 端口几何
// ============================================================

// 定义"按旋转角旋转点"静态辅助函数
static void rotatePoint(double& x, double& y, int rot) {
    // 归一化到 0..3
    rot = ((rot % 4) + 4) % 4;
    // 保存原坐标
    double nx = x, ny = y;
    // 按 90° 增量旋转
    switch (rot) {
    case 1: nx = -y; ny =  x; break;
    case 2: nx = -x; ny = -y; break;
    case 3: nx =  y; ny = -x; break;
    default: break;
    }
    // 写回旋转后坐标
    x = nx; y = ny;
}

// 定义"计算所有输出端口几何"接口
QVariantList outputPorts(const QVariantMap& comp) {
    // 准备结果列表
    QVariantList result;
    // 取类型
    const QString t = comp.value("type").toString();
    // 输出端口数量
    int n = outputCount(comp);
    // 旋转角
    int rot = comp.value("rotation").toInt();

    // 默认元件宽高
    double W = 80.0, H = 60.0;

    // 数据源：右侧中间一个输出端口
    if (t == "input" || t == "clock" || t == "const") {
        // 宽度随位宽增长
        W = qMax(60.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        // 高度固定
        H = 40.0;
        // 准备端口
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
        // 分线器：右侧多行输出，行数等于分段数
    } else if (t == "splitter") {
        W = 80.0;
        QVariantList splits = comp.value("outputSplits").toList();
        // 分段数，至少 1
        int segN = qMax(1, splits.size());
        // 高度按分段数估算
        H = qMax(60.0, (double)segN * 24.0);
        // 逐分段生成端口
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            // 纵向均匀分布
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = outputBitWidth(comp, i);
            result.append(p);
        }
        // 汇线器：右侧中间一个输出端口
    } else if (t == "hub") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
        // NMOS：输出在右上（漏极）
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = -H/4;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
        // PMOS：输出在右下（漏极）
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = H/4;
        p["bitWidth"] = outputBitWidth(comp, 0);
        result.append(p);
        // 门电路/子电路/传输门：右侧输出，按端口数纵向分布
    } else if (t == "not" || t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub" ||
               t == "tgate") {
        // 子电路宽一些
        W = (t == "sub") ? 110.0 : 80.0;
        // 输入端口数
        int ic = inputCount(comp);
        // 输出端口数，至少 1
        int oc = n; if (oc < 1) oc = 1;
        // 高度按输入/输出端口数较大者估算
        H = qMax(60.0, (double)qMax(ic, oc) * 24.0);
        // 逐输出端口生成
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            // 单输出居中，多输出纵向均匀分布
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = outputBitWidth(comp, i);
            result.append(p);
        }
    }

    // 对所有端口应用旋转
    for (int i = 0; i < result.size(); ++i) {
        QVariantMap p = result[i].toMap();
        double px = p.value("x").toDouble();
        double py = p.value("y").toDouble();
        rotatePoint(px, py, rot);
        p["x"] = px; p["y"] = py;
        result[i] = p;
    }
    // 返回端口几何列表
    return result;
}

// 定义"计算所有输入端口几何"接口
QVariantList inputPorts(const QVariantMap& comp) {
    // 准备结果列表
    QVariantList result;
    // 取类型
    const QString t = comp.value("type").toString();
    // 输入端口数量
    int n = inputCount(comp);
    // 旋转角
    int rot = comp.value("rotation").toInt();

    // 默认元件宽高
    double W = 80.0, H = 60.0;

    // 指示灯/输出探针：左侧中间一个输入端口
    if (t == "led" || t == "output") {
        // 宽度随位宽增长
        W = qMax(t == "output" ? 60.0 : 50.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        // 高度固定
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
        // 分线器：左侧中间一个输入端口
    } else if (t == "splitter") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
        // 汇线器：左侧多行输入，行数等于分段数
    } else if (t == "hub") {
        W = 80.0;
        QVariantList splits = comp.value("outputSplits").toList();
        // 分段数，至少 1
        int segN = qMax(1, splits.size());
        // 高度按分段数估算
        H = qMax(60.0, (double)segN * 24.0);
        // 逐分段生成端口
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            // 纵向均匀分布
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = inputBitWidth(comp, i);
            result.append(p);
        }
        // 非门：左侧中间一个输入端口
    } else if (t == "not") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputBitWidth(comp, 0);
        result.append(p);
        // NMOS：栅极在左中，漏极在右下（作为第二输入用于晶体管端口布局）
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        // 第一个输入：栅极
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = inputBitWidth(comp, 0);
        result.append(pg);

        // 第二个输入：漏极（右侧）
        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = H/4;
        pd["bitWidth"] = inputBitWidth(comp, 1);
        result.append(pd);
        // PMOS：栅极在左中，漏极在右上
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        // 第一个输入：栅极
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = inputBitWidth(comp, 0);
        result.append(pg);

        // 第二个输入：漏极（右侧）
        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = -H/4;
        pd["bitWidth"] = inputBitWidth(comp, 1);
        result.append(pd);
        // 门电路/子电路/传输门：左侧输入，按端口数纵向分布
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub" ||
               t == "tgate") {
        // 子电路宽一些
        W = (t == "sub") ? 110.0 : 80.0;
        // 高度按输入端口数估算
        H = qMax(60.0, (double)n * 24.0);
        // 逐输入端口生成
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            // 单输入居中，多输入纵向均匀分布
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = inputBitWidth(comp, i);
            result.append(p);
        }
    }

    // 对所有端口应用旋转
    for (int i = 0; i < result.size(); ++i) {
        QVariantMap p = result[i].toMap();
        double px = p.value("x").toDouble();
        double py = p.value("y").toDouble();
        rotatePoint(px, py, rot);
        p["x"] = px; p["y"] = py;
        result[i] = p;
    }
    // 返回端口几何列表
    return result;
}

// ============================================================
// 组合逻辑输出计算（两个引擎共用）
// ============================================================
// 定义"计算组合逻辑输出"接口
QVector<QString> computeCombinationalOutputs(const QString& type,
                                             const QVector<QString>& inputs,
                                             int bitWidth,
                                             const QVariantList& outputSplits)
{
    // 准备输出列表
    QVector<QString> outs;
    // 位宽下限钳到 1
    int bw = bitWidth < 1 ? 1 : bitWidth;

    // 内部 lambda：安全读取某串第 b 位，越界按 '0'
    auto bitAt = [](const QString& s, int b) -> QChar {
        return (b < s.length()) ? s[b] : QChar('0');
    };
    // 内部 lambda：安全取第 i 个输入串，缺失返回全 0 串
    auto inStr = [&](int i) -> QString {
        if (i < inputs.size()) return inputs[i];
        return QString(bw, '0');
    };

    // ---- NOT ----
    if (type == "not") {
        // 取第一个输入
        const QString a = inStr(0);
        // 准备输出串
        QString out; out.reserve(bw);
        // 逐位取反，Z/E 映射为 E
        for (int b = 0; b < bw; ++b) {
            QChar c = bitAt(a, b);
            if (c == 'Z' || c == 'E') out += 'E';
            else                       out += (c == '1') ? '0' : '1';
        }
        // 追加输出并返回
        outs.append(out);
        return outs;
    }

    // ---- AND / OR / NAND / NOR / XOR / XNOR ----
    if (type == "and" || type == "or" || type == "nand" ||
        type == "nor" || type == "xor" || type == "xnor") {
        // 输入端口数
        const int  ic      = inputs.size();
        // 是否与类（and/nand）
        const bool isAnd   = (type == "and"  || type == "nand");
        // 是否或类（or/nor）
        const bool isOr    = (type == "or"   || type == "nor");
        // 是否需要在规约后取反（nand/nor/xnor）
        const bool needNot = (type == "nand" || type == "nor" || type == "xnor");

        // 准备输出串
        QString out; out.reserve(bw);
        // 逐位计算
        for (int b = 0; b < bw; ++b) {
            // 该位是否有任一输入为 Z/E
            bool hasErr = false;
            for (int p = 0; p < ic; ++p) {
                QChar c = bitAt(inputs[p], b);
                if (c == 'Z' || c == 'E') { hasErr = true; break; }
            }
            // 有错误态则该位输出 E
            if (hasErr) { out += 'E'; continue; }

            // 规约初值：与类为 true，或/异或类为 false
            bool r = isAnd ? true : false;
            // 逐输入规约
            for (int p = 0; p < ic; ++p) {
                bool v = (b < inputs[p].length() && inputs[p][b] == '1');
                if (isAnd)      r = r && v;
                else if (isOr)  r = r || v;
                else            r = r != v;
            }
            // 需要取反则取反
            if (needNot) r = !r;
            // 追加该位结果
            out += r ? '1' : '0';
        }
        // 追加输出并返回
        outs.append(out);
        return outs;
    }

    // ---- TGATE / NTRAN / PTRAN ----
    if (type == "tgate" || type == "ntran" || type == "ptran") {
        // 第一个输入
        const QString in0 = inStr(0);
        // 第二个输入
        const QString in1 = inStr(1);
        // 是否传输门
        const bool isT = (type == "tgate");
        // 是否 NMOS
        const bool isN = (type == "ntran");
        // else: ptran

        // 准备输出串
        QString out; out.reserve(bw);
        // 逐位计算三态输出
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
        // 追加输出并返回
        outs.append(out);
        return outs;
    }

    // ---- SPLITTER ----
    if (type == "splitter") {
        // 主输入
        QString mainIn = inStr(0);
        // 长度不足：右侧补 0
        while (mainIn.length() < bw) mainIn += '0';
        // 长度超出：只保留高位部分
        if (mainIn.length() > bw) mainIn = mainIn.left(bw);

        // 从最高位开始按分段切分
        int offset = bw;
        // 逐段生成输出
        for (auto& sv : outputSplits) {
            // 该段位宽，至少 1
            int len = sv.toInt();
            if (len < 1) len = 1;
            // 该段起始偏移
            offset -= len;
            if (offset < 0) offset = 0;
            // 准备该段输出
            QString out; out.reserve(len);
            // 逐位复制，越界补 0
            for (int b = 0; b < len; ++b)
                out += (offset + b < mainIn.length()) ? mainIn[offset + b] : QChar('0');
            // 追加该段
            outs.append(out);
        }
        // 返回各段输出
        return outs;
    }

    // ---- HUB ----
    if (type == "hub") {
        // 准备拼接结果
        QString merged; merged.reserve(bw + 8);
        // 从最后一个输入开始向前拼接，保证低位在前
        for (int s = inputs.size() - 1; s >= 0; --s) {
            // 该段位宽，至少 1
            int len = (s < outputSplits.size()) ? outputSplits[s].toInt() : 1;
            if (len < 1) len = 1;
            // 取该输入
            QString sv = inputs[s];
            // 长度不足：右侧补 0
            while (sv.length() < len) sv += '0';
            // 长度超出：只保留高位部分
            if (sv.length() > len) sv = sv.left(len);
            // 拼接到结果
            merged += sv;
        }
        // 长度不足：右侧补 0
        while (merged.length() < bw) merged += '0';
        // 长度超出：只保留高位部分
        if (merged.length() > bw) merged = merged.left(bw);
        // 追加合并输出
        outs.append(merged);
        return outs;
    }

    // 未识别类型（如 sub）由调用者处理，返回空列表
    return outs;
}

} // namespace ComponentLogic
// 关闭 ComponentLogic 命名空间