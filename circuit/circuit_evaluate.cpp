#include "circuit.h"
#include "CircuitEvaluator.h"

void Circuit::evaluateAll() {
    CircuitEvaluator::evaluateAll(m_root, m_subContexts);
}

void Circuit::evaluate() {
    evaluateAll();
}
