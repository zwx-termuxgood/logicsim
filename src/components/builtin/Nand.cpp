#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 与非门继承 GateDefinition 的组合逻辑角色：inputCount 限制为 2 至 64 个输入、一个输出。
// 各端口继承 bitWidth 位宽及左右侧布局，输入 x=-40、输出 x=40；默认按端口数量填零。
// 文件末尾工厂由生成表显式引用，使静态库链接保留本定义，无需全局自注册。
class NandDefinition final : public GateDefinition {
public:
    // nand 同时是注册表键和持久化类型标识，保证已保存元件仍能匹配此定义。
    QString typeId() const override { return "nand"; }
    // in 为输入位串集合，bw 至少为一，第三个分段参数不用；按字符索引逐位与规约后反相。
    // 缺位补零、超宽截去；任一参与位为 Z/E 就输出 E，反相不会改变错误态。
    // 返回一个输出位串；空输入集合以真开始规约，反相后得到全零。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::And, true);
    }
};
}
std::unique_ptr<Definition> createNandDefinition() { return std::make_unique<NandDefinition>(); }
} // namespace Components
