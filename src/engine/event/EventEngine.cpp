#include "EventEngine.h"
// 引入本文件要实现的 EventEngine 类完整声明：定义其成员函数前必须看到类定义
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：本文件按需使用（与其它 EventEngine 实现文件保持一致的依赖）
#include <QDebug>
// 引入 QDebug：调试输出用（本文件保留 include 以便后续加日志）
#include <algorithm>
// 引入算法库：本文件按需使用（最值/遍历工具）

// 构造函数：初始化空引擎，不分配大量资源（容器默认构造，节点与表均为空）
EventEngine::EventEngine() {}
// 析构函数：QVector/QHash 等成员自动析构，无需手动释放
EventEngine::~EventEngine() {}

// 定义"设置全局门延迟"接口
void EventEngine::setGlobalGateDelay(quint64 d) {
    // 下限钳到 1：零延迟会让同一时刻的重算无限触发
    if (d < 1) d = 1;
    // 写入运行参数
    m_opts.globalGateDelay = d;
}
// 定义"设置线延迟"接口
void EventEngine::setWireDelay(quint64 d) {
    // 下限钳到 1：零延迟会导致同刻传播自我循环
    if (d < 1) d = 1;
    // 写入运行参数
    m_opts.wireDelay = d;
}
// 定义"为某类型门设置延迟覆盖"接口
void EventEngine::setGateTypeDelay(const QString& type, quint64 d) {
    // 下限钳到 1
    if (d < 1) d = 1;
    // 写入覆盖表，优先于全局值
    m_gateDelays[type] = d;
}
// 定义"移除某类型的延迟覆盖"接口
void EventEngine::removeGateTypeDelay(const QString& type) {
    // 从覆盖表移除，回退使用全局门延迟
    m_gateDelays.remove(type);
}
// 定义"清空所有类型延迟覆盖"接口
void EventEngine::clearGateTypeDelays() {
    // 清空覆盖表：加载新存档或重置设置时使用
    m_gateDelays.clear();
}

// 定义"查询某类型实际延迟"接口
quint64 EventEngine::gateDelayFor(const QString& type) const {
    // 先查类型覆盖表
    auto it = m_gateDelays.constFind(type);
    // 命中覆盖则返回覆盖值
    if (it != m_gateDelays.constEnd()) return it.value();
    // 未命中回退全局门延迟
    return m_opts.globalGateDelay;
}

// 定义"完全清空引擎"接口：节点、索引、表、队列、计数器、脏集合全部重置
void EventEngine::clear() {
    // 清空节点数组
    m_nodes.clear();
    // 清空 qid → 索引映射
    m_idToIdx.clear();
    // 清空输入端口驱动表
    m_inDrivers.clear();
    // 清空输出端口目标表
    m_outDests.clear();
    // 清空端口驱动值表
    m_portDriverValues.clear();
    // 逐个弹出事件队列（priority_queue 不支持 clear）
    while (!m_queue.empty()) m_queue.pop();
    // 仿真时间归零
    m_time = 0;
    // 事件序号归零
    m_seq = 0;
    // 累计处理计数归零
    m_totalEventsProcessed = 0;
    // 清空脏节点队列
    m_dirtyQids.clear();
    // 清空脏节点去重集合
    m_dirtySet.clear();
}

// 定义"软重置引擎"接口：保留输入元件当前值，其余节点值清零并重建初始事件
void EventEngine::reset() {
    // 暂存输入元件的当前输出值（索引 → 值）
    QVector<QPair<int, QString>> savedInputs;
    // 预分配容量
    savedInputs.reserve(m_nodes.size());
    // 遍历节点收集输入元件当前值
    for (int i = 0; i < m_nodes.size(); ++i) {
        // 取节点
        const Node& n = m_nodes[i];
        // 只收集数据源且有有效输出值的节点
        if (n.isInput && !n.outVals.isEmpty())
            savedInputs.append(qMakePair(i, n.outVals[0]));
    }

    // 清空事件队列
    while (!m_queue.empty()) m_queue.pop();
    // 仿真时间归零
    m_time = 0;
    // 事件序号归零
    m_seq = 0;
    // 累计处理计数归零
    m_totalEventsProcessed = 0;
    // 清空脏节点队列
    m_dirtyQids.clear();
    // 清空脏节点去重集合
    m_dirtySet.clear();

    // 遍历所有节点，把输入/输出端口值清零（保持各自位宽长度）
    for (auto& n : m_nodes) {
        for (auto& s : n.inVals)  s = QString(s.length(), '0');
        for (auto& s : n.outVals) s = QString(s.length(), '0');
    }
    // 清空端口驱动值表中各驱动的记录值
    for (auto it = m_portDriverValues.begin(); it != m_portDriverValues.end(); ++it)
        it.value().clear();

    // 重新初始化数据源/时钟的初值
    initValues();

    // 把暂存的输入值写回对应节点
    for (const auto& p : savedInputs) {
        // 节点索引
        int i = p.first;
        // 暂存的值
        const QString& v = p.second;
        // 索引非法则跳过
        if (i < 0 || i >= m_nodes.size()) continue;
        // 写回输出端口值
        if (!m_nodes[i].outVals.isEmpty()) m_nodes[i].outVals[0] = v;
        // 写回输入端口值
        if (!m_nodes[i].inVals.isEmpty())  m_nodes[i].inVals[0]  = v;
    }

    // 重新初始化驱动表并预传播，调度初始 Recalc 事件
    initDriverValues();
}