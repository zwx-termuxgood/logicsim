#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class LedDefinition final : public Definition {
public:
    QString typeId() const override { return "led"; }
    Role role() const override { return Role::Led; }
    int inputCount(const QVariantMap&) const override { return 1; }
    QVariantList inputPorts(const QVariantMap& c) const override {
        return singlePort(*this, c, false, -qMax(50.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
    }
};
}
std::unique_ptr<Definition> createLedDefinition() { return std::make_unique<LedDefinition>(); }
} // namespace Components
