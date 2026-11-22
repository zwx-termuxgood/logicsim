#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class ConstDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "const"; }
    Role role() const override { return Role::Constant; }
    void defaults(QVariantMap& c, const QString&) const override {
        c["name"] = "CONST";
        c["outputPorts"] = QVariantList();
        c["inputPortValues"] = QVariantList();
    }
};
}
std::unique_ptr<Definition> createConstDefinition() { return std::make_unique<ConstDefinition>(); }
} // namespace Components
