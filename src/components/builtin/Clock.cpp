#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class ClockDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "clock"; }
    Role role() const override { return Role::Clock; }
    void defaults(QVariantMap& c, const QString&) const override {
        c["name"] = "CLK";
        c["outputPorts"] = QVariantList();
        c["inputPortValues"] = QVariantList();
    }
};
}
std::unique_ptr<Definition> createClockDefinition() { return std::make_unique<ClockDefinition>(); }
} // namespace Components
