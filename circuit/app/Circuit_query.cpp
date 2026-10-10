#include "../Circuit.h"
#include "../core/ValueCodec.h"
#include "../core/Component.h"
#include "../logic/ComponentLogic.h"
#include "../logic/event/EventEngine.h"
#include <functional>

// ============================================================
// 元件 / 连线查询
// ============================================================

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
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return ComponentLogic::outputPorts(currentCtx().components[idx].toMap());
}

QVariantList Circuit::inputPortsInfo(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    return ComponentLogic::inputPorts(currentCtx().components[idx].toMap());
}

QVariantMap Circuit::allPortsInfo() const {
    QVariantMap out;
    const Context& ctx = currentCtx();
    for (const auto& cv : ctx.components) {
        QVariantMap c = cv.toMap();
        QString id = c.value("id").toString();
        QVariantMap info;
        info["out"] = ComponentLogic::outputPorts(c);
        info["in"] = ComponentLogic::inputPorts(c);
        out[id] = info;
    }
    return out;
}

// ============================================================
// 值 / 进制
// ============================================================

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
    if (c.value("displayBase").toString() == base) return;
    pushUndo("base:" + id);
    c["displayBase"] = base;
    ctx.components[idx] = c;
    emit changed();
}

QString Circuit::formatValue(const QString& binStr, const QString& base, int bw) const {
    return ValueCodec::format(binStr, base, bw);
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
    QString binStr = ValueCodec::bitsToString(bits, bw);
    return ValueCodec::format(binStr, base, bw);
}

QString Circuit::setInputFromString(const QString& id, const QString& text) {
    if (m_viewOnly) return "浏览模式不可修改";
    int idx = indexOfComponent(id);
    if (idx < 0) return "元件不存在";
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "input" && t != "const") return "不是可编辑的元件";
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;
    QString base = c.value("displayBase").toString();
    if (base.isEmpty()) base = "bin";
    QString err;
    QVariantList bits = ValueCodec::parse(text, base, bw, &err);
    if (!err.isEmpty()) return err;
    if (c.value("inputBits").toList() == bits) return "";
    c["inputBits"] = bits;
    ctx.components[idx] = c;

    if (m_engineType == "event" && m_eventEngine) {
        QString binStr = ValueCodec::bitsToString(bits, bw);
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else {
        evaluateAll();
    }
    emit changed();
    return "";
}

// ============================================================
// 统计
// ============================================================

QVariantMap Circuit::statistics() const {
    QVariantMap result;
    QHash<QString, int> typeCount;
    int totalGates = 0, totalInputs = 0, totalOutputs = 0, totalSplits = 0;
    int totalHubs = 0, totalClocks = 0, totalSubInst = 0, totalTexts = 0;
    int totalConsts = 0;

    std::function<void(const Context&, QSet<QString>&)> countCtx =
        [&](const Context& ctx, QSet<QString>& visited) {
            for (const auto& cv : ctx.components) {
                QVariantMap c = cv.toMap();
                QString t = c.value("type").toString();
                if (t == "text") { totalTexts++; continue; }
                if (t == "input") { totalInputs++; continue; }
                if (t == "const") { totalConsts++; continue; }
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
    result["totalConsts"] = totalConsts;
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
