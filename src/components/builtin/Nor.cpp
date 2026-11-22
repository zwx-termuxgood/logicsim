#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 或非门继承 GateDefinition 的组合逻辑角色：inputCount 限制为 2 至 64 个输入、一个输出。
// 各端口继承至少一位的 bitWidth 及左右侧布局，输入 x=-40、输出 x=40；默认端口值为零。
// 文件末尾工厂供生成表显式引用，避免静态库链接时丢弃该类型。
class NorDefinition final : public GateDefinition {
public:
    // nor 是持久化类型标识，也是查询注册表时的稳定键。
    QString typeId() const override { return "nor"; }
    // in 为输入位串集合，bw 至少为一，第三个分段参数不用；按相同字符索引逐位或规约后反相。
    // 短串缺位补零，超宽不参与；任一参与位为 Z/E 均输出 E，不因已有一而短路。
    // 返回唯一输出；空输入集合的或规约初值为假，反相后得到全一。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Or, true);
    }
};
}
std::unique_ptr<Definition> createNorDefinition() { return std::make_unique<NorDefinition>(); }
} // namespace Components
