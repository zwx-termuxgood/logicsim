#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class OrDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "or"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Or, false);
    }
};
}
std::unique_ptr<Definition> createOrDefinition() { return std::make_unique<OrDefinition>(); }
} // namespace Components
