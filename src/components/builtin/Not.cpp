#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 反相器继承 GateDefinition 的组合逻辑角色及左右侧布局，但强制一个输入，不采用多输入下限。
// 输出仍为一个，输入在 (-40,0)、输出在 (40,0)，两端宽度均为至少一位的 bitWidth。
// 默认继承零值端口初始化；文件末尾工厂通过生成表的显式引用进入静态库链接。
class NotDefinition final : public GateDefinition {
public:
    // not 是持久化类型标识与注册表键，不因显示名称变化而改变。
    QString typeId() const override { return "not"; }
    int inputCount(const QVariantMap&) const override { return 1; }
    // in 只读第零个输入，bw 至少为一，第三个分段参数不用；缺输入或短串按零处理，超宽忽略。
    // 按字符索引逐位输出：Z/E 变 E，一变零，其余字符变一；不调整位序，返回唯一输出。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList&) const override {
        bw = qMax(1, bw);
        const auto a = input(in, 0, bw);
        QString out;
        for (int b = 0; b < bw; ++b) {
            const auto c = bitAt(a, b);
            out += (c == 'Z' || c == 'E') ? 'E' : (c == '1' ? '0' : '1');
        }
        return {out};
    }
};
}
std::unique_ptr<Definition> createNotDefinition() { return std::make_unique<NotDefinition>(); }
} // namespace Components
