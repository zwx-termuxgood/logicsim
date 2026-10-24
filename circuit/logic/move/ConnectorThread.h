#ifndef CONNECTERTHREAD_H
#define CONNECTERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include "MoveRequest.h"
#include "MoveResult.h"

class ConnectorThread : public QThread {
public:
    static ConnectorThread* instance();
    static void enqueueRequest(const MoveRequest& req, bool priority);
    static bool isOverrideRequested();

    void run() override;

private:
    ConnectorThread();
    static ConnectorThread* s_instance;

    QMutex m_lock;
    QWaitCondition m_cond;
    QQueue<MoveRequest> m_queue;
    bool m_hasPending = false;
};

#endif