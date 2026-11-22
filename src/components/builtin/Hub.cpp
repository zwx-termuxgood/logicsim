#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class HubDefinition final : public Definition {
public:
    QString typeId() const override { return "hub"; }
    int inputCount(const QVariantMap& c) const override { return qMax(1, c.value("outputSplits").toList().size()); }
    int outputCount(const QVariantMap&) const override { return 1; }
    int inputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "outputSplits", p, 1); }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false); }
    QVariantList outputPorts(const QVariantMap& c) const override { return singlePort(*this, c, true, 40); }
    void defaults(QVariantMap& c, const QString&) const override {
        QVariantList splits;
        for (int i = 0; i < width(c); ++i) splits.append(1);
        c["outputSplits"] = splits;
        zeroPorts(*this, c);
    }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList& splits) const override {
        bw = qMax(1, bw);
        QString merged;
        for (int s = int(in.size()) - 1; s >= 0; --s) {
            const int len = qMax(1, s < splits.size() ? splits[s].toInt() : 1);
            QString part = in[s];
            while (part.size() < len) part += '0';
            merged += part.left(len);
        }
        while (merged.size() < bw) merged += '0';
        return {merged.left(bw)};
    }
};
}
std::unique_ptr<Definition> createHubDefinition() { return std::make_unique<HubDefinition>(); }
} // namespace Components
