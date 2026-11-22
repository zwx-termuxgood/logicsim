#include "ComponentLogic.h"
#include "core/Grid.h"

namespace ComponentLogic {
namespace {
// 元件类型的全部差异由注册表中的定义对象承担；此处不再维护第二份类型分支表。
const Components::Definition& definition(const QVariantMap& c) {
    return Components::definition(c.value("type").toString());
}
// 按值接收端口表，只变换返回副本，不修改元件字段或定义对象。
// rotation 表示四分之一圈数，先把负数与超范围值归一化到 0..3。
QVariantList transformed(QVariantList ports, const QVariantMap& c) {
    const int rot = ((c.value("rotation").toInt() % 4) + 4) % 4;
    for (auto& value : ports) {
        auto p = value.toMap();
        const double x = p.value("x").toDouble(), y = p.value("y").toDouble();
        double nx = x, ny = y;
        switch (rot) {
        case 1: nx = -y; ny = x; break;
        case 2: nx = -x; ny = -y; break;
        case 3: nx = y; ny = -x; break;
        default: break;
        }
        // 只旋转局部偏移；世界坐标平移留给调用方，端口索引与位宽保持不变。
        p["x"] = Grid::snap(nx);
        p["y"] = Grid::snap(ny);
        value = p;
    }
    return ports;
}
}
// 查询与初始化均转发注册定义；几何查询额外统一坐标变换，两套仿真引擎共用计算入口。
QStringList typeIds() { return Components::typeIds(); }
Role role(const QString& type) { return Components::definition(type).role(); }
int inputCount(const QVariantMap& c) { return definition(c).inputCount(c); }
int outputCount(const QVariantMap& c) { return definition(c).outputCount(c); }
int inputBitWidth(const QVariantMap& c, int p) { return definition(c).inputWidth(c, p); }
int outputBitWidth(const QVariantMap& c, int p) { return definition(c).outputWidth(c, p); }
QVariantList inputPorts(const QVariantMap& c) { return transformed(definition(c).inputPorts(c), c); }
QVariantList outputPorts(const QVariantMap& c) { return transformed(definition(c).outputPorts(c), c); }
void initializeDefaults(QVariantMap& c, const QString& subId) { definition(c).defaults(c, subId); }
QVector<QString> computeCombinationalOutputs(const QString& type, const QVector<QString>& inputs,
                                             int bitWidth, const QVariantList& splits) {
    return Components::definition(type).compute(inputs, bitWidth, splits);
}
} // namespace ComponentLogic
