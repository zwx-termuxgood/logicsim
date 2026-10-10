#include "PortGeometry.h"
#include "ComponentTraits.h"
#include <QtGlobal>

namespace PortGeometry {

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
    int n = ComponentTraits::outputCount(comp);
    int rot = comp.value("rotation").toInt();

    double W = 80.0, H = 60.0;

    if (t == "input" || t == "clock" || t == "const") {
        W = qMax(60.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = ComponentTraits::outputBitWidth(comp, 0);
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
            p["bitWidth"] = ComponentTraits::outputBitWidth(comp, i);
            result.append(p);
        }
    } else if (t == "hub") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = W/2; p["y"] = 0;
        p["bitWidth"] = ComponentTraits::outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = -H/4;
        p["bitWidth"] = ComponentTraits::outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0;
        p["x"] = W/2;
        p["y"] = H/4;
        p["bitWidth"] = ComponentTraits::outputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "not" || t == "and" || t == "or" || t == "nand" ||
               t == "nor" || t == "xor" || t == "xnor" || t == "sub" ||
               t == "tgate") {
        W = (t == "sub") ? 110.0 : 80.0;
        int ic = ComponentTraits::inputCount(comp);
        int oc = n; if (oc < 1) oc = 1;
        H = qMax(60.0, (double)qMax(ic, oc) * 24.0);
        for (int i = 0; i < n; ++i) {
            QVariantMap p;
            p["index"] = i;
            p["x"] = W/2;
            p["y"] = n == 1 ? 0.0 : (-H/2 + (i + 0.5) * (H / n));
            p["bitWidth"] = ComponentTraits::outputBitWidth(comp, i);
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
    int n = ComponentTraits::inputCount(comp);
    int rot = comp.value("rotation").toInt();

    double W = 80.0, H = 60.0;

    if (t == "led" || t == "output") {
        W = qMax(t == "output" ? 60.0 : 50.0, comp.value("bitWidth").toInt() * 22.0 + 8.0);
        H = 40.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = ComponentTraits::inputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "splitter") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = ComponentTraits::inputBitWidth(comp, 0);
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
            p["bitWidth"] = ComponentTraits::inputBitWidth(comp, i);
            result.append(p);
        }
    } else if (t == "not") {
        W = 80.0; H = 60.0;
        QVariantMap p;
        p["index"] = 0; p["x"] = -W/2; p["y"] = 0;
        p["bitWidth"] = ComponentTraits::inputBitWidth(comp, 0);
        result.append(p);
    } else if (t == "ntran") {
        W = 80.0; H = 60.0;
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = ComponentTraits::inputBitWidth(comp, 0);
        result.append(pg);

        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = H/4;
        pd["bitWidth"] = ComponentTraits::inputBitWidth(comp, 1);
        result.append(pd);
    } else if (t == "ptran") {
        W = 80.0; H = 60.0;
        QVariantMap pg;
        pg["index"] = 0;
        pg["x"] = -W/2; pg["y"] = 0;
        pg["bitWidth"] = ComponentTraits::inputBitWidth(comp, 0);
        result.append(pg);

        QVariantMap pd;
        pd["index"] = 1;
        pd["x"] = W/2;
        pd["y"] = -H/4;
        pd["bitWidth"] = ComponentTraits::inputBitWidth(comp, 1);
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
            p["bitWidth"] = ComponentTraits::inputBitWidth(comp, i);
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

} // namespace PortGeometry