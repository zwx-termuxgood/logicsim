#include "circuit.h"
#include "CircuitEvaluator.h"

void Circuit::evaluateAll() {
    // 1. 从 root 出发递归求值整棵电路树（保持 root 及其内部实例输出一致）
    CircuitEvaluator::evaluateAll(m_root, m_subContexts);

    // 2. 如果当前正在“编辑”某个子电路（switchEditContext 进入，非浏览模式），
    //    对该子电路独立求值一次：以它的 input 元件当前值为输入，
    //    使其内部逻辑与 output 自动传播。
    //    这一步与 root 完全无关 —— root 不会因为用户改了子电路内 input 而变化。
    if (!m_currentCtxId.isEmpty() && !m_viewOnly) {
        auto it = m_subContexts.find(m_currentCtxId);
        if (it != m_subContexts.end())
            CircuitEvaluator::evaluateStandalone(it.value(), m_subContexts);
    }
}

// Q_INVOKABLE 入口：QML 侧可能调用 circuit.evaluate()
void Circuit::evaluate() {
    evaluateAll();
}