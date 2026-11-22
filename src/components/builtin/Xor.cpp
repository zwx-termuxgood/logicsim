#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 异或门继承 GateDefinition 的组合逻辑角色：inputCount 限制为 2 至 64 个输入、一个输出。
// 各端口沿用 bitWidth 位宽及左右侧布局，输入 x=-40、输出 x=40；默认按端口数量填零。
// 文件末尾工厂由生成表显式引用，使静态库链接保留本类型，无需全局自注册。
class XorDefinition final : public GateDefinition {
public:
    // xor 为注册表键及持久化类型标识，读取保存的元件时据此恢复定义。
    QString typeId() const override { return "xor"; }
    // in 是输入位串集合，bw 至少为一，第三个分段参数不用；同字符索引逐位异或，表示一的奇偶性。
    // 短串缺位补零，超宽忽略；任一参与位为 Z/E 时输出 E，不将其当成普通零。
    // 返回唯一输出；空输入集合初值为零，false 表示不反相。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Xor, false);
    }
};
}
std::unique_ptr<Definition> createXorDefinition() { return std::make_unique<XorDefinition>(); }
} // namespace Components
