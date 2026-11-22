#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class OutputDefinition final : public Definition {
public:
    QString typeId() const override { return "output"; }
    Role role() const override { return Role::Output; }
    int inputCount(const QVariantMap&) const override { return 1; }
    QVariantList inputPorts(const QVariantMap& c) const override {
        return singlePort(*this, c, false, -qMax(60.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
    }
};
}
std::unique_ptr<Definition> createOutputDefinition() { return std::make_unique<OutputDefinition>(); }
} // namespace Components
