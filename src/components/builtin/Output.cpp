#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 外部输出观察端使用 Role::Output；继承 Definition 的零输出并提供一个 bitWidth 宽输入。
// 输入在显示外形左边缘 (-max(60,bitWidth*22+8)/2,0)，位宽至少一位，坐标再由统一逻辑旋转。
// 未覆写 compute，继承空结果且三个参数不用；不在此执行四态运算或位序转换。
// 默认继承 zeroPorts：一个全零输入、空输出表；typeId 的 output 是持久化标识及注册表键。
// 文件末尾工厂由生成表显式引用，保证静态库链接保留此定义。
class OutputDefinition final : public Definition {
public:
    QString typeId() const override { return "output"; }
    Role role() const override { return Role::Output; }
    int inputCount(const QVariantMap&) const override { return 1; }
    QVariantList inputPorts(const QVariantMap& c) const override {
        return singlePort(*this, c, false, -qMax(60.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
    }
};
}
std::unique_ptr<Definition> createOutputDefinition() { return std::make_unique<OutputDefinition>(); }
} // namespace Components
