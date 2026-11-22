#include "ComponentDefinition.h"
#include <QMap>
#include <vector>

namespace Components {
// 普通端口至少为一位；分段端口另读宽度表，索引越界时保留调用方指定的回退值。
int width(const QVariantMap& c) { return qMax(1, c.value("bitWidth").toInt()); }
int listWidth(const QVariantMap& c, const char* key, int port, int fallback) {
    const auto values = c.value(key).toList();
    return port >= 0 && port < values.size() ? qMax(1, values[port].toInt()) : fallback;
}
int Definition::inputWidth(const QVariantMap& c, int) const { return width(c); }
int Definition::outputWidth(const QVariantMap& c, int) const { return width(c); }
void Definition::defaults(QVariantMap& c, const QString&) const { zeroPorts(*this, c); }
// 先确定端口数，再按各端口实际位宽创建全零位串；负数表示使用类型定义的数量。
// 这里写的是实例的存储字段，不改变端口几何或计算规则。
void zeroPorts(const Definition& d, QVariantMap& c, int inputs, int outputs) {
    QVariantList in, out;
    if (inputs < 0) inputs = d.inputCount(c);
    if (outputs < 0) outputs = d.outputCount(c);
    for (int p = 0; p < inputs; ++p) in.append(QString(d.inputWidth(c, p), '0'));
    for (int p = 0; p < outputs; ++p) out.append(QString(d.outputWidth(c, p), '0'));
    c["inputPortValues"] = in;
    c["outputPorts"] = out;
}
QVariantList singlePort(const Definition& d, const QVariantMap& c, bool output,
                        double x, double y, int index) {
    return {QVariantMap{{"index", index}, {"x", x}, {"y", y},
                       {"bitWidth", output ? d.outputWidth(c, index) : d.inputWidth(c, index)}}};
}
// 左侧为输入、右侧为输出；高度可由两侧较大的端口数决定，保证两侧使用相同外形。
// 单端口居中，多端口均匀分布；此处只生成未旋转的局部坐标。
QVariantList sidePorts(const Definition& d, const QVariantMap& c, bool output,
                       double w, double minimumHeight, int heightCount) {
    const int n = output ? d.outputCount(c) : d.inputCount(c);
    const double h = qMax(minimumHeight, double(heightCount < 0 ? n : heightCount) * 24.0);
    QVariantList ports;
    for (int i = 0; i < n; ++i)
        ports.append(singlePort(d, c, output, output ? w/2 : -w/2,
                                n == 1 ? 0 : -h/2 + (i + 0.5) * (h/n), i));
    return ports;
}
int GateDefinition::inputCount(const QVariantMap& c) const { return qBound(2, c.value("inputCount").toInt(), 64); }
QVariantList GateDefinition::inputPorts(const QVariantMap& c) const { return sidePorts(*this, c, false); }
QVariantList GateDefinition::outputPorts(const QVariantMap& c) const {
    return sidePorts(*this, c, true, 80, 60, qMax(inputCount(c), qMax(1, outputCount(c))));
}
QVariantList SourceDefinition::outputPorts(const QVariantMap& c) const {
    return singlePort(*this, c, true, qMax(60.0, c.value("bitWidth").toInt()*22.0 + 8.0)/2);
}
void TransistorDefinition::defaults(QVariantMap& c, const QString&) const {
    c["inputCount"] = 2;
    zeroPorts(*this, c);
}
// 逻辑计算缺少的字符/输入以零补齐；位索引由下方的非负循环提供。
QChar bitAt(const QString& s, int bit) { return bit < s.size() ? s[bit] : QChar('0'); }
QString input(const QVector<QString>& inputs, int index, int bw) {
    return index < inputs.size() ? inputs[index] : QString(bw, '0');
}
// 逐字符规约所有输入：与运算初值为真，或/异或初值为假，反相门最后统一取反。
// 任一输入位为高阻 Z 或错误 E，该输出位直接为 E，不以逻辑短路掩盖无效输入。
QVector<QString> reduce(const QVector<QString>& inputs, int bw, Reduction op, bool invert) {
    bw = qMax(1, bw);
    QString out;
    out.reserve(bw);
    for (int b = 0; b < bw; ++b) {
        bool error = false;
        bool r = op == Reduction::And;
        for (const auto& s : inputs) {
            const QChar c = bitAt(s, b);
            if (c == 'Z' || c == 'E') { error = true; break; }
            const bool v = c == '1';
            if (op == Reduction::And) r = r && v;
            else if (op == Reduction::Or) r = r || v;
            else r = r != v;
        }
        out += error ? 'E' : ((invert ? !r : r) ? '1' : '0');
    }
    return {out};
}
// 传输门的输入 0 为数据、输入 1 为使能；NMOS/PMOS 则是输入 0 为栅极、输入 1 为数据。
// PMOS 低电平导通，其余高电平导通；未知使能输出 E，关闭时输出 Z，导通时原样传递数据。
QVector<QString> switchOutputs(const QVector<QString>& inputs, int bw, Switch mode) {
    bw = qMax(1, bw);
    const auto a = input(inputs, 0, bw), b = input(inputs, 1, bw);
    QString out;
    for (int bit = 0; bit < bw; ++bit) {
        const QChar gate = bitAt(mode == Switch::Transmission ? b : a, bit);
        const QChar data = bitAt(mode == Switch::Transmission ? a : b, bit);
        if (gate == 'Z' || gate == 'E') out += 'E';
        else if (gate == (mode == Switch::Pmos ? '0' : '1')) out += data;
        else out += 'Z';
    }
    return {out};
}

// CMake 生成的显式工厂调用会形成真实符号引用，使静态库链接保留每种定义，
// 不依赖全局构造函数执行自注册。
#include "BuiltinFactories.inc"
namespace {
// 未知类型仍返回安全定义（无端口、无组合输出），使查询接口无需返回空指针。
class UnknownDefinition final : public Definition {
    QString typeId() const override { return {}; }
    Role role() const override { return Role::Unknown; }
};
// owned 独占定义对象，byId 仅建立只读索引；对象地址随注册表保持有效。
// 空 ID 或重复 ID 属于开发期注册错误，立即终止，避免悄悄覆盖另一类型。
struct Registry {
    std::vector<std::unique_ptr<Definition>> owned;
    QMap<QString, const Definition*> byId;
    Registry() {
        owned = builtinDefinitions();
        for (const auto& d : owned) {
            const auto id = d->typeId();
            if (id.isEmpty() || byId.contains(id)) qFatal("Duplicate or empty component typeId");
            byId.insert(id, d.get());
        }
    }
};
// 函数局部静态对象在首次查询时初始化；不依赖不同编译单元间的全局初始化顺序。
const Registry& registry() { static const Registry r; return r; }
}
const Definition& definition(const QString& id) {
    static const UnknownDefinition unknown;
    const auto* d = registry().byId.value(id, nullptr);
    return d ? *d : unknown;
}
QStringList typeIds() { return registry().byId.keys(); }
} // namespace Components
