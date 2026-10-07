#include "circuit.h"
#include "ComponentTraits.h"
#include "PortGeometry.h"
#include <QVariantMap>
#include <QString>

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
    return currentCtx().indexOfComponent(id);
}

int Circuit::indexOfWire(const QString& id) const {
    return currentCtx().indexOfWire(id);
}

// 静态转发到 ComponentTraits
int Circuit::componentInputCount(const QVariantMap& comp) {
    return ComponentTraits::inputCount(comp);
}
int Circuit::componentOutputCount(const QVariantMap& comp) {
    return ComponentTraits::outputCount(comp);
}
int Circuit::outputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentTraits::outputBitWidth(comp, portIdx);
}
int Circuit::inputPortBitWidth(const QVariantMap& comp, int portIdx) {
    return ComponentTraits::inputBitWidth(comp, portIdx);
}

QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    if (m_viewOnly) return QString();
    if (bitWidth < 1) bitWidth = 1;
    if (bitWidth > 64) bitWidth = 64;
    if (inputCount < 2) inputCount = 2;
    if (inputCount > 64) inputCount = 64;

    if (type == "sub" && !canAddSubInstance(subId)) return QString();

    pushUndo();

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
    } else if (type == "tgate" || type == "ntran" || type == "ptran") {
        // 传输门/晶体管：2 路输入（数据 D、控制 G），1 路输出
        c["inputCount"] = 2;
        outPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
        inPorts.append(QString(bitWidth, '0'));
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
    if (c.value("content").toString() == content) return;
    pushUndo("text:" + id);
    c["content"] = content;
    ctx.components[idx] = c;
    emit changed();
}