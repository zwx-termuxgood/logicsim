#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 指示灯使用 Role::Led；在 Definition 的零输出基础上增加一个输入，输入宽度继承 bitWidth。
// 输入位于显示外形左边缘 (-max(50,bitWidth*22+8)/2,0)，不同于普通输出元件的最小宽度 60。
// 未覆写 compute，继承空结果且三个参数不用；仅观察输入，不在此改变四态或位序。
// 默认继承 zeroPorts：一个全零输入、空输出表；typeId 的 led 是持久化标识与注册表键。
// 文件末尾工厂由生成表显式引用，使静态库保留本类型而无需全局自注册。
class LedDefinition final : public Definition {
public:
    QString typeId() const override { return "led"; }
    Role role() const override { return Role::Led; }
    int inputCount(const QVariantMap&) const override { return 1; }
    QVariantList inputPorts(const QVariantMap& c) const override {
        return singlePort(*this, c, false, -qMax(50.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
    }
};
}
std::unique_ptr<Definition> createLedDefinition() { return std::make_unique<LedDefinition>(); }
} // namespace Components
