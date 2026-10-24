#include "ConnectorThread.h"
#include "Connector.h"
#include "MoveGesture.h"
#include <QMutexLocker>

ConnectorThread* ConnectorThread::s_instance = nullptr;

ConnectorThread* ConnectorThread::instance() {
    if (!s_instance) {
        s_instance = new ConnectorThread();
        s_instance->start();
    }
    return s_instance;
}

ConnectorThread::ConnectorThread() {
    setObjectName("ConnectorThread");
}

void ConnectorThread::enqueueRequest(const MoveRequest& req, bool priority) {
    ConnectorThread* inst = instance();
    QMutexLocker lk(&inst->m_lock);
    if (priority) {
        inst->m_queue.prepend(req);
    } else {
        inst->m_queue.append(req);
    }
    inst->m_hasPending = true;
    inst->m_cond.wakeAll();
}

bool ConnectorThread::isOverrideRequested() {
    if (!s_instance) return false;
    QMutexLocker lk(&s_instance->m_lock);
    return s_instance->m_hasPending;
}

void ConnectorThread::run() {
    while (true) {
        MoveRequest req(nullptr, 0, 0);
        {
            QMutexLocker lk(&m_lock);
            while (m_queue.isEmpty()) {
                m_cond.wait(&m_lock);
            }
            req = m_queue.dequeue();
            m_hasPending = !m_queue.isEmpty();
        }

        MoveResult* result = Connector::computeWires(req);
        // 即使 result 为 nullptr（请求被覆盖/中止），也必须通知，
        // 否则 MoveGesture::forceRequest 会永久等待 → UI 死锁
        if (!result) result = new MoveResult();

        req.gesture()->notifyResult(req, result);
    }
}