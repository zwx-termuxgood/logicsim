#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class NorDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "nor"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Or, true);
    }
};
}
std::unique_ptr<Definition> createNorDefinition() { return std::make_unique<NorDefinition>(); }
} // namespace Components
