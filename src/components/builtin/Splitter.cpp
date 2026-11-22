#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 总线分线器继承 Definition 的组合逻辑角色，一个输入，输出数量取 outputSplits 长度且至少一个。
// 输入宽度继承整体 bitWidth，输出宽度读分段表且至少一位，缺项回退一位；输入 (-40,0)，输出 x=40 均布。
// splitter 是持久化类型标识及注册表键；末尾工厂由生成表显式引用，确保静态库链接保留本类型。
class SplitterDefinition final : public Definition {
public:
    QString typeId() const override { return "splitter"; }
    int inputCount(const QVariantMap&) const override { return 1; }
    int outputCount(const QVariantMap& c) const override { return qMax(1, c.value("outputSplits").toList().size()); }
    int outputWidth(const QVariantMap& c, int p) const override { return listWidth(c, "outputSplits", p, 1); }
    QVariantList inputPorts(const QVariantMap& c) const override { return singlePort(*this, c, false, -40); }
    QVariantList outputPorts(const QVariantMap& c) const override { return sidePorts(*this, c, true); }
    // 默认按整体位宽生成每段一位的 outputSplits，并按实际端口宽度初始化全零；子电路标识不用。
    void defaults(QVariantMap& c, const QString&) const override {
        QVariantList splits;
        for (int i = 0; i < width(c); ++i) splits.append(1);
        c["outputSplits"] = splits;
        zeroPorts(*this, c);
    }
    // in 只取第零个主输入，bw 至少一位，缺输入或短串在右侧补零、长串取左 bw 位；Z/E 按字符保留。
    // splits 每项生成一个输出，段长至少一位；从 offset=bw 开始先减段长，再按递增字符索引取段。
    // 因此先取主串靠右的段，段内不反转；越界字符补零，offset 不低于零，过量分段可能重复取左部。
    // splits 为空时返回空集合，虽然几何至少有一个输出；不修正历史 splitter 的分段及位序行为。
    QVector<QString> compute(const QVector<QString>& in, int bw, const QVariantList& splits) const override {
        bw = qMax(1, bw);
        QString main = input(in, 0, bw);
        while (main.size() < bw) main += '0';
        main = main.left(bw);
        int offset = bw;
        QVector<QString> out;
        for (const auto& v : splits) {
            const int len = qMax(1, v.toInt());
            offset = qMax(0, offset - len);
            QString part;
            for (int b = 0; b < len; ++b) part += bitAt(main, offset + b);
            out.append(part);
        }
        return out;
    }
};
}
std::unique_ptr<Definition> createSplitterDefinition() { return std::make_unique<SplitterDefinition>(); }
} // namespace Components
