#ifndef MOVERESULT_H
#define MOVERESULT_H

#include <QVariantMap>
#include <QList>
#include <QPair>
#include "ConnectionData.h"

class MoveResult {
public:
    QList<QVariantMap> additions;
    QList<QVariantMap> removals;
    QList<QPair<QVariantMap, QVariantMap>> replacements;
    QList<ConnectionData> unsatisfied;
    int totalDistance = 0;
};

#endif
