#ifndef CONNECTERTHREAD_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define CONNECTERTHREAD_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QThread>
// 引入 QThread：ConnectorThread 继承它，把耗时的 A* 寻路放到独立线程
#include <QMutex>
// 引入 QMutex：保护请求队列与 m_hasPending 标志，跨线程互斥访问
#include <QWaitCondition>
// 引入 QWaitCondition：队列为空时工作线程休眠，入队时唤醒，避免忙等
#include <QQueue>
// 引入 QQueue：请求队列，支持首部插入（优先请求）与尾部追加
#include "MoveRequest.h"
// 引入 MoveRequest 完整定义：队列元素类型，需要完整布局才能实例化 QQueue<MoveRequest>
#include "MoveResult.h"
// 引入 MoveResult 完整定义：run 中接收计算结果并交给 gesture 回调

class ConnectorThread : public QThread {
    // 单例工作线程：全进程只有一个寻路线程，避免多线程同时消耗 CPU；
    // 继承 QThread 并重写 run 实现独立事件循环外的计算循环
public:
    // 公有静态接口

    static ConnectorThread* instance();
    // 获取单例：首次调用时创建对象并 start()；
    // 静态函数内延迟初始化，避免静态构造顺序问题
    static void enqueueRequest(const MoveRequest& req, bool priority);
    // 入队一个寻路请求：
    //   priority=true 时插到队首（拖动结束时强制算一次，需尽快返回），
    //   否则追加到队尾（拖动过程中的预计算）；入队后唤醒工作线程
    static bool isOverrideRequested();
    // 是否已有更高优先级请求在排队：
    // A* 内部周期性检查此标志，一旦为真立即放弃当前计算，
    // 让位给更新的拖动位置，避免用户快速拖动时累积大量过期计算

    void run() override;
    // 工作线程主循环：
    //   1) 加锁等待队列非空；
    //   2) 取出队首请求，更新 m_hasPending；
    //   3) 解锁后调用 Connector::computeWires 执行耗时计算；
    //   4) 即使 result 为 nullptr 也构造空结果并通知 gesture，
    //      否则 forceRequest 会永久等待导致 UI 死锁

private:
    // 私有区：单例控制与同步原语

    ConnectorThread();
    // 私有构造：禁止外部 new，强制走 instance()

    static ConnectorThread* s_instance;
    // 单例指针：整个进程共享

    QMutex m_lock;
    // 互斥锁：保护 m_queue 与 m_hasPending；
    // 同时作为 QWaitCondition 的关联锁
    QWaitCondition m_cond;
    // 条件变量：队列空时 wait，入队时 wakeAll
    QQueue<MoveRequest> m_queue;
    // 请求队列：MoveRequest 值语义拷贝入队，包含 gesture 指针与位移
    bool m_hasPending = false;
    // 是否还有待处理请求（或正在处理时队列非空）：
    // 作为"是否需要中断当前计算"的粗粒度标志
};

#endif
// 结束头文件保护