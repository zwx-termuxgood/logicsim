#include "circuit.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QDateTime>
#include <QStringList>
#include <QSet>
#include <QQueue>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>

Circuit::Circuit(QObject* parent) : QObject(parent) {
    loadRecentFiles();
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &Circuit::tick);
}

Circuit::Context& Circuit::currentCtx() {
    if (m_currentCtxId.isEmpty()) return m_root;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return m_root;
    return it.value();
}
const Circuit::Context& Circuit::currentCtx() const {
    if (m_currentCtxId.isEmpty()) return m_root;
    auto it = m_subContexts.find(m_currentCtxId);
    if (it == m_subContexts.end()) return m_root;
    return it.value();
}
Circuit::Context* Circuit::ctxById(const QString& id) {
    if (id.isEmpty()) return &m_root;
    auto it = m_subContexts.find(id);
    if (it == m_subContexts.end()) return nullptr;
    return &it.value();
}
const Circuit::Context* Circuit::ctxById(const QString& id) const {
    if (id.isEmpty()) return &m_root;
    auto it = m_subContexts.find(id);
    if (it == m_subContexts.end()) return nullptr;
    return &it.value();
}

QVariantList Circuit::components() const { return currentCtx().components; }
QVariantList Circuit::wires() const { return currentCtx().wires; }

QString Circuit::contextName() const {
    if (m_currentCtxId.isEmpty()) return "主电路";
    QString n = m_subNames.value(m_currentCtxId, "子电路");
    if (m_viewOnly) n += "（浏览）";
    return n;
}

QVariantList Circuit::subcircuits() const {
    QVariantList out;
    for (auto it = m_subNames.begin(); it != m_subNames.end(); ++it) {
        QVariantMap m;
        m["id"] = it.key();
        m["name"] = it.value();
        const Context* ctx = ctxById(it.key());
        int inCount = 0, outCount = 0;
        if (ctx) {
            for (auto& cv : ctx->components) {
                QString t = cv.toMap().value("type").toString();
                if (t == "input") inCount++;
                else if (t == "output") outCount++;
            }
        }
        m["inCount"] = inCount;
        m["outCount"] = outCount;
        out.append(m);
    }
    return out;
}

QVariantList Circuit::editContexts() const {
    QVariantList out;
    QVariantMap rootMap;
    rootMap["id"] = "";
    rootMap["name"] = "主电路";
    out.append(rootMap);
    QStringList ids = m_subNames.keys();
    std::sort(ids.begin(), ids.end());
    for (auto& id : ids) {
        QVariantMap m;
        m["id"] = id;
        m["name"] = m_subNames.value(id);
        out.append(m);
    }
    return out;
}

int Circuit::indexOfComponent(const QString& id) const {
    const Context& ctx = currentCtx();
    for (int i = 0; i < ctx.components.size(); ++i)
        if (ctx.components[i].toMap().value("id").toString() == id) return i;
    return -1;
}

int Circuit::indexOfWire(const QString& id) const {
    const Context& ctx = currentCtx();
    for (int i = 0; i < ctx.wires.size(); ++i)
        if (ctx.wires[i].toMap().value("id").toString() == id) return i;
    return -1;
}

int Circuit::componentInputCount(const QVariantMap& comp) {
    const QString t = comp.value("type").toString();
    if (t == "input" || t == "clock" || t == "text") return 0;
    if (t == "not") return 1;
    if (t == "and" || t == "or" || t == "nand" || t == "nor" || t == "xor" || t == "xnor") {
        int n = comp.value("inputCount").toInt();
        return n < 2 ? 2 : qMin(n, 64);
    }
    if (t == "led" || t == "output") return 1;
    if (t == "splitter") return 1;
    if (t == "hub") return qMax(1, comp.value("outputSplits").toList().size());
    if (t == "sub") return comp.value("subInputNames").toList().size();
    return 0;
}

int Circuit::componentOutputCount(const QVariantMap& comp) {
    const QString t = comp.value("type").toString();
    if (t == "input" || t == "clock") return 1;
    if (t == "text") return 0;
    if (t == "not" || t == "and" || t == "or" || t == "nand" ||
        t == "nor" || t == "xor" || t == "xnor") return 1;
    if (t == "splitter") return qMax(1, comp.value("outputSplits").toList().size());
    if (t == "hub") return 1;
    if (t == "led" || t == "output") return 0;
    if (t == "sub") return comp.value("subOutputNames").toList().size();
    return 0;
}

int Circuit::outputPortBitWidth(const QVariantMap& comp, int portIdx) {
    const QString t = comp.value("type").toString();
    int bw = comp.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (t == "input" || t == "clock" || t == "not" || t == "and" || t == "or" ||
        t == "nand" || t == "nor" || t == "xor" || t == "xnor") return bw;
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

int Circuit::inputPortBitWidth(const QVariantMap& comp, int portIdx) {
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

QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    if (m_viewOnly) return QString();
    if (bitWidth < 1) bitWidth = 1;
    if (bitWidth > 64) bitWidth = 64;
    if (inputCount < 2) inputCount = 2;
    if (inputCount > 64) inputCount = 64;

    if (type == "sub" && !canAddSubInstance(subId)) return QString();

    QVariantMap c;
    Context& ctx = currentCtx();
    c["id"] = QString("C%1").arg(++ctx.compCounter);
    c["type"] = type;
    c["x"] = x; c["y"] = y;
    c["bitWidth"] = bitWidth;
    c["inputCount"] = inputCount;
    c["displayBase"] = "bin";
    c["name"] = "";

    QVariantList bits;
    for (int i = 0; i < bitWidth; ++i) bits.append(false);
    c["inputBits"] = bits;

    QVariantList outPorts, inPorts;

    if (type == "text") {
        c["content"] = "文本";
    } else if (type == "clock") {
        c["name"] = "CLK";
    } else if (type == "sub") {
        c["subId"] = subId;
        c["subInputNames"] = QVariantList();
        c["subOutputNames"] = QVariantList();
        c["subInputWidths"] = QVariantList();
        c["subOutputWidths"] = QVariantList();
    } else if (type == "splitter") {
        QVariantList splits;
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        int segN = splits.size();
        inPorts.append(QString(bitWidth, '0'));
        for (int i = 0; i < segN; ++i) {
            int w = splits[i].toInt();
            if (w < 1) w = 1;
            outPorts.append(QString(w, '0'));
        }
    } else if (type == "hub") {
        QVariantList splits;
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        c["outputSplits"] = splits;
        int segN = splits.size();
        for (int i = 0; i < segN; ++i) {
            int w = splits[i].toInt();
            if (w < 1) w = 1;
            inPorts.append(QString(w, '0'));
        }
        outPorts.append(QString(bitWidth, '0'));
    } else {
        int n = 0;
        if (type == "input" || type == "clock" || type == "not" ||
            type == "and" || type == "or" || type == "nand" ||
            type == "nor" || type == "xor" || type == "xnor") n = 1;
        for (int i = 0; i < n; ++i) outPorts.append(QString(bitWidth, '0'));
        int ic = 0;
        if (type == "not" || type == "led" || type == "output") ic = 1;
        else if (type == "and" || type == "or" || type == "nand" ||
                 type == "nor" || type == "xor" || type == "xnor") ic = inputCount;
        for (int i = 0; i < ic; ++i) inPorts.append(QString(bitWidth, '0'));
    }

    c["outputPorts"] = outPorts;
    c["inputPortValues"] = inPorts;

    ctx.components.append(c);

    if (type == "clock") m_clockStates[c["id"].toString()] = false;

    evaluateAll();
    emit changed();
    return c["id"].toString();
}

QString Circuit::addText(double x, double y, const QString& text) {
    QString id = addComponent("text", x, y, 1, 2, "");
    if (!id.isEmpty()) setTextContent(id, text);
    return id;
}

void Circuit::setTextContent(const QString& id, const QString& content) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "text") return;
    c["content"] = content;
    ctx.components[idx] = c;
    emit changed();
}

void Circuit::removeComponent(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() == "clock") m_clockStates.remove(id);
    ctx.components.removeAt(idx);
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (w.value("fromComp").toString() == id || w.value("toComp").toString() == id)
            ctx.wires.removeAt(i);
    }
    evaluateAll();
    emit changed();
}

void Circuit::removeComponents(const QStringList& ids) {
    if (m_viewOnly) return;
    Context& ctx = currentCtx();
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        auto c = ctx.components[i].toMap();
        if (ids.contains(c.value("id").toString())) {
            if (c.value("type").toString() == "clock")
                m_clockStates.remove(c.value("id").toString());
            ctx.components.removeAt(i);
        }
    }
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (ids.contains(w.value("fromComp").toString()) ||
            ids.contains(w.value("toComp").toString()))
            ctx.wires.removeAt(i);
    }
    evaluateAll();
    emit changed();
}

void Circuit::moveComponent(const QString& id, double x, double y) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    c["x"] = x; c["y"] = y;
    ctx.components[idx] = c;
    emit geometryChanged();
}

void Circuit::moveSelection(const QStringList& ids, double dx, double dy) {
    if (m_viewOnly) return;
    if (ids.isEmpty()) return;
    Context& ctx = currentCtx();
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);
    for (int i = 0; i < ctx.components.size(); ++i) {
        QVariantMap c = ctx.components[i].toMap();
        if (idSet.contains(c.value("id").toString())) {
            c["x"] = c.value("x").toDouble() + dx;
            c["y"] = c.value("y").toDouble() + dy;
            ctx.components[i] = c;
        }
    }
    emit geometryChanged();
}

void Circuit::toggleInput(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "input") return;
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    bool v = !bits[0].toBool();
    for (int i = 0; i < bits.size(); ++i) bits[i] = v;
    c["inputBits"] = bits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::setBitValue(const QString& id, int bit, bool value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    if (bit < 0 || bit >= bits.size()) return;
    bits[bit] = value;
    c["inputBits"] = bits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::setComponentProp(const QString& id, const QString& key, const QVariant& value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (key == "bitWidth") {
        int bw = value.toInt();
        if (bw < 1) bw = 1;
        if (bw > 64) bw = 64;
        c["bitWidth"] = bw;
        QVariantList bits = c.value("inputBits").toList();
        while (bits.size() < bw) bits.prepend(false);
        while (bits.size() > bw) bits.removeFirst();
        c["inputBits"] = bits;
        QString t = c.value("type").toString();
        if (t == "splitter" || t == "hub") {
            QVariantList splits = c.value("outputSplits").toList();
            int sum = 0;
            for (auto& sv : splits) sum += sv.toInt();
            if (sum != bw) {
                QVariantList ns;
                for (int i = 0; i < bw; ++i) ns.append(1);
                c["outputSplits"] = ns;
            }
        }
    } else if (key == "inputCount") {
        int v = value.toInt();
        if (v < 2) v = 2;
        if (v > 64) v = 64;
        c[key] = v;
    } else {
        c[key] = value;
    }
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

void Circuit::renameComponent(const QString& id, const QString& name) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    c["name"] = name;
    ctx.components[idx] = c;
    emit changed();
}

void Circuit::setSplitterSplits(const QString& id, const QString& splitsStr) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return;
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    QString s = splitsStr;
    s.replace(',', ' ');
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    QVariantList splits;
    int sum = 0;
    for (auto& p : parts) {
        bool ok = false;
        int v = p.toInt(&ok);
        if (!ok || v < 1) continue;
        splits.append(v); sum += v;
    }
    if (sum != bw || splits.isEmpty()) return;
    c["outputSplits"] = splits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
}

QString Circuit::getSplitterSplitsStr(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    QVariantMap c = currentCtx().components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return {};
    QVariantList splits = c.value("outputSplits").toList();
    QStringList parts;
    for (auto& v : splits) parts.append(QString::number(v.toInt()));
    return parts.join(",");
}

QString Circuit::getInputBase(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return "bin";
    QVariantMap c = currentCtx().components[idx].toMap();
    QString b = c.value("displayBase").toString();
    return b.isEmpty() ? "bin" : b;
}

void Circuit::setInputBase(const QString& id, const QString& base) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    c["displayBase"] = base;
    ctx.components[idx] = c;
    emit changed();
}

QString Circuit::formatValue(const QString& binStr, const QString& base, int bw) const {
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;
    QString s = binStr;
    if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
    if (s.length() > bw) s = s.right(bw);
    QString b = base;
    if (b.isEmpty()) b = "bin";
    if (b == "bin") return s;
    if (b == "hex") {
        int digits = (bw + 3) / 4;
        QString out;
        for (int d = digits - 1; d >= 0; --d) {
            int v = 0;
            for (int bit = 0; bit < 4; ++bit) {
                int pos = d * 4 + bit;
                int idx = bw - 1 - pos;
                if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1 << bit);
            }
            out += (v < 10) ? QChar('0' + v) : QChar('A' + v - 10);
        }
        return out;
    }
    if (b == "udec" || b == "sdec") {
        quint64 v = 0;
        for (int i = 0; i < bw && i < 64; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1ULL << i);
        }
        if (b == "udec") return QString::number(v);
        if (bw < 64) {
            qint64 sv = (qint64)v;
            qint64 sign = 1LL << (bw - 1);
            if (sv & sign) sv -= (1LL << bw);
            return QString::number(sv);
        }
        return QString::number((qint64)v);
    }
    if (b == "f32") {
        quint32 uv = 0;
        for (int i = 0; i < 32; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1u << i);
        }
        float f; std::memcpy(&f, &uv, 4);
        return QString::number((double)f, 'g', 9);
    }
    if (b == "f64") {
        quint64 uv = 0;
        for (int i = 0; i < 64; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1ULL << i);
        }
        double d; std::memcpy(&d, &uv, 8);
        return QString::number(d, 'g', 17);
    }
    return s;
}

QString Circuit::getInputAsString(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return "";
    QVariantMap c = currentCtx().components[idx].toMap();
    QString base = c.value("displayBase").toString();
    if (base.isEmpty()) base = "bin";
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;
    QVariantList bits = c.value("inputBits").toList();
    QString binStr;
    for (int i = 0; i < bw; ++i)
        binStr += (i < bits.size() && bits[i].toBool()) ? '1' : '0';
    return formatValue(binStr, base, bw);
}

QVariantList Circuit::parseToBits(const QString& text, const QString& base, int bw, QString* err) const {
    QVariantList bits;
    for (int i = 0; i < bw; ++i) bits.append(false);
    QString s = text.trimmed();
    if (s.isEmpty()) return bits;
    if (base == "bin") {
        for (auto ch : s) if (ch != '0' && ch != '1') { if (err) *err = "二进制只能是 0/1"; return bits; }
        if (s.length() > bw) { if (err) *err = QString("最多 %1 位").arg(bw); return bits; }
        int offset = bw - s.length();
        for (int i = 0; i < s.length(); ++i)
            bits[offset + i] = (s[i] == '1');
        return bits;
    }
    if (base == "hex") {
        QString up = s.toUpper();
        for (auto ch : up) if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) { if (err) *err = "非法十六进制"; return bits; }
        int maxDigits = (bw + 3) / 4;
        if (up.length() > maxDigits) { if (err) *err = QString("最多 %1 位十六进制").arg(maxDigits); return bits; }
        for (int i = up.length() - 1; i >= 0; --i) {
            int digitFromRight = (up.length() - 1 - i);
            QChar ch = up[i];
            int v = (ch >= '0' && ch <= '9') ? (ch.unicode() - '0') : (ch.unicode() - 'A' + 10);
            for (int bit = 0; bit < 4; ++bit) {
                int pos = digitFromRight * 4 + bit;
                int idx = bw - 1 - pos;
                if (idx >= 0 && idx < bw)
                    bits[idx] = (v & (1 << bit)) != 0;
            }
        }
        return bits;
    }
    if (base == "udec") {
        bool ok = false;
        qint64 v = s.toLongLong(&ok);
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        if (v < 0) { if (err) *err = "无符号不能为负"; return bits; }
        if (bw < 63) {
            qint64 maxV = (1LL << bw) - 1;
            if (v > maxV) { if (err) *err = QString("超出范围 [0, %1]").arg(maxV); return bits; }
        }
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((v >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "sdec") {
        bool ok = false;
        qint64 v = s.toLongLong(&ok);
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        if (bw < 63) {
            qint64 minV = -(1LL << (bw - 1));
            qint64 maxV = (1LL << (bw - 1)) - 1;
            if (v < minV || v > maxV) { if (err) *err = QString("超出范围 [%1, %2]").arg(minV).arg(maxV); return bits; }
        }
        quint64 uv = (quint64)v;
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "f32") {
        if (bw < 32) { if (err) *err = "需要至少 32 位"; return bits; }
        bool ok = false; float f = s.toFloat(&ok);
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        quint32 uv; std::memcpy(&uv, &f, 4);
        for (int i = 0; i < 32; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "f64") {
        if (bw < 64) { if (err) *err = "需要至少 64 位"; return bits; }
        bool ok = false; double d = s.toDouble(&ok);
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        quint64 uv; std::memcpy(&uv, &d, 8);
        for (int i = 0; i < 64; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (err) *err = "未知进制";
    return bits;
}

QString Circuit::setInputFromString(const QString& id, const QString& text) {
    if (m_viewOnly) return "浏览模式不可修改";
    int idx = indexOfComponent(id);
    if (idx < 0) return "元件不存在";
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "input") return "不是输入元件";
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;
    QString base = c.value("displayBase").toString();
    if (base.isEmpty()) base = "bin";
    QString err;
    QVariantList bits = parseToBits(text, base, bw, &err);
    if (!err.isEmpty()) return err;
    c["inputBits"] = bits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
    return "";
}

QString Circuit::addWire(const QString& fromComp, int fromPort,
                         const QString& toComp, int toPort) {
    if (m_viewOnly) return QString();
    if (indexOfComponent(fromComp) < 0 || indexOfComponent(toComp) < 0) return QString();
    if (fromComp == toComp) return QString();
    Context& ctx = currentCtx();

    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        auto w = ctx.wires[i].toMap();
        if (w.value("toComp").toString() == toComp && w.value("toPort").toInt() == toPort)
            ctx.wires.removeAt(i);
    }

    QVariantMap w;
    w["id"] = QString("W%1").arg(++ctx.wireCounter);
    w["fromComp"] = fromComp;
    w["fromPort"] = fromPort;
    w["toComp"] = toComp;
    w["toPort"] = toPort;
    ctx.wires.append(w);
    evaluateAll();
    emit changed();
    return w["id"].toString();
}

void Circuit::removeWire(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfWire(id);
    if (idx < 0) return;
    currentCtx().wires.removeAt(idx);
    evaluateAll();
    emit changed();
}

QVariantMap Circuit::getComponent(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return currentCtx().components[idx].toMap();
}

QVariantMap Circuit::getWire(const QString& id) const {
    int idx = indexOfWire(id);
    if (idx < 0) return {};
    return currentCtx().wires[idx].toMap();
}

QVariantList Circuit::outputPortsInfo(const QString& id) const {
    QVariantList result;
    int idx = indexOfComponent(id);
    if (idx < 0) return result;
    QVariantMap c = currentCtx().components[idx].toMap();
    const QString t = c.value("type").toString();
    int n = componentOutputCount(c);

    double W = 80.0, H = 60.0;

    if (t == "input" || t == "clock") {
        W = qMax(60.0, c.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputPortBitWidth(c, 0);
        result.append(p);
    } else if (t == "splitter") {
        W = 80.0;
        QVariantList splits = c.value("outputSplits").toList();
        int segN = qMax(1, splits.size());
        H = qMax(60.0, (double)segN * 24.0);
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = outputPortBitWidth(c, i);
            result.append(p);
        }
    } else if (t == "hub") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = outputPortBitWidth(c, 0);
        result.append(p);
    } else if (t == "not" || t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub") {
        W = t == "sub" ? 110.0 : 80.0;
        int ic = componentInputCount(c);
        int oc = n; if (oc < 1) oc = 1;
        H = qMax(60.0, (double)qMax(ic, oc) * 24.0);
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = outputPortBitWidth(c, i);
            result.append(p);
        }
    }
    return result;
}

QVariantList Circuit::inputPortsInfo(const QString& id) const {
    QVariantList result;
    int idx = indexOfComponent(id);
    if (idx < 0) return result;
    QVariantMap c = currentCtx().components[idx].toMap();
    const QString t = c.value("type").toString();
    int n = componentInputCount(c);

    double W = 80.0, H = 60.0;

    if (t == "led" || t == "output") {
        W = qMax(t == "output" ? 60.0 : 50.0, c.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputPortBitWidth(c, 0);
        result.append(p);
    } else if (t == "splitter") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputPortBitWidth(c, 0);
        result.append(p);
    } else if (t == "hub") {
        W = 80.0;
        QVariantList splits = c.value("outputSplits").toList();
        int segN = qMax(1, splits.size());
        H = qMax(60.0, (double)segN * 24.0);
        for (int i = 0; i < segN; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            p["y"] = -H/2 + (i + 0.5) * (H / segN);
            p["bitWidth"] = inputPortBitWidth(c, i);
            result.append(p);
        }
    } else if (t == "not") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = inputPortBitWidth(c, 0);
        result.append(p);
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub") {
        W = t == "sub" ? 110.0 : 80.0;
        H = qMax(60.0, (double)n * 24.0);
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = -W/2;
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = inputPortBitWidth(c, i);
            result.append(p);
        }
    }
    return result;
}

QVariantMap Circuit::allPortsInfo() const {
    QVariantMap out;
    const Context& ctx = currentCtx();
    for (const auto& cv : ctx.components) {
        QVariantMap c = cv.toMap();
        QString id = c.value("id").toString();
        QVariantMap info;
        info["out"] = outputPortsInfo(id);
        info["in"] = inputPortsInfo(id);
        out[id] = info;
    }
    return out;
}

QString Circuit::addSubcircuit(const QString& name) {
    if (m_viewOnly) return QString();
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
    emit contextChanged(); emit changed();
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

QVariantList Circuit::copySelection(const QStringList& ids) {
    QVariantList out;
    const Context& ctx = currentCtx();
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);
    QVariantList compsOut, wiresOut;
    for (auto& cv : ctx.components) {
        auto c = cv.toMap();
        if (idSet.contains(c.value("id").toString())) compsOut.append(c);
    }
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        if (idSet.contains(w.value("fromComp").toString()) &&
            idSet.contains(w.value("toComp").toString()))
            wiresOut.append(w);
    }
    QVariantMap data;
    data["components"] = compsOut;
    data["wires"] = wiresOut;
    out.append(data);
    return out;
}

QStringList Circuit::pasteSelection(const QVariantList& data, double dx, double dy) {
    QStringList newIds;
    if (m_viewOnly) return newIds;
    if (data.isEmpty()) return newIds;
    QVariantMap d = data[0].toMap();
    QVariantList comps = d.value("components").toList();
    QVariantList wires = d.value("wires").toList();
    Context& ctx = currentCtx();

    QHash<QString, QString> idMap;
    for (auto& cv : comps) {
        QVariantMap c = cv.toMap();
        QString oldId = c.value("id").toString();
        QString newId = QString("C%1").arg(++ctx.compCounter);
        idMap[oldId] = newId;
        c["id"] = newId;
        c["x"] = c.value("x").toDouble() + dx;
        c["y"] = c.value("y").toDouble() + dy;
        if (c.value("type").toString() == "clock") m_clockStates[newId] = false;
        ctx.components.append(c);
        newIds.append(newId);
    }
    for (auto& wv : wires) {
        QVariantMap w = wv.toMap();
        QString from = idMap.value(w.value("fromComp").toString(), "");
        QString to = idMap.value(w.value("toComp").toString(), "");
        if (from.isEmpty() || to.isEmpty()) continue;
        QVariantMap nw;
        nw["id"] = QString("W%1").arg(++ctx.wireCounter);
        nw["fromComp"] = from;
        nw["fromPort"] = w.value("fromPort");
        nw["toComp"] = to;
        nw["toPort"] = w.value("toPort");
        ctx.wires.append(nw);
    }
    evaluateAll();
    emit changed();
    return newIds;
}

void Circuit::refreshErrors() {
    m_errors.clear();
    auto checkCtx = [&](const Context& ctx, const QString& label) {
        for (auto& wv : ctx.wires) {
            QVariantMap w = wv.toMap();
            QString fromId = w.value("fromComp").toString();
            QString toId = w.value("toComp").toString();
            int fromPort = w.value("fromPort").toInt();
            int toPort = w.value("toPort").toInt();
            int fi = -1, ti = -1;
            for (int i = 0; i < ctx.components.size(); ++i) {
                QString cid = ctx.components[i].toMap().value("id").toString();
                if (cid == fromId) fi = i;
                if (cid == toId) ti = i;
            }
            if (fi < 0 || ti < 0) continue;
            int fbw = outputPortBitWidth(ctx.components[fi].toMap(), fromPort);
            int tbw = inputPortBitWidth(ctx.components[ti].toMap(), toPort);
            if (fbw != tbw) {
                QVariantMap err;
                err["type"] = "width_mismatch";
                err["msg"] = QString("%1：%2 位宽不匹配（源 %3 位，目标 %4 位）")
                                 .arg(label, w.value("id").toString()).arg(fbw).arg(tbw);
                m_errors.append(err);
            }
        }
    };
    checkCtx(m_root, "主电路");
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        checkCtx(it.value(), m_subNames.value(it.key(), it.key()));
    emit errorsChanged();
}

static inline bool strBit(const QString& s, int idx) {
    if (idx < 0 || idx >= s.length()) return false;
    return s[idx] == '1';
}
static inline QString bitsToStr(const QVariantList& bits, int bw) {
    QString s;
    s.reserve(bw);
    for (int i = 0; i < bw; ++i)
        s += (i < bits.size() && bits[i].toBool()) ? '1' : '0';
    return s;
}

void Circuit::syncSubPins(Context& ctx) {
    for (int i = 0; i < ctx.components.size(); ++i) {
        QVariantMap c = ctx.components[i].toMap();
        if (c.value("type").toString() != "sub") continue;
        QString subId = c.value("subId").toString();
        auto sit = m_subContexts.find(subId);
        if (sit == m_subContexts.end()) continue;

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

static bool evalOne(QVariantMap& c,
                    const QHash<QString, QPair<int,int>>& inputMap,
                    const QVector<QVariantMap>& comps) {
    const QString id = c.value("id").toString();
    const QString t = c.value("type").toString();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;

    if (t == "text") return false;

    int ic = Circuit::componentInputCount(c);
    QVariantList newInList;
    newInList.reserve(ic);
    for (int p = 0; p < ic; ++p) {
        int bwp = Circuit::inputPortBitWidth(c, p);
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
        newOutList.append(bitsToStr(c.value("inputBits").toList(), bw));
    } else if (t == "clock") {
        newOutList = c.value("outputPorts").toList();
        if (newOutList.isEmpty()) newOutList.append(QString(bw, '0'));
    } else if (t == "not") {
        QString a = newInList.isEmpty() ? QString(bw, '0') : newInList[0].toString();
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) out += strBit(a, b) ? '0' : '1';
        newOutList.append(out);
    } else if (t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor") {
        QString out; out.reserve(bw);
        for (int b = 0; b < bw; ++b) {
            bool r = (t == "and" || t == "nand") ? true : false;
            for (int p = 0; p < ic; ++p) {
                QString pv = (p < newInList.size()) ? newInList[p].toString() : QString(bw, '0');
                bool v = strBit(pv, b);
                if (t == "and" || t == "nand") r = r && v;
                else if (t == "or" || t == "nor") r = r || v;
                else if (t == "xor" || t == "xnor") r = r != v;
            }
            if (t == "nand" || t == "nor" || t == "xnor") r = !r;
            out += r ? '1' : '0';
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
                out += strBit(mainIn, offset + b) ? '1' : '0';
            newOutList.append(out);
        }
    } else if (t == "hub") {
        QString merged;
        for (int s = newInList.size() - 1; s >= 0; --s) {
            int len = Circuit::inputPortBitWidth(c, s);
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

void Circuit::evaluateContext(Context& ctx) {
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
        for (int iter = 0; iter < 50; ++iter) {
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

void Circuit::evaluateAll() {
    syncSubPins(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        syncSubPins(it.value());

    if (m_root.components.isEmpty() && m_subContexts.isEmpty()) return;

    for (int outer = 0; outer < 20; ++outer) {
        bool anyChanged = false;

        for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
            evaluateContext(it.value());
        evaluateContext(m_root);

        for (int i = 0; i < m_root.components.size(); ++i) {
            QVariantMap c = m_root.components[i].toMap();
            if (c.value("type").toString() != "sub") continue;
            QString subId = c.value("subId").toString();
            auto sit = m_subContexts.find(subId);
            if (sit == m_subContexts.end()) continue;

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
                m_root.components[i] = c;
                anyChanged = true;
            }
        }

        if (!anyChanged && outer > 0) break;
    }
}

void Circuit::evaluate() { evaluateAll(); }

// ★ 关键修复：tick 里 emit changed()，让 QML 重建缓存
void Circuit::tick() {
    m_clockTickCount++;

    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        it.value() = !it.value();

    auto updateClocks = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (c.value("type").toString() != "clock") continue;
            bool v = m_clockStates.value(c.value("id").toString(), false);
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList out;
            out.append(v ? QString(bw, '1') : QString(bw, '0'));
            c["outputPorts"] = out;
            ctx.components[i] = c;
        }
    };
    updateClocks(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        updateClocks(it.value());

    evaluateAll();

    emit changed();       // ★ 让 QML 重建缓存
    emit clockChanged();
}

void Circuit::startClock() {
    if (!m_clockTimer) return;
    m_clockRunning = true;
    int interval = qMax(1, 1000 / qMax(1, m_clockFrequency));
    m_clockTimer->start(interval);
    emit clockChanged();
}

void Circuit::stopClock() {
    if (!m_clockTimer) return;
    m_clockRunning = false;
    m_clockTimer->stop();
    emit clockChanged();
}

void Circuit::setClockFrequency(int hz) {
    m_clockFrequency = qBound(1, hz, 1000);
    if (m_clockRunning) {
        int interval = qMax(1, 1000 / m_clockFrequency);
        m_clockTimer->start(interval);
    }
    emit clockChanged();
}

void Circuit::singleStep() { tick(); }

void Circuit::resetAllInputs() {
    auto resetCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (c.value("type").toString() != "input") continue;
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList bits;
            for (int k = 0; k < bw; ++k) bits.append(false);
            c["inputBits"] = bits;
            ctx.components[i] = c;
        }
    };
    resetCtx(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        resetCtx(it.value());
    evaluateAll();
    emit changed();
}

QVariantMap Circuit::statistics() const {
    QVariantMap result;
    QHash<QString, int> typeCount;
    int totalGates = 0, totalInputs = 0, totalOutputs = 0, totalSplits = 0;
    int totalHubs = 0, totalClocks = 0, totalSubInst = 0, totalTexts = 0;

    std::function<void(const Context&, QSet<QString>&)> countCtx =
        [&](const Context& ctx, QSet<QString>& visited) {
            for (const auto& cv : ctx.components) {
                QVariantMap c = cv.toMap();
                QString t = c.value("type").toString();
                if (t == "text") { totalTexts++; continue; }
                if (t == "input") { totalInputs++; continue; }
                if (t == "output" || t == "led") { totalOutputs++; continue; }
                if (t == "clock") { totalClocks++; continue; }
                if (t == "splitter") { totalSplits++; continue; }
                if (t == "hub") { totalHubs++; continue; }
                if (t == "sub") {
                    totalSubInst++;
                    QString subId = c.value("subId").toString();
                    if (!visited.contains(subId)) {
                        visited.insert(subId);
                        auto it = m_subContexts.find(subId);
                        if (it != m_subContexts.end()) countCtx(it.value(), visited);
                    }
                    continue;
                }
                totalGates++;
                typeCount[t]++;
            }
        };

    QSet<QString> visited;
    countCtx(m_root, visited);

    result["totalGates"] = totalGates;
    result["totalInputs"] = totalInputs;
    result["totalOutputs"] = totalOutputs;
    result["totalSplits"] = totalSplits;
    result["totalHubs"] = totalHubs;
    result["totalClocks"] = totalClocks;
    result["totalSubInst"] = totalSubInst;
    result["totalTexts"] = totalTexts;
    result["subCount"] = m_subContexts.size();

    QVariantMap byType;
    for (auto it = typeCount.begin(); it != typeCount.end(); ++it)
        byType[it.key()] = it.value();
    result["byType"] = byType;
    return result;
}

void Circuit::clear() {
    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_subCounter = 0;
    m_errors.clear();
    m_clockStates.clear();
    m_clockTickCount = 0;
    if (m_clockRunning) stopClock();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
}

QVariantMap Circuit::toJson() const {
    QVariantMap result;
    result["version"] = 9;
    result["root"] = QVariantMap{
        {"components", m_root.components},
        {"wires", m_root.wires},
        {"compCounter", m_root.compCounter},
        {"wireCounter", m_root.wireCounter}
    };
    QVariantList subs;
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        QVariantMap s;
        s["id"] = it.key();
        s["name"] = m_subNames.value(it.key(), "");
        s["components"] = it.value().components;
        s["wires"] = it.value().wires;
        s["compCounter"] = it.value().compCounter;
        s["wireCounter"] = it.value().wireCounter;
        subs.append(s);
    }
    result["subcircuits"] = subs;
    result["subCounter"] = m_subCounter;
    result["clockFrequency"] = m_clockFrequency;
    return result;
}

bool Circuit::fromJson(const QVariantMap& data) {
    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_clockStates.clear();

    if (data.contains("root")) {
        QVariantMap r = data.value("root").toMap();
        m_root.components = r.value("components").toList();
        m_root.wires = r.value("wires").toList();
        m_root.compCounter = r.value("compCounter").toInt();
        m_root.wireCounter = r.value("wireCounter").toInt();
    }
    QVariantList subs = data.value("subcircuits").toList();
    for (auto& sv : subs) {
        QVariantMap s = sv.toMap();
        QString id = s.value("id").toString();
        Context c;
        c.components = s.value("components").toList();
        c.wires = s.value("wires").toList();
        c.compCounter = s.value("compCounter").toInt();
        c.wireCounter = s.value("wireCounter").toInt();
        m_subContexts[id] = c;
        m_subNames[id] = s.value("name").toString();
    }
    m_subCounter = data.value("subCounter").toInt();
    m_clockFrequency = qBound(1, data.value("clockFrequency").toInt(), 1000);
    if (m_clockFrequency < 1) m_clockFrequency = 2;

    auto fixCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (!c.contains("displayBase")) c["displayBase"] = "bin";
            if (!c.contains("name")) c["name"] = "";
            if (c.value("type").toString() == "text" && !c.contains("content"))
                c["content"] = "文本";
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList bits = c.value("inputBits").toList();
            while (bits.size() < bw) bits.prepend(false);
            while (bits.size() > bw) bits.removeFirst();
            c["inputBits"] = bits;
            QString t = c.value("type").toString();
            if ((t == "splitter" || t == "hub") && !c.contains("outputSplits")) {
                QVariantList splits;
                for (int j = 0; j < bw; ++j) splits.append(1);
                c["outputSplits"] = splits;
            }
            if (t == "clock") m_clockStates[c.value("id").toString()] = false;
            ctx.components[i] = c;
        }
    };
    fixCtx(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) fixCtx(it.value());

    evaluateAll();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
    return true;
}

bool Circuit::saveToFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    addRecentFile(path);
    return true;
}

bool Circuit::loadFromFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;
    bool ok = fromJson(doc.object().toVariantMap());
    if (ok) addRecentFile(path);
    return ok;
}

QString Circuit::wireAtPoint(double x, double y, double tol) const {
    const Context& ctx = currentCtx();
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        QString id = w.value("id").toString();
        QString fromId = w.value("fromComp").toString();
        QString toId = w.value("toComp").toString();
        int fromIdx = w.value("fromPort").toInt();
        int toIdx = w.value("toPort").toInt();

        auto compPos = [&](const QString& cid, int port, bool isOut, double& px, double& py) -> bool {
            int ci = indexOfComponent(cid);
            if (ci < 0) return false;
            QVariantMap c = ctx.components[ci].toMap();
            QVariantList ports = isOut ? outputPortsInfo(cid) : inputPortsInfo(cid);
            if (port < 0 || port >= ports.size()) return false;
            QVariantMap p = ports[port].toMap();
            px = c.value("x").toDouble() + p.value("x").toDouble();
            py = c.value("y").toDouble() + p.value("y").toDouble();
            return true;
        };

        double x1, y1, x2, y2;
        if (!compPos(fromId, fromIdx, true, x1, y1)) continue;
        if (!compPos(toId, toIdx, false, x2, y2)) continue;

        double cx = std::max(std::abs(x2 - x1) / 2.0, 40.0);
        const int samples = 20;
        for (int i = 0; i <= samples; ++i) {
            double t = (double)i / samples;
            double mt = 1.0 - t;
            double bx = mt*mt*mt*x1 + 3*mt*mt*t*(x1+cx) + 3*mt*t*t*(x2-cx) + t*t*t*x2;
            double by = mt*mt*mt*y1 + 3*mt*mt*t*y1 + 3*mt*t*t*y2 + t*t*t*y2;
            double dx = bx - x, dy = by - y;
            if (dx*dx + dy*dy < tol*tol) return id;
        }
    }
    return QString();
}

void Circuit::loadRecentFiles() {
    QSettings s;
    QString json = s.value("recentFiles").toString();
    if (json.isEmpty()) { m_recentFiles.clear(); return; }
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();
    m_recentFiles.clear();
    for (const auto v : arr)
        m_recentFiles.append(v.toObject().toVariantMap());
}

void Circuit::saveRecentFiles() {
    QJsonArray arr;
    for (auto& v : m_recentFiles)
        arr.append(QJsonObject::fromVariantMap(v.toMap()));
    QSettings s;
    s.setValue("recentFiles", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    s.sync();
}

void Circuit::addRecentFile(const QString& path) {
    if (path.isEmpty()) return;
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    QVariantMap m;
    m["path"] = path;
    QFileInfo fi(path);
    m["name"] = fi.fileName();
    m["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    m_recentFiles.prepend(m);
    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::removeRecentFile(const QString& path) {
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::clearRecentFiles() {
    m_recentFiles.clear();
    saveRecentFiles();
    emit recentFilesChanged();
}