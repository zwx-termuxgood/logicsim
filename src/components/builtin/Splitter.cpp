#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class SplitterDefinition final : public Definition {
public:
    QString typeId() const override { return "splitter"; }
    int inputCount(const QVariantMap&) const override { return 1; }
    int outputCount(const QVariantMap& c) const override { return qMax(1, c.value("outputSplits").toList().size()); }
    int outputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "outputSplits", p, 1); }
    QVariantList inputPorts(const QVariantMap& c) const override { return singlePort(*this, c, false, -40); }
    QVariantList outputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, true); }
    void defaults(QVariantMap& c, const QString&) const override {
        QVariantList splits;
        for (int i = 0; i < width(c); ++i) splits.append(1);
        c["outputSplits"] = splits;
        zeroPorts(*this, c);
    }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList& splits) const override {
        bw = qMax(1, bw);
        QString main = input(in, 0, bw);
        while (main.size() < bw) main += '0';
        main = main.left(bw);
        int offset = bw;
        QVector<QString> out;
        for (const auto& v : splits) {
            const int len = qMax(1, v.toInt());
            offset = qMax(0, offset - len);
            QString part;
            for (int b = 0; b < len; ++b) part += bitAt(main, offset + b);
            out.append(part);
        }
        return out;
    }
};
}
std::unique_ptr<Definition> createSplitterDefinition() { return std::make_unique<SplitterDefinition>(); }
} // namespace Components
