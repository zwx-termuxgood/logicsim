#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 常量源以 Role::Constant 标识，继承 SourceDefinition 的零输入、一个输出。
// 输出位宽继承至少一位的 bitWidth，局部位置为 (max(60,bitWidth*22+8)/2,0)。
// 未覆写 compute，三个计算参数均不用且返回空结果；常量由外部角色逻辑提供，本文件不变换四态或位序。
// typeId 返回的 const 是持久化类型标识及注册表键，与显示名称 CONST 分离。
// 文件末尾工厂供生成表显式引用，避免静态库链接丢弃本类型。
class ConstDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "const"; }
    Role role() const override { return Role::Constant; }
    // 默认名称为 CONST，输入与输出值表均显式置空；并非按一个输出初始化零值，子电路标识不用。
    void defaults(QVariantMap& c, const QString&) const override {
        c["name"] = "CONST";
        c["outputPorts"] = QVariantList();
        c["inputPortValues"] = QVariantList();
    }
};
}
std::unique_ptr<Definition> createConstDefinition() { return std::make_unique<ConstDefinition>(); }
} // namespace Components
