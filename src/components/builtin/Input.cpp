#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class InputDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "input"; }
    Role role() const override { return Role::Input; }
};
}
std::unique_ptr<Definition> createInputDefinition() { return std::make_unique<InputDefinition>(); }
} // namespace Components
