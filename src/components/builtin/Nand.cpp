#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class NandDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "nand"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::And, true);
    }
};
}
std::unique_ptr<Definition> createNandDefinition() { return std::make_unique<NandDefinition>(); }
} // namespace Components
