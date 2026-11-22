#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 与门沿用 GateDefinition 的组合逻辑角色：输入数量由 inputCount 限制在 2 至 64，输出固定一个。
// 端口继承左右侧布局，输入位于 x=-40、输出位于 x=40；各端口宽度均为至少一位的 bitWidth。
// 默认值继承零值端口初始化；文件末尾工厂供生成表显式引用，确保静态库链接保留本类型。
class AndDefinition final : public GateDefinition {
public:
    // 持久化类型标识与注册表键一致，读取已保存的 and 元件时据此恢复定义。
    QString typeId() const override { return "and"; }
    // in 是各输入位串，bw 至少按一位计算，第三个分段参数不使用；按相同字符索引逐位与运算。
    // 短串缺位补零、超宽字符不参与；任一参与位为 Z 或 E 均输出 E，不以零短路掩盖错误。
    // 返回唯一输出位串；空输入集合的与规约初值为一，false 表示不再反相。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        return reduce(in, bw, Reduction::And, false);
    }
};
}
std::unique_ptr<Definition> createAndDefinition() { return std::make_unique<AndDefinition>(); }
} // namespace Components
