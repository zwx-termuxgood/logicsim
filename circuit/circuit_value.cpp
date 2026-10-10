#include "circuit.h"
#include "ValueCodec.h"
#include "EventEngine.h"

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