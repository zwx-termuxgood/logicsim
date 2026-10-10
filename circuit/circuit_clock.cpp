#include "circuit.h"
#include "EventEngine.h"

void Circuit::tick() {
    m_clockTickCount++;

    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        it.value() = !it.value();

    // 更新所有上下文里的 clock 输出值
    auto updateClockOutputs = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (c.value("type").toString() != "clock") continue;
            bool v = m_clockStates.value(c.value("id").toString(), false);
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList out;
            out.append(v ? QString(bw, '1') : QString(bw, '0'));
            c["outputPorts"] = out;
            ctx.components[i] = c;
        }
    };

    if (m_engineType == "event" && m_eventEngine) {
        // 未开始仿真：仅更新 UI 状态，禁止传播事件
        if (!m_simulationRunning) {
            updateClockOutputs(m_root);
            for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
                updateClockOutputs(it.value());
            emit changed();
            emit clockChanged();
            return;
        }

        // 已开始仿真：正常传播
        auto applyToCtx = [&](Context& ctx, const QString& ctxId) {
            for (int i = 0; i < ctx.components.size(); ++i) {
                QVariantMap c = ctx.components[i].toMap();
                if (c.value("type").toString() != "clock") continue;
                bool v = m_clockStates.value(c.value("id").toString(), false);
                int bw = c.value("bitWidth").toInt();
                if (bw < 1) bw = 1;
                QString valStr = v ? QString(bw, '1') : QString(bw, '0');
                QVariantList out;
                out.append(valStr);
                c["outputPorts"] = out;
                ctx.components[i] = c;

                QStringList qids = engineQidsFor(ctxId, c.value("id").toString());
                for (const auto& qid : qids)
                    m_eventEngine->triggerForceOutput(qid, 0, valStr);
            }
        };
        applyToCtx(m_root, QString());
        for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
            applyToCtx(it.value(), it.key());

        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        quint64 advance = (wd + gd) * 8 + 20;
        m_eventEngine->run(advance, 200000);
        applyEngineOutputsToContexts();
    } else {
        updateClockOutputs(m_root);
        for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
            updateClockOutputs(it.value());
        evaluateAll();
    }

    emit changed();
    emit clockChanged();
}

void Circuit::startClock() {
    if (!m_clockTimer) return;
    // 新引擎必须已开始仿真才能启动时钟
    if (m_engineType == "event" && !m_simulationRunning) return;
    m_clockRunning = true;
    int interval = qMax(1, 1000 / qMax(1, m_clockFrequency));
    m_clockTimer->start(interval);
    emit clockChanged();
}

void Circuit::stopClock() {
    if (!m_clockTimer) return;
    m_clockRunning = false;
    m_clockTimer->stop();
    emit clockChanged();
}

void Circuit::setClockFrequency(int hz) {
    m_clockFrequency = qBound(1, hz, 1000);
    if (m_clockRunning) {
        int interval = qMax(1, 1000 / m_clockFrequency);
        m_clockTimer->start(interval);
    }
    emit clockChanged();
}

void Circuit::singleStep() {
    if (m_engineType == "event") {
        if (m_engineDirty) {
            rebuildEventEngine();
            applyEngineOutputsToContexts();
        }
        if (m_eventEngine) {
            m_eventEngine->step();
            applyEngineOutputsToContexts();
            emit simulationTick();
            emit changed();
        }
        return;
    }
    tick();
}

// 控制模式下点击时钟：切换其状态
// 新引擎必须已开始仿真才允许传播；未仿真时只改状态
void Circuit::toggleClock(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "clock") return;

    bool v = !m_clockStates.value(id, false);
    m_clockStates[id] = v;

    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    QString valStr = v ? QString(bw, '1') : QString(bw, '0');
    QVariantList out;
    out.append(valStr);
    c["outputPorts"] = out;
    ctx.components[idx] = c;

    if (m_engineType == "event" && m_eventEngine) {
        // 未开始仿真：不传播，只更新状态
        if (!m_simulationRunning) {
            emit changed();
            return;
        }

        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerForceOutput(qid, 0, valStr);

        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        quint64 advance = (wd + gd) * 8 + 20;
        m_eventEngine->run(advance, 200000);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else {
        evaluateAll();
    }
    emit changed();
}

void Circuit::resetAllInputs() {
    if (m_viewOnly) return;
    pushUndo();

    m_clockTickCount = 0;

    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        it.value() = false;

    auto resetCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            QString t = c.value("type").toString();
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            if (t == "clock") {
                QVariantList out;
                out.append(QString(bw, '0'));
                c["outputPorts"] = out;
                ctx.components[i] = c;
            }
        }
    };
    resetCtx(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        resetCtx(it.value());

    if (m_engineType == "event" && m_eventEngine) {
        rebuildEventEngine();
        m_eventEngine->reset();
        applyEngineOutputsToContexts();
    } else {
        evaluateAll();
    }

    emit clockChanged();
    emit changed();
}