#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 或门继承 GateDefinition 的组合逻辑角色：inputCount 限制为 2 至 64 个输入、一个输出。
// 继承左右侧端口布局，输入 x=-40、输出 x=40，位宽取至少一位的 bitWidth；默认填零。
// 文件末尾工厂由生成表显式引用，保证静态库链接保留此类型而非依赖全局自注册。
class OrDefinition final : public GateDefinition {
public:
    // or 是注册表键及持久化类型标识，保存和读取电路必须使用同一标识。
    QString typeId() const override { return "or"; }
    // in 为各输入位串，bw 至少为一，第三个分段参数不用；按相同字符索引逐位或规约。
    // 缺位补零、超宽忽略；任一参与位为 Z/E 则输出 E，即使其他输入已为一也不短路。
    // 返回一个输出位串，空集合初值为零；false 表示不反相。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Or, false);
    }
};
}
std::unique_ptr<Definition> createOrDefinition() { return std::make_unique<OrDefinition>(); }
} // namespace Components
