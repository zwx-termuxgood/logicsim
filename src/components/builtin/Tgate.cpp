#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class TgateDefinition final : public TransistorDefinition {
public:
    QString typeId() const override { return "tgate"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return switchOutputs(in, bw, Switch::Transmission);
    }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false); }
    QVariantList outputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, true); }
    void defaults(QVariantMap& c, const QString&) const override {
        // Preserve the historical three stored values despite two logical inputs.
        c["inputCount"] = 3;
        zeroPorts(*this, c, 3);
    }
};
}
std::unique_ptr<Definition> createTgateDefinition() { return std::make_unique<TgateDefinition>(); }
} // namespace Components
