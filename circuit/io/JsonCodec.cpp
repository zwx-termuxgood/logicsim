#include "JsonCodec.h"
#include <QtGlobal>

namespace JsonCodec {

QVariantMap encode(const CircuitContext& root,
                   const QHash<QString, CircuitContext>& subContexts,
                   const QHash<QString, QString>& subNames,
                   int subCounter,
                   int clockFrequency,
                   int clockTickCount,
                   const QString& engineType,
                   qulonglong globalGateDelay,
                   qulonglong wireDelay,
                   qulonglong simulationSpeed,
                   int simulationSpeedExp,
                   bool autoSpeed,
                   const QHash<QString, quint64>& gateTypeDelays)
{
    QVariantMap result;
    result["version"] = 11;
    result["root"] = QVariantMap{
        {"components", root.components},
        {"wires", root.wires},
        {"compCounter", root.compCounter},
        {"wireCounter", root.wireCounter}
    };
    QVariantList subs;
    for (auto it = subContexts.begin(); it != subContexts.end(); ++it) {
        QVariantMap s;
        s["id"] = it.key();
        s["name"] = subNames.value(it.key(), "");
        s["components"] = it.value().components;
        s["wires"] = it.value().wires;
        s["compCounter"] = it.value().compCounter;
        s["wireCounter"] = it.value().wireCounter;
        subs.append(s);
    }
    result["subcircuits"] = subs;
    result["subCounter"] = subCounter;
    result["clockFrequency"] = clockFrequency;
    result["clockTickCount"] = clockTickCount;
    result["engineType"] = engineType;
    result["globalGateDelay"] = QVariant::fromValue<qulonglong>(globalGateDelay);
    result["wireDelay"] = QVariant::fromValue<qulonglong>(wireDelay);
    result["simulationSpeed"] = QVariant::fromValue<qulonglong>(simulationSpeed);
    result["simulationSpeedExp"] = simulationSpeedExp;
    result["autoSpeed"] = autoSpeed;
    QVariantMap gateDelays;
    for (auto it = gateTypeDelays.begin(); it != gateTypeDelays.end(); ++it) {
        gateDelays[it.key()] = QVariant::fromValue<qulonglong>(it.value());
    }
    result["gateTypeDelays"] = gateDelays;
    return result;
}

static void fixCtx(CircuitContext& ctx, QHash<QString, bool>& clockStates) {
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
        if (t == "clock") clockStates[c.value("id").toString()] = false;
        ctx.components[i] = c;
    }
}

DecodedData decode(const QVariantMap& data) {
    DecodedData out;

    if (data.contains("root")) {
        QVariantMap r = data.value("root").toMap();
        out.root.components = r.value("components").toList();
        out.root.wires = r.value("wires").toList();
        out.root.compCounter = r.value("compCounter").toInt();
        out.root.wireCounter = r.value("wireCounter").toInt();
    }

    QVariantList subs = data.value("subcircuits").toList();
    for (auto& sv : subs) {
        QVariantMap s = sv.toMap();
        QString id = s.value("id").toString();
        CircuitContext c;
        c.components = s.value("components").toList();
        c.wires = s.value("wires").toList();
        c.compCounter = s.value("compCounter").toInt();
        c.wireCounter = s.value("wireCounter").toInt();
        out.subContexts[id] = c;
        out.subNames[id] = s.value("name").toString();
    }

    out.subCounter = data.value("subCounter").toInt();

    out.clockFrequency = qBound(1, data.value("clockFrequency").toInt(), 1000);
    if (out.clockFrequency < 1) out.clockFrequency = 2;

    out.clockTickCount = data.value("clockTickCount").toInt();
    if (out.clockTickCount < 0) out.clockTickCount = 0;

    out.engineType = data.value("engineType", "event").toString();
    if (out.engineType != "iter" && out.engineType != "event")
        out.engineType = "event";

    qulonglong gd = data.value("globalGateDelay").toULongLong();
    if (gd < 1) gd = 1;
    out.globalGateDelay = gd;

    qulonglong wd = data.value("wireDelay").toULongLong();
    if (wd < 1) wd = 1;
    out.wireDelay = wd;

    QVariantMap gmap = data.value("gateTypeDelays").toMap();
    for (auto it = gmap.begin(); it != gmap.end(); ++it) {
        qulonglong v = it.value().toULongLong();
        out.gateTypeDelays[it.key()] = v;
    }

    out.simulationSpeed = data.value("simulationSpeed").toULongLong();
    if (out.simulationSpeed < 1) out.simulationSpeed = 1000;

    out.simulationSpeedExp = data.value("simulationSpeedExp").toInt();
    if (out.simulationSpeedExp < 0) out.simulationSpeedExp = 0;
    if (out.simulationSpeedExp > 12) out.simulationSpeedExp = 12;

    out.autoSpeed = data.value("autoSpeed", true).toBool();

    fixCtx(out.root, out.clockStates);
    for (auto it = out.subContexts.begin(); it != out.subContexts.end(); ++it)
        fixCtx(it.value(), out.clockStates);

    return out;
}

} // namespace JsonCodec
