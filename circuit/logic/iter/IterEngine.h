#ifndef ITERENGINE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define ITERENGINE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：evalOne 直接修改元件的 map（读写 inputPortValues/outputPorts）
#include <QVariantList>
// 引入 QVariantList：evalOne 内部读取 outputPorts 等列表字段；此处为对齐约定保留
#include <QHash>
// 引入 QHash：inputMap 参数类型，以及 syncSubPins/evaluateAll 中按 ID 组织的子上下文
#include <QString>
// 引入 QString：inputMap 的 key 是 "compId:port" 形式字符串
#include <QVector>
// 引入 QVector：inputMap 的 value 是 QVector<QPair<int,int>>，comps 参数也是 QVector
#include <QPair>
// 引入 QPair：inputMap 的每个驱动源是 (源索引, 源端口) 对
#include "../../core/CircuitContext.h"
// 引入 CircuitContext 完整定义：evaluateContext/evaluateAll/evaluateStandalone
// 都以引用直接修改上下文的 components 列表

/**
 * @brief 电路求值引擎（旧引擎，迭代到收敛）。
 *
 * 支持多驱：一个输入端口可以接多个输出，
 * 合并时如果存在 0/1 冲突则输出 E。
 */
// 上方注释点明本引擎的定位：
//   - "旧引擎"：作为 EventEngine 的对照与回退，语义更直观但性能较差；
//   - "迭代到收敛"：反复扫描所有元件直到一轮无变化，不建事件队列；
//   - "支持多驱"：一个输入端口接多个输出源时按位合并，冲突输出 E（error/unknown）
// 全部方法都是 static：引擎无内部状态，所有状态都在传入的上下文里，
// 这样调用方无需管理对象生命周期，也便于在浏览子电路时对副本单独求值

class IterEngine {
    // 不使用 QObject：纯 C++ 工具类，无信号槽需求

public:
    static bool evalOne(QVariantMap& c,
                        const QHash<QString, QVector<QPair<int,int>>>& inputMap,
                        const QVector<QVariantMap>& comps);
    // 计算单个元件的输入值与输出值：
    //   - 根据 inputMap 收集每个输入端口的驱动源，读取 comps 中源元件的 outputPorts，
    //     用多驱合并规则算出该端口输入值；
    //   - 按元件类型计算输出（数据源取 inputBits，组合逻辑委托 ComponentLogic）；
    //   - 仅在值实际变化时写回 c，返回是否发生改变；
    // 返回 true 表示该元件有变化，调用方据此决定是否继续迭代

    static bool evaluateContext(CircuitContext& ctx);
    // 对单个上下文做一轮求值：
    //   1) 生成虚拟线并合并进临时 wire 列表；
    //   2) 构建"目标端口 → 驱动源列表"的 inputMap；
    //   3) 按拓扑序（无环时）或循环迭代（有环时，最多 200 轮）逐个 evalOne；
    // 返回本轮是否有任何元件发生变化；不做子电路递归，由 evaluateAll 负责

    static void syncSubPins(CircuitContext& ctx,
                            const QHash<QString, CircuitContext>& subContexts);
    // 同步子电路实例的端口元数据与端口值数组：
    //   - 遍历子电路定义中的 input/output 元件，生成 subInputNames/subOutputNames/
    //     subInputWidths/subOutputWidths，写入实例 map；
    //   - 若实例的 inputPortValues/outputPorts 数量与定义不符，补零/截断对齐；
    // 在每次求值前调用，保证子电路结构变化（增删 input/output）后实例仍合法

    static void evaluateAll(CircuitContext& root,
                            QHash<QString, CircuitContext>& subContexts);
    // 全量求值入口：
    //   - 先对所有上下文（根与子电路）调用 syncSubPins；
    //   - 再反复调用 evalRecursive 直到收敛或达到 50 轮上限；
    // evalRecursive 内部会交替执行"上下文内迭代"与"子电路实例求值"，
    // 从而处理跨层反馈

    static bool evaluateStandalone(CircuitContext& ctx,
                                   const QHash<QString, CircuitContext>& subContexts);
    // 对单个上下文求值（含其引用的子电路）：
    // 与 evaluateAll 的区别是不求值根上下文，也不修改传入的 subContexts；
    // 用于浏览子电路时对当前上下文本地刷新，避免影响父电路状态
};

#endif // ITERENGINE_H
// 结束头文件保护