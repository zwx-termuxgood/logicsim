#ifndef MOVEGESTURE_H
#define MOVEGESTURE_H

#include <QVariantMap>
#include <QList>
#include <QSet>
#include <QHash>
#include <QString>
#include "ConnectionData.h"
#include "MoveRequest.h"
#include "MoveResult.h"

class AvoidanceMap;
class CircuitContext;

class MoveGesture {
public:
    MoveGesture(const CircuitContext* circuit,
                const QList<QVariantMap>& selectedComponents);
    ~MoveGesture();

    const QList<ConnectionData>& getConnections() const;
    AvoidanceMap* getFixedAvoidanceMap() const;
    const QList<QVariantMap>& getSelected() const { return m_selected; }

    void notifyResult(const MoveRequest& req, MoveResult* result);
    MoveResult* findResult(int dx, int dy);
    bool enqueueRequest(int dx, int dy);
    MoveResult* forceRequest(int dx, int dy);

    const CircuitContext* circuit() const { return m_circuit; }

private:
    const CircuitContext* m_circuit;
    QList<QVariantMap> m_selected;
    mutable QList<ConnectionData> m_connections;
    mutable bool m_connectionsValid = false;
    mutable AvoidanceMap* m_initAvoid = nullptr;
    QHash<int, MoveResult*> m_cachedResults;

    void computeConnections();
};

#endif
