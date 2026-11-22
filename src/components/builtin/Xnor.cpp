#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 同或门继承 GateDefinition 的组合逻辑角色：inputCount 限制为 2 至 64 个输入、一个输出。
// 端口沿用左右侧布局，输入 x=-40、输出 x=40，位宽为至少一位的 bitWidth；默认值填零。
// 文件末尾工厂供生成表显式引用，静态库链接无需依赖全局构造自注册。
class XnorDefinition final : public GateDefinition {
public:
    // xnor 是持久化类型标识，也是加载元件时查找定义的注册表键。
    QString typeId() const override { return "xnor"; }
    // in 是各输入位串，bw 至少为一，第三个分段参数不用；按字符索引异或规约后反相。
    // 多输入计算的是奇偶性的反相，并非两两相等判断；短串补零，超宽忽略，任一 Z/E 输出 E。
    // 返回唯一输出；空集合的异或初值为零，反相得到全一，错误态不参与反相。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::Xor, true);
    }
};
}
std::unique_ptr<Definition> createXnorDefinition() { return std::make_unique<XnorDefinition>(); }
} // namespace Components
