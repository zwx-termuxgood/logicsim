#ifndef CIRCUITCONTEXT_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define CIRCUITCONTEXT_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantList>
// 引入 QVariantList（即 QList<QVariant>）：components 与 wires 都用它存储，元素本身是 QVariantMap
#include <QString>
// 引入 QString：indexOfComponent/indexOfWire 的参数类型需要它

/**
 * @brief 电路上下文：一组元件与连线。
 *
 * 一个 Circuit 由 1 个根上下文（主电路）+ N 个子电路上下文组成。
 */
// 上方注释说明本结构体的定位：CircuitContext 是纯粹的"数据袋"，不持有任何引擎/定时器/撤销状态，
// 因此可以被 Circuit 以值语义自由拷贝（撤销快照、导入、粘贴等都依赖这一点）

struct CircuitContext {
    // 使用 struct 而非 class：成员默认 public，符合"纯数据容器"的语义，
    // 外部可以直接访问 components/wires/counter 字段，不需要额外 getter

    QVariantList components;   // 元件列表（每个元素为 QVariantMap）
    // 元件集合：每个 QVariantMap 至少含 id/type/x/y/bitWidth/inputCount 等字段，
    // 具体字段由 Component 命名空间的 inline 存取器约定；
    // 选用 QVariantList 而非强类型 struct，是为了让 QML 侧直接以 JS 对象数组使用

    QVariantList wires;        // 连线列表（每个元素为 QVariantMap）
    // 连线集合：bezier 线存 fromComp/fromPort/toComp/toPort，
    // ortho 线额外存 wireType="ortho" 与 path 顶点列表；
    // 同样用 QVariantMap 保持与 QML 的零转换交互

    int compCounter = 0;       // 元件 ID 计数器
    // 每个上下文独立维护的元件编号：IdAllocator 通过 ++compCounter 生成 C1/C2/...；
    // 独立计数保证子电路内的 ID 只在本上下文唯一，跨上下文由实例路径区分

    int wireCounter = 0;       // 连线 ID 计数器
    // 连线编号计数器：生成 W1/W2/...；
    // 与 compCounter 分开，避免元件与连线 ID 空间冲突

    int indexOfComponent(const QString& id) const;
    // 线性查找元件索引：遍历 components 逐个比较 toMap().value("id")；
    // 声明在此、实现在 cpp，避免头文件引入 QVariantMap 的完整模板展开

    int indexOfWire(const QString& id) const;
    // 线性查找连线索引：同上；
    // 之所以用 O(n) 线性查找而非维护 QHash 索引，
    // 是因为上下文规模通常不大，且撤销/粘贴会整体替换列表，维护索引反而复杂

    bool isEmpty() const { return components.isEmpty() && wires.isEmpty(); }
    // 判断上下文是否为空：元件与连线都为空的快捷判断；
    // inline 实现因为只是一次成员调用，放在头文件可省一次函数调用开销
};

#endif // CIRCUITCONTEXT_H
// 结束头文件保护