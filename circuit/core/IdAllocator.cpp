#include "IdAllocator.h"

namespace IdAllocator {

QString nextComponentId(CircuitContext& ctx) {
    return QString("C%1").arg(++ctx.compCounter);
}

QString nextWireId(CircuitContext& ctx) {
    return QString("W%1").arg(++ctx.wireCounter);
}

} // namespace IdAllocator
