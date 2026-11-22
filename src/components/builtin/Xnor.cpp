#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class XnorDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "xnor"; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Xor, true);
    }
};
}
std::unique_ptr<Definition> createXnorDefinition() { return std::make_unique<XnorDefinition>(); }
} // namespace Components
