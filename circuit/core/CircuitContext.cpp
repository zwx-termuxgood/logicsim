#include "CircuitContext.h"

int CircuitContext::indexOfComponent(const QString& id) const {
    for (int i = 0; i < components.size(); ++i)
        if (components[i].toMap().value("id").toString() == id) return i;
    return -1;
}

int CircuitContext::indexOfWire(const QString& id) const {
    for (int i = 0; i < wires.size(); ++i)
        if (wires[i].toMap().value("id").toString() == id) return i;
    return -1;
}
