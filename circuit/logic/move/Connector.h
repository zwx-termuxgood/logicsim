#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "MoveRequest.h"
#include "MoveResult.h"

class Connector {
public:
    static MoveResult* computeWires(const MoveRequest& req);
};

#endif
