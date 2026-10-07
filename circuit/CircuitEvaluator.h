#ifndef CIRCUITEVALUATOR_H
#define CIRCUITEVALUATOR_H

#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QString>
#include <QVector>
#include <QPair>
#include "CircuitContext.h"

/**
 * @brief 电路求值引擎（无状态，纯函数式）。
 *
 * 求值模型：
 *   evaluateAll 对 root 调用 evalRecursive：
 *     - evalRecursive：本地小迭代（本上下文门 + 每个 sub 实例递归求值）
 *     - evalInstance：克隆子定义 → 灌入实例输入 → 递归求值克隆 → 回灌输出
 *   “克隆”是关键：相同 subId 的多个实例各自独立求值，输入互不覆盖。
 *   递归模型天然支持任意深度子电路嵌套。
 *
 * 独立求值：
 *   evaluateStandalone 用于“进入子电路编辑”时把该子电路当作独立电路求值，
 *   使其 input 元件（由用户输入的值）能自动传播到其内部逻辑与 output。
 */
class CircuitEvaluator {
public:
    // 单个元件求值；返回 true 表示输出有变化
    static bool evalOne(QVariantMap& c,
                        const QHash<QString, QPair<int,int>>& inputMap,
                        const QVector<QVariantMap>& comps);

    // 单个上下文求值；返回 true 表示有任何变化
    static bool evaluateContext(CircuitContext& ctx);

    // 把子电路定义的 input/output 元件信息同步到实例上（结构变化时使用）
    static void syncSubPins(CircuitContext& ctx,
                            const QHash<QString, CircuitContext>& subContexts);

    // 全量求值（从 root 出发递归整棵电路树）
    static void evaluateAll(CircuitContext& root,
                            QHash<QString, CircuitContext>& subContexts);

    // 独立求值：把 ctx 当作它自己的“根”，用其内部 input 元件的当前值作为输入源
    // 递归求值 ctx 内所有门和 sub 实例，更新 ctx 内 output 元件的值。
    // 用于子电路编辑上下文——不依赖 root，与父电路完全无关。
    static bool evaluateStandalone(CircuitContext& ctx,
                                   const QHash<QString, CircuitContext>& subContexts);
};

#endif // CIRCUITEVALUATOR_H