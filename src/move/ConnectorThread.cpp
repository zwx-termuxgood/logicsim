#include "ConnectorThread.h"
// 引入本文件要实现的 ConnectorThread 类完整声明：定义其成员函数前必须看到类定义
#include "Connector.h"
// 引入连线重连计算器：工作线程取到请求后调用 Connector::computeWires 执行耗时计算
#include "MoveGesture.h"
// 引入移动手势完整定义：计算完成后需调用 gesture 的 notifyResult 回调写回结果
#include <QMutexLocker>
// 引入 QMutexLocker：RAII 方式加解锁互斥量，保证异常/提前返回时自动解锁

ConnectorThread* ConnectorThread::s_instance = nullptr;
// 定义单例静态指针：初值为空，首次 instance() 调用时创建

// 定义"获取单例"接口
ConnectorThread* ConnectorThread::instance() {
    // 尚未创建则创建并启动
    if (!s_instance) {
        // 私有构造：new 出唯一实例
        s_instance = new ConnectorThread();
        // 启动工作线程，进入 run() 的事件循环
        s_instance->start();
    }
    // 返回单例指针
    return s_instance;
}

// 构造函数：初始化工作线程对象
ConnectorThread::ConnectorThread() {
    // 设置对象名，便于调试器/日志中识别该线程
    setObjectName("ConnectorThread");
}

// 定义"入队寻路请求"静态接口
void ConnectorThread::enqueueRequest(const MoveRequest& req, bool priority) {
    // 取单例（必要时创建并启动）
    ConnectorThread* inst = instance();
    // 加锁保护请求队列与 m_hasPending
    QMutexLocker lk(&inst->m_lock);
    // 优先请求：插到队首，让拖动结束的最终位置尽快得到计算
    if (priority) {
        inst->m_queue.prepend(req);
        // 普通请求：追加到队尾，拖动过程中的预计算按顺序排队
    } else {
        inst->m_queue.append(req);
    }
    // 标记存在待处理请求（供 isOverrideRequested 判断是否需要中断当前计算）
    inst->m_hasPending = true;
    // 唤醒可能在等待队列非空的工作线程
    inst->m_cond.wakeAll();
}

// 定义"查询是否有更高优先级请求在排队"静态接口
bool ConnectorThread::isOverrideRequested() {
    // 单例尚未创建说明根本没人计算，无需中断
    if (!s_instance) return false;
    // 加锁保护 m_hasPending
    QMutexLocker lk(&s_instance->m_lock);
    // 有待处理请求即为真，A* 内部据此放弃过期计算
    return s_instance->m_hasPending;
}

// 定义"工作线程主循环"接口
void ConnectorThread::run() {
    // 永久循环：线程生命周期内持续处理请求
    while (true) {
        // 先构造一个占位请求（MoveRequest 无默认构造，需显式给参）
        MoveRequest req(nullptr, 0, 0);
        // 用块限界控制锁的作用范围，避免持锁执行耗时计算
        {
            // 加锁保护请求队列
            QMutexLocker lk(&m_lock);
            // 队列为空时在条件变量上休眠，直到入队唤醒
            while (m_queue.isEmpty()) {
                m_cond.wait(&m_lock);
            }
            // 取出队首请求
            req = m_queue.dequeue();
            // 更新待处理标志：队列已空则为 false，说明当前无更新请求需要抢占
            m_hasPending = !m_queue.isEmpty();
        }

        // 执行耗时计算（此时已释放锁，允许入队线程并发追加新请求）
        MoveResult* result = Connector::computeWires(req);
        // 即使 result 为 nullptr（请求被覆盖/中止），也必须通知，
        // 否则 MoveGesture::forceRequest 会永久等待 → UI 死锁
        if (!result) result = new MoveResult();

        // 把结果写回所属手势（加锁在 notifyResult 内部完成）
        req.gesture()->notifyResult(req, result);
    }
}