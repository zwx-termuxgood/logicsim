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
 * @brief 电路求值引擎（旧引擎，迭代到收敛）。
 *
 * 支持多驱：一个输入端口可以接多个输出，
 * 合并时如果存在 0/1 冲突则输出 E。
 */
class CircuitEvaluator {
public:
    static bool evalOne(QVariantMap& c,
                        const QHash<QString, QVector<QPair<int,int>>>& inputMap,
                        const QVector<QVariantMap>& comps);

    static bool evaluateContext(CircuitContext& ctx);

    static void syncSubPins(CircuitContext& ctx,
                            const QHash<QString, CircuitContext>& subContexts);

    static void evaluateAll(CircuitContext& root,
                            QHash<QString, CircuitContext>& subContexts);

    static bool evaluateStandalone(CircuitContext& ctx,
                                   const QHash<QString, CircuitContext>& subContexts);
};

#endif // CIRCUITEVALUATOR_H