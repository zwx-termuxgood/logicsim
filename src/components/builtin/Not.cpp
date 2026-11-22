#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class NotDefinition final : public GateDefinition {
public:
    QString typeId() const override { return "not"; }
    int inputCount(const QVariantMap&) const override { return 1; }
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        bw = qMax(1, bw);
        const auto a = input(in, 0, bw);
        QString out;
        for (int b = 0; b < bw; ++b) {
            const auto c = bitAt(a, b);
            out += (c == 'Z' || c == 'E') ? 'E' : (c == '1' ? '0' : '1');
        }
        return {out};
    }
};
}
std::unique_ptr<Definition> createNotDefinition() { return std::make_unique<NotDefinition>(); }
} // namespace Components
