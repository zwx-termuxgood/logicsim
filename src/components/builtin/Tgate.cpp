#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 传输门继承 TransistorDefinition 的组合逻辑角色、两个逻辑输入和一个输出，位宽均继承 bitWidth。
// 本类补充左右侧布局：两个输入在 x=-40 均布，一个输出居中位于 (40,0)，存储数量不改变此布局。
// tgate 是持久化类型标识及注册表键；末尾工厂由生成表显式引用，保证静态库链接保留定义。
class TgateDefinition final : public TransistorDefinition {
public:
    QString typeId() const override { return "tgate"; }
    // in 零号为数据、一号为使能，多余输入不参与；bw 至少一位，第三个分段参数不用，按字符索引计算。
    // 缺输入或短串补零、超宽忽略；使能 Z/E 输出 E，使能一原样传递数据（含 Z/E），否则输出 Z。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return switchOutputs(in, bw, Switch::Transmission);
    }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false); }
    QVariantList outputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, true); }
    void defaults(QVariantMap& c, const QString&) const override {
        // 保留历史数据格式中的三个已存输入值；逻辑上晶体管仍只有两个输入端口。
        c["inputCount"] = 3;
        zeroPorts(*this, c, 3);
    }
};
}
std::unique_ptr<Definition> createTgateDefinition() { return std::make_unique<TgateDefinition>(); }
} // namespace Components
