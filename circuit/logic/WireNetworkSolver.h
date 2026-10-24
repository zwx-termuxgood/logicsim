#ifndef WIRENETWORKSOLVER_H
#define WIRENETWORKSOLVER_H

#include <QVariantList>

namespace WireNetworkSolver {
    QVariantList generateVirtualWires(const QVariantList& components,
                                      const QVariantList& wires);
    QVariantList connectionDots(const QVariantList& components,
                                const QVariantList& wires);
}

#endif
