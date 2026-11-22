#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 时钟源以 Role::Clock 标识，继承 SourceDefinition 的零输入、一个输出，而非本地计算振荡。
// 输出继承 bitWidth 位宽，局部位置为 (max(60,bitWidth*22+8)/2,0)，由统一逻辑旋转。
// 未覆写 compute，继承空结果且不使用三个计算参数；本文件不变换四态或位序，时序由外部处理。
// typeId 的 clock 是持久化类型标识与注册表键，保证重新加载时匹配时钟定义。
// 文件末尾工厂供生成表显式引用，确保静态库链接保留该类型。
class ClockDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "clock"; }
    Role role() const override { return Role::Clock; }
    // 默认名称为 CLK；特意保存空输出值表及空输入值表，不按几何上的一个输出预填零；子电路标识不用。
    void defaults(QVariantMap& c, const QString&) const override {
        c["name"] = "CLK";
        c["outputPorts"] = QVariantList();
        c["inputPortValues"] = QVariantList();
    }
};
}
std::unique_ptr<Definition> createClockDefinition() { return std::make_unique<ClockDefinition>(); }
} // namespace Components
