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

    // 全量求值
    static void evaluateAll(CircuitContext& root,
                            QHash<QString, CircuitContext>& subContexts);
};

#endif // CIRCUITEVALUATOR_H