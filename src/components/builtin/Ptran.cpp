#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// PMOS 开关继承 TransistorDefinition：组合逻辑角色、两个输入、一个输出，所有端口均继承 bitWidth。
// 默认继承 inputCount=2 并初始化零值；ptran 是持久化类型标识及注册表键。
// 独有布局为输入零（栅极）在 (-40,0)、输入一（数据）在 (40,-15)，输出在 (40,15)。
// 文件末尾工厂由生成表显式引用，使静态库链接保留本类型。
class PtranDefinition final : public TransistorDefinition {
public:
    QString typeId() const override { return "ptran"; }
    // in 零号是栅极、一号是数据；bw 至少一位，第三个分段参数不用，按相同字符索引逐位开关。
    // 缺输入或短串补零，超宽忽略；栅极 Z/E 输出 E，栅极零原样传递数据（含 Z/E），否则输出 Z。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return switchOutputs(in, bw, Switch::Pmos);
    }
    QVariantList outputPorts(const QVariantMap& c) const override { return singlePort(*this, c, true, 40, 15); }
    QVariantList inputPorts(const QVariantMap& c) const override {
        auto ports = singlePort(*this, c, false, -40);
        ports.append(singlePort(*this, c, false, 40, -15, 1));
        return ports;
    }
};
}
std::unique_ptr<Definition> createPtranDefinition() { return std::make_unique<PtranDefinition>(); }
} // namespace Components
