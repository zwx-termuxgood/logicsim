#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class SubDefinition final : public Definition {
public:
    QString typeId() const override { return "sub"; }
    Role role() const override { return Role::Subcircuit; }
    int inputCount(const QVariantMap& c) const override { return c.value("subInputNames").toList().size(); }
    int outputCount(const QVariantMap& c) const override { return c.value("subOutputNames").toList().size(); }
    int inputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "subInputWidths", p, width(c)); }
    int outputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "subOutputWidths", p, width(c)); }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false, 110); }
    QVariantList outputPorts(const QVariantMap& c) const override {
        return sidePorts(*this, c, true, 110, 60, qMax(inputCount(c), qMax(1, outputCount(c))));
    }
    void defaults(QVariantMap& c, const QString& subId) const override {
        c["subId"] = subId;
        for (const auto key : {"subInputNames", "subOutputNames", "subInputWidths", "subOutputWidths"}) c[key] = QVariantList();
        zeroPorts(*this, c);
    }
};
}
std::unique_ptr<Definition> createSubDefinition() { return std::make_unique<SubDefinition>(); }
} // namespace Components
