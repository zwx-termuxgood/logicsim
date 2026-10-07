#include "ComponentTraits.h"
#include <QtGlobal>

namespace ComponentTraits {

int inputCount(const QVariantMap& comp) {
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

int outputCount(const QVariantMap& comp) {
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

int outputBitWidth(const QVariantMap& comp, int portIdx) {
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

} // namespace ComponentTraits