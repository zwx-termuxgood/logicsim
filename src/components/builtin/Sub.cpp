#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 子电路实例继承 Definition 并标记 Role::Subcircuit，端口数量分别取输入、输出名称表长度，可为零。
// 每端宽度读对应宽度表并至少为一，缺项回退为整体 bitWidth；两侧局部布局宽 110，输入 x=-55、输出 x=55。
// 输入侧高度按输入数量，输出侧按两侧较大数量计算，均至少高 60；不在此改变历史上的两侧高度差异。
// 不覆写 compute：三个参数均不用且返回空结果，子电路执行由外部负责，本定义不变换四态或位序。
// sub 是持久化类型标识及注册表键，具体子电路由 subId 引用；末尾工厂供生成表显式引用以保留静态链接。
class SubDefinition final : public Definition {
public:
    QString typeId() const override { return "sub"; }
    Role role() const override { return Role::Subcircuit; }
    int inputCount(const QVariantMap& c) const override { return c.value("subInputNames").toList().size(); }
    int outputCount(const QVariantMap& c) const override { return c.value("subOutputNames").toList().size(); }
    int inputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "subInputWidths", p, width(c)); }
    int outputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "subOutputWidths", p, width(c)); }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false, 110); }
    QVariantList outputPorts(const QVariantMap& c) const override {
        return sidePorts(*this, c, true, 110, 60, qMax(inputCount(c), qMax(1, outputCount(c))));
    }
    // 保存传入 subId，清空名称和宽度四张表，再按当前零端口初始化空值表，等待外部补全接口。
    void defaults(QVariantMap& c, const QString& subId) const override {
        c["subId"] = subId;
        for (const auto key : {"subInputNames", "subOutputNames", "subInputWidths", "subOutputWidths"}) c[key] = QVariantList();
        zeroPorts(*this, c);
    }
};
}
std::unique_ptr<Definition> createSubDefinition() { return std::make_unique<SubDefinition>(); }
} // namespace Components
