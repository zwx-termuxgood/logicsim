#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 总线汇聚器继承 Definition 的组合逻辑角色，输入数量取 outputSplits 长度且至少一个，输出固定一个。
// 各输入宽度由 outputSplits 对应项决定、缺项回退一位；输出继承整体 bitWidth。
// 输入在 x=-40 按数量均布，输出居中在 (40,0)；hub 是持久化类型标识与注册表键。
// 文件末尾工厂由生成表显式引用，确保静态库链接保留本定义。
class HubDefinition final : public Definition {
public:
    QString typeId() const override { return "hub"; }
    int inputCount(const QVariantMap& c) const override { return qMax(1, c.value("outputSplits").toList().size()); }
    int outputCount(const QVariantMap&) const override { return 1; }
    int inputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "outputSplits", p, 1); }
    QVariantList inputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, false); }
    QVariantList outputPorts(const QVariantMap& c) const override { return singlePort(*this, c, true, 40); }
    // 默认按整体位宽生成每段一位的 outputSplits，再依实际分段端口初始化全零；子电路标识不用。
    void defaults(QVariantMap& c, const QString&) const override {
        QVariantList splits;
        for (int i = 0; i < width(c); ++i) splits.append(1);
        c["outputSplits"] = splits;
        zeroPorts(*this, c);
    }
    // in 为实际输入位串，bw 至少一位；splits 按输入索引指定长度，缺项或非正长度均按一位。
    // 历史拼接顺序为从最后一个输入到第零个，段内字符顺序不变；Z/E 原样保留，不作四态规约。
    // 每段短串在右侧补零、长串取左部，合并后同样右补零或取左 bw 位；输入空集合得到全零。
    // 计算遍历实际 in 而不是声明的输入数量，多余分段表项不生成数据，保留现有 hub 行为。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList& splits) const override {
        bw = qMax(1, bw);
        QString merged;
        for (int s = int(in.size()) - 1; s >= 0; --s) {
            const int len = qMax(1, s < splits.size() ? splits[s].toInt() : 1);
            QString part = in[s];
            while (part.size() < len) part += '0';
            merged += part.left(len);
        }
        while (merged.size() < bw) merged += '0';
        return {merged.left(bw)};
    }
};
}
std::unique_ptr<Definition> createHubDefinition() { return std::make_unique<HubDefinition>(); }
} // namespace Components
