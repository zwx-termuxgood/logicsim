#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class AndDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "and"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::And, false);
    }
};
}
std::unique_ptr<Definition> createAndDefinition() { return std::make_unique<AndDefinition>(); }
} // namespace Components
