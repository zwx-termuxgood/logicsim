#include "ComponentDefinition.h"
#include <QMap>
#include <vector>

namespace Components {
int width(const QVariantMap& c) { return qMax(1, c.value("bitWidth").toInt()); }
int listWidth(const QVariantMap& c, const char* key, int port, int fallback) {
    const auto values = c.value(key).toList();
    return port >= 0 && port < values.size() ? qMax(1, values[port].toInt()) : fallback;
}
int Definition::inputWidth(const QVariantMap& c, int) const { return width(c); }
int Definition::outputWidth(const QVariantMap& c, int) const { return width(c); }
void Definition::defaults(QVariantMap& c, const QString&) const { zeroPorts(*this, c); }
void zeroPorts(const Definition& d, QVariantMap& c, int inputs, int outputs) {
    QVariantList in, out;
    if (inputs < 0) inputs = d.inputCount(c);
    if (outputs < 0) outputs = d.outputCount(c);
    for (int p = 0; p < inputs; ++p) in.append(QString(d.inputWidth(c, p), '0'));
    for (int p = 0; p < outputs; ++p) out.append(QString(d.outputWidth(c, p), '0'));
    c["inputPortValues"] = in;
    c["outputPorts"] = out;
}
QVariantList singlePort(const Definition& d, const QVariantMap& c, bool output,
                        double x, double y, int index) {
    return {QVariantMap{{"index", index}, {"x", x}, {"y", y},
                       {"bitWidth", output ? d.outputWidth(c, index) : d.inputWidth(c, index)}}};
}
QVariantList sidePorts(const Definition& d, const QVariantMap& c, bool output,
                       double w, double minimumHeight, int heightCount) {
    const int n = output ? d.outputCount(c) : d.inputCount(c);
    const double h = qMax(minimumHeight, double(heightCount < 0 ? n : heightCount) * 24.0);
    QVariantList ports;
    for (int i = 0; i < n; ++i)
        ports.append(singlePort(d, c, output, output ? w/2 : -w/2,
                                n == 1 ? 0 : -h/2 + (i + 0.5) * (h/n), i));
    return ports;
}
int GateDefinition::inputCount(const QVariantMap& c) const { return qBound(2, c.value("inputCount").toInt(), 64); }
QVariantList GateDefinition::inputPorts(const QVariantMap& c) const { return sidePorts(*this, c, false); }
QVariantList GateDefinition::outputPorts(const QVariantMap& c) const {
    return sidePorts(*this, c, true, 80, 60, qMax(inputCount(c), qMax(1, outputCount(c))));
}
QVariantList SourceDefinition::outputPorts(const QVariantMap& c) const {
    return singlePort(*this, c, true, qMax(60.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
}
void TransistorDefinition::defaults(QVariantMap& c, const QString&) const {
    c["inputCount"] = 2;
    zeroPorts(*this, c);
}
QChar bitAt(const QString& s, int bit) { return bit < s.size() ? s[bit] : QChar('0'); }
QString input(const QVector<QString>& inputs, int index, int bw) {
    return index < inputs.size() ? inputs[index] : QString(bw, '0');
}
QVector<QString> reduce(const QVector<QString>& inputs, int bw, Reduction op, bool invert) {
    bw = qMax(1, bw);
    QString out;
    out.reserve(bw);
    for (int b = 0; b < bw; ++b) {
        bool error = false;
        bool r = op == Reduction::And;
        for (const auto& s : inputs) {
            const QChar c = bitAt(s, b);
            if (c == 'Z' || c == 'E') { error = true; break; }
            const bool v = c == '1';
            if (op == Reduction::And) r = r && v;
            else if (op == Reduction::Or) r = r || v;
            else r = r != v;
        }
        out += error ? 'E' : ((invert ? !r : r) ? '1' : '0');
    }
    return {out};
}
QVector<QString> switchOutputs(const QVector<QString>& inputs, int bw, Switch mode) {
    bw = qMax(1, bw);
    const auto a = input(inputs, 0, bw), b = input(inputs, 1, bw);
    QString out;
    for (int bit = 0; bit < bw; ++bit) {
        const QChar gate = bitAt(mode == Switch::Transmission ? b : a, bit);
        const QChar data = bitAt(mode == Switch::Transmission ? a : b, bit);
        if (gate == 'Z' || gate == 'E') out += 'E';
        else if (gate == (mode == Switch::Pmos ? '0' : '1')) out += data;
        else out += 'Z';
    }
    return {out};
}

// Generated calls are real symbol references: every definition is retained when
// this library is linked statically, without relying on global constructors.
#include "BuiltinFactories.inc"
namespace {
class UnknownDefinition final : public Definition {
    QString typeId() const override { return {}; }
    Role role() const override { return Role::Unknown; }
};
struct Registry {
    std::vector<std::unique_ptr<Definition>> owned;
    QMap<QString, const Definition*> byId;
    Registry() {
        owned = builtinDefinitions();
        for (const auto& d : owned) {
            const auto id = d->typeId();
            if (id.isEmpty() || byId.contains(id)) qFatal("Duplicate or empty component typeId");
            byId.insert(id, d.get());
        }
    }
};
const Registry& registry() { static const Registry r; return r; }
}
const Definition& definition(const QString& id) {
    static const UnknownDefinition unknown;
    const auto* d = registry().byId.value(id, nullptr);
    return d ? *d : unknown;
}
QStringList typeIds() { return registry().byId.keys(); }
} // namespace Components
