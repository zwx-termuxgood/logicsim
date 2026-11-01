#ifndef MOVEGESTURE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define MOVEGESTURE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：选中元件列表 m_selected 的元素类型
#include <QList>
// 引入 QList：m_selected 与 getConnections 返回的连接列表都用它
#include <QSet>
// 引入 QSet：实现中用于记录已访问 wire ID，避免沿链重复遍历（头文件为对齐保留）
#include <QHash>
// 引入 QHash：m_cachedResults 以 (dx,dy) 哈希为 key 缓存计算结果
#include <QString>
// 引入 QString：实现中比较 wire ID、方向串等需要
#include "ConnectionData.h"
// 引入 ConnectionData 完整定义：m_connections 以值语义存储它，需要完整布局
#include "MoveRequest.h"
// 引入 MoveRequest 完整定义：notifyResult/enqueueRequest/forceRequest 的参数类型
#include "MoveResult.h"
// 引入 MoveResult 完整定义：缓存的是指针但回调中需要传递完整类型

class AvoidanceMap;
// 前置声明：只持有指针 m_initAvoid，无需完整定义，减少编译依赖
class CircuitContext;
// 前置声明：只持有 const 指针 m_circuit，读取元件/连线字段在实现中进行

class MoveGesture {
    // 一次"移动手势"的会话对象：
    // 在用户开始拖动选中元件时创建，记录被移动元件、它们与外部网络的连接、
    // 以及固定避让地图；拖动过程中不断提交位移请求，由 ConnectorThread 异步计算，
    // 结果缓存在本对象中供 UI 取用；拖动结束后销毁
public:
    // 生命周期与请求/结果接口

    MoveGesture(const CircuitContext* circuit,
                const QList<QVariantMap>& selectedComponents);
    // 构造：
    //   circuit — 只读指针，指向拖动发生时的上下文（调用方须保证其存活期覆盖 gesture）；
    //   selectedComponents — 被移动元件的 map 副本，拷贝是为了拖动期间上下文可能被修改
    ~MoveGesture();
    // 析构：删除所有缓存的 MoveResult 与 m_initAvoid，避免泄漏

    const QList<ConnectionData>& getConnections() const;
    // 获取连接描述列表（懒计算）：
    // 首次调用时 computeConnections() 扫描选中元件端口与外部网络的关联，
    // 结果缓存并返回 const 引用；Connector::computeWires 每次请求都会读取它
    AvoidanceMap* getFixedAvoidanceMap() const;
    // 获取固定避让地图（懒创建）：
    // 只包含非选中元件与非选中 ortho 线，作为所有位移请求的基底；
    // 每次 A* 尝试会 clone 它，避免临时标记污染
    const QList<QVariantMap>& getSelected() const { return m_selected; }
    // 返回被移动元件列表：Connector 用它在克隆地图上叠加"选中元件新位置"的占用

    void notifyResult(const MoveRequest& req, MoveResult* result);
    // 工作线程回调：把计算结果放入缓存，key 为请求的 hash；
    // 由 ConnectorThread::run 在计算完成后调用，加锁保护
    MoveResult* findResult(int dx, int dy);
    // 查询某位移的结果：命中返回缓存指针（所有权归本对象），未命中返回 nullptr；
    // UI 定时轮询此接口，拿到结果后应用到电路
    bool enqueueRequest(int dx, int dy);
    // 提交异步计算请求：若该位移已缓存则直接返回 false（无需重复计算），
    // 否则交给 ConnectorThread 非优先入队并返回 true
    MoveResult* forceRequest(int dx, int dy);
    // 同步强制计算：以优先入队提交请求，然后自旋等待缓存出现；
    // 用于拖动结束时的最终位置，必须立即拿到结果；
    // 依赖 ConnectorThread 在结果为空时也通知，否则会死锁

    const CircuitContext* circuit() const { return m_circuit; }
    // 暴露上下文指针：实现中需要遍历原始元件/连线，Connector 通过它获取

private:
    // 私有状态与懒计算

    const CircuitContext* m_circuit;
    // 上下文只读指针：不拥有，生命周期由 Circuit 保证
    QList<QVariantMap> m_selected;
    // 选中元件副本：拖动期间上下文可能被刷新，用副本保持计算输入稳定
    mutable QList<ConnectionData> m_connections;
    // 连接描述缓存：mutable 因为 getConnections 是 const 但需懒计算填充
    mutable bool m_connectionsValid = false;
    // 连接缓存是否有效：首次 getConnections 后置 true
    mutable AvoidanceMap* m_initAvoid = nullptr;
    // 固定避让地图缓存：mutable + 指针，析构时 delete
    QHash<int, MoveResult*> m_cachedResults;
    // 位移结果缓存：key 为 MoveRequest::hash()，value 为堆分配的 MoveResult；
    // 加锁访问，因为工作线程写入而 UI 线程读取

    void computeConnections();
    // 实际扫描逻辑：
    //   - 收集选中元件所有端口的绝对坐标；
    //   - 对每个落在非选中元件端口上的坐标，沿非选中 ortho wire 向外走，
    //     记录走过的 wire 链、链末端坐标与首选方向；
    //   - 结果填入 m_connections
};

#endif
// 结束头文件保护