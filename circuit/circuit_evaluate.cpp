#include "circuit.h"
#include "CircuitEvaluator.h"
#include "EventEngine.h"

void Circuit::evaluateAll() {
    // 1) 同步子电路端口元信息（修复"端口数一直为 0"）
    CircuitEvaluator::syncSubPins(m_root, m_subContexts);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        CircuitEvaluator::syncSubPins(it.value(), m_subContexts);

    // 2) 浏览子电路时：把父实例输入灌入子电路内部 input 元件
    if (m_viewOnly && !m_currentCtxId.isEmpty()) {
        syncSubStateFromParent(m_currentCtxId);
    }

    if (m_engineType == "event") {
        m_engineDirty = true;
        // 通知左面板刷新 [in/out] 计数
        emit subcircuitsChanged();
        return;
    }

    // 旧引擎
    CircuitEvaluator::evaluateAll(m_root, m_subContexts);

    // 只要当前有子上下文，就独立求值（包括浏览模式）
    if (!m_currentCtxId.isEmpty()) {
        auto it = m_subContexts.find(m_currentCtxId);
        if (it != m_subContexts.end())
            CircuitEvaluator::evaluateStandalone(it.value(), m_subContexts);
    }

    // 通知左面板刷新 [in/out] 计数
    emit subcircuitsChanged();
}

void Circuit::evaluate() {
    evaluateAll();
}