#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class NtranDefinition final : public TransistorDefinition {
public:
    QString typeId() const override { return "ntran"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return switchOutputs(in, bw, Switch::Nmos);
    }
    QVariantList outputPorts(const QVariantMap& c) const override { return singlePort(*this, c, true, 40, -15); }
    QVariantList inputPorts(const QVariantMap& c) const override {
        auto ports = singlePort(*this, c, false, -40);
        ports.append(singlePort(*this, c, false, 40, 15, 1));
        return ports;
    }
};
}
std::unique_ptr<Definition> createNtranDefinition() { return std::make_unique<NtranDefinition>(); }
} // namespace Components
