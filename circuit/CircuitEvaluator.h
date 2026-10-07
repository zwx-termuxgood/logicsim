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
 * 负责：
 *   - 单个元件的组合逻辑求值
 *   - 单个上下文的拓扑排序 + 求值
 *   - 子电路引脚同步
 *   - 全量求值（含子电路状态回灌）
 */
class CircuitEvaluator {
public:
    static bool evalOne(QVariantMap& c,
                        const QHash<QString, QPair<int,int>>& inputMap,
                        const QVector<QVariantMap>& comps);

    static void evaluateContext(CircuitContext& ctx);

    static void syncSubPins(CircuitContext& ctx,
                            const QHash<QString, CircuitContext>& subContexts);

    static void evaluateAll(CircuitContext& root,
                            QHash<QString, CircuitContext>& subContexts);
};

#endif // CIRCUITEVALUATOR_H