#include "components/ComponentDefinition.h"

namespace Components {
namespace {
class TextDefinition final : public Definition {
public:
    QString typeId() const override { return "text"; }
    Role role() const override { return Role::Text; }
    void defaults(QVariantMap& c, const QString&) const override {
        c["content"] = "文本";
        zeroPorts(*this, c);
    }
};
}
std::unique_ptr<Definition> createTextDefinition() { return std::make_unique<TextDefinition>(); }
} // namespace Components
