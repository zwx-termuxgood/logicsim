#include "circuit.h"

void Circuit::tick() {
    m_clockTickCount++;

    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        it.value() = !it.value();

    auto updateClocks = [&](Context& ctx) {
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
    updateClocks(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        updateClocks(it.value());

    evaluateAll();

    emit changed();
    emit clockChanged();
}

void Circuit::startClock() {
    if (!m_clockTimer) return;
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

void Circuit::singleStep() { tick(); }

void Circuit::resetAllInputs() {
    if (m_viewOnly) return;
    pushUndo();

    // 复位时钟拍数
    m_clockTickCount = 0;

    // 复位所有时钟状态
    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        it.value() = false;

    auto resetCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            QString t = c.value("type").toString();
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            if (t == "input") {
                QVariantList bits;
                for (int k = 0; k < bw; ++k) bits.append(false);
                c["inputBits"] = bits;
                ctx.components[i] = c;
            } else if (t == "clock") {
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
    evaluateAll();
    emit clockChanged();
    emit changed();
}