#ifndef EVENTENGINE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define EVENTENGINE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QString>
// 引入 QString：节点 qid、类型名、值字符串等都以 QString 表示
#include <QStringList>
// 引入 QStringList：getOutputs/getInputs 返回端口值列表
#include <QVariantList>
// 引入 QVariantList：build 接收元件/连线列表（与 Circuit 中存储形式一致）
#include <QVariantMap>
// 引入 QVariantMap：Node::raw 保存元件原始 map，按需读取 outputSplits 等字段
#include <QHash>
// 引入 QHash：ID→索引、驱动表、输出目标表、门延迟覆盖都用它
#include <QSet>
// 引入 QSet：脏节点去重集合 m_dirtySet
#include <QVector>
// 引入 QVector：节点数组 m_nodes、输入/输出值数组、脏队列与缓冲区
#include <QPair>
// 引入 QPair：输出目标表元素是 (目标节点, 目标端口) 对
#include <queue>
// 引入 std::priority_queue：事件队列按 (时间, 序号) 排序
#include <cstdint>
// 引入标准整数类型：quint64/qint64 是 Qt 别名，底层依赖 <cstdint> 的 uint64_t

class EventEngine
// 无基类：引擎是纯 C++ 对象，不继承 QObject，避免元对象开销与信号依赖，
// 由 Circuit 手动 new/delete 管理生命周期
{
public:
    // 公有接口：配置、构建、触发、推进、查询、脏节点追踪

    struct Options {
        // 引擎运行参数聚合：集中放置便于整体默认值维护
        quint64 globalGateDelay = 1;
        // 全局门延迟：未按类型覆盖时所有门的默认延迟（仿真时间单位）
        quint64 wireDelay       = 1;
        // 线延迟：输出变化传播到下游输入端所需时间
        quint64 runBudget       = 200000;
        // 单次 run 的事件处理上限：防止超大电路在一次调用中耗尽 UI 线程
    };

    EventEngine();
    // 构造函数：初始化空引擎，不分配大量资源
    ~EventEngine();
    // 析构函数：释放内部容器（QVector/QHash 自动析构）

    void setGlobalGateDelay(quint64 d);
    // 设置全局门延迟：内部钳位到 ≥1，避免零延迟导致同一时刻无限重算
    void setWireDelay(quint64 d);
    // 设置线延迟：同样钳位到 ≥1
    void setGateTypeDelay(const QString& type, quint64 d);
    // 为某类型门设置延迟覆盖：写入 m_gateDelays，优先于全局值
    void removeGateTypeDelay(const QString& type);
    // 移除某类型的延迟覆盖：回到使用全局门延迟
    void clearGateTypeDelays();
    // 清空所有类型覆盖：加载新存档或重置设置时使用
    void setDefaultRunBudget(quint64 n) { if (n < 1) n = 1; m_opts.runBudget = n; }
    // 设置默认 run 预算：inline，n<1 时兜底为 1，防止零预算导致 run 永不推进

    quint64 globalGateDelay() const { return m_opts.globalGateDelay; }
    // 读取全局门延迟（供 Circuit 保存设置）
    quint64 wireDelay() const       { return m_opts.wireDelay; }
    // 读取线延迟
    quint64 defaultRunBudget() const { return m_opts.runBudget; }
    // 读取默认预算
    quint64 gateDelayFor(const QString& type) const;
    // 查询某类型的实际延迟：先查覆盖表，无则回退全局值；实现中避免重复查表
    QHash<QString, quint64> gateTypeDelays() const { return m_gateDelays; }
    // 返回覆盖表副本：用于存档序列化；返回副本避免外部修改内部状态

    void build(const QVariantList& rootComps,
               const QVariantList& rootWires,
               const QHash<QString, QVariantList>& subComps,
               const QHash<QString, QVariantList>& subWires);
    // 构建引擎：清空旧状态后递归展开子电路，建立节点、驱动/目标表，初始化值并调度初始事件
    void clear();
    // 完全清空：节点、索引、表、队列、计数器、脏集合全部重置
    void reset();
    // 软重置：保留输入元件当前值，清零其余节点值，重建初始事件；
    // 与 clear 的区别是输入状态不丢，用于"重置仿真但保留输入设置"

    void triggerInputBit(const QString& compId, int bit, bool value);
    // 修改某 input 元件的单 bit 并调度下游传播：
    // 用于 QML 逐位编辑；只改指定 bit，其余位保持
    void triggerInputString(const QString& compId, const QString& binStr);
    // 整串设置 input 值并调度传播：用于 toggleInput/setInputFromString 等批量修改
    void triggerClockToggle(const QString& compId);
    // 翻转 clock 输出并调度传播：时钟定时器每 tick 调用一次
    void triggerForceOutput(const QString& compId, int port, const QString& value);
    // 强制设置某节点某端口输出并调度传播：
    // 用于 Circuit 同步 clock 状态、或需要绕过逻辑直接注入值的场景

    bool step();
    // 处理队列中一个最早事件：推进 m_time 到事件时间，返回是否还有事件
    bool run(quint64 units, quint64 budget = 0);
    // 推进仿真时间 units：处理所有 ≤ endTime 的事件；
    // budget 为 0 时用默认预算；返回 true 表示已处理到 endTime 或队列提前清空
    int drainNow(quint64 budget = 0);
    // 只处理当前时刻（m_time）的事件，不推进时间：
    // 用于 rebuild 后立即收敛同一时刻的初始事件

    quint64 time() const { return m_time; }
    // 当前仿真时间：供 QML 显示与 Circuit::simTime
    bool isIdle() const { return m_queue.empty(); }
    // 事件队列是否为空：runEventEngineFully 据此判断是否收敛
    quint64 totalEventsProcessed() const { return m_totalEventsProcessed; }
    // 累计处理事件数：调试/性能观测
    int nodeCount() const { return m_nodes.size(); }
    // 节点数量：Circuit 判断是否需要 rebuild
    QString getOutput(const QString& compId, int port = 0) const;
    // 读取某节点某端口输出值：不存在或越界返回空串
    QStringList getOutputs(const QString& compId) const;
    // 读取某节点所有输出端口值：applyEngineOutputsToContexts 使用
    QStringList getInputs(const QString& compId) const;
    // 读取某节点所有输入端口值：子电路浏览时同步输入状态使用

    // ---- 脏节点追踪（性能优化）----
    // 每次 recalcNode 输出变化 / trigger* 修改输出时，把节点 qid 加入
    // takeDirtyQids() 取走并清空
    QVector<QString> takeDirtyQids() {
        // 取走并清空脏队列：返回 swap 出的副本，同时清空去重集合
        QVector<QString> r;
        r.swap(m_dirtyQids);
        m_dirtySet.clear();
        return r;
    }
    void clearDirtyQids() { m_dirtyQids.clear(); m_dirtySet.clear(); }
    // 主动清空脏集合：全量 apply 后调用，避免残留导致下次误判增量
    int dirtyCount() const { return m_dirtyQids.size(); }
    // 脏节点数量：调试观测

    // 去重：同一节点一次 run() 里只追加一次
    void markDirty(const QString& qid) {
        // 若已记录则直接返回，保证同一节点在脏队列中最多出现一次；
        // 避免同一帧内反复更新同一元件造成 QML 抖动
        if (m_dirtySet.contains(qid)) return;
        m_dirtySet.insert(qid);
        m_dirtyQids.append(qid);
    }

private:
    // 私有区：内部数据结构与算法实现

    struct Node {
        // 引擎内的节点：每个元件对应一个（sub 实例本身不建节点，其内部元件递归建节点）
        QString qid;
        // 限定 ID："实例路径/本地ID"；根上下文元件即本地 ID
        QString localId;
        // 本地 ID：用于从 raw 中回读字段、与上下文元件对应
        QString type;
        // 元件类型：决定 computeOutputs 分支与门延迟查询
        int bitWidth = 1;
        // 位宽：钳位后的有效值，用于字符串补零/截断
        QVariantMap raw;
        // 元件原始 map：读取 outputSplits、inputBits 等类型相关字段
        QVector<QString> inVals;
        // 各输入端口当前值：字符串形式，长度与端口位宽一致
        QVector<QString> outVals;
        // 各输出端口当前值
        bool isInput = false;
        // 是否 input/const：其输出由外部 trigger 直接写入，不参与 recalc
        bool isClock = false;
        // 是否 clock：输出由时钟定时器 triggerClockToggle 驱动
        bool isOutput = false;
        // 是否 output 探针：在子电路展开时作为边界节点，供父级读取
        bool isLed = false;
        // 是否 LED：仅显示，无输出
    };

    struct Event {
        // 事件：队列元素，按 (time, seq) 优先级排序
        enum Kind { Recalc, Wire } kind;
        // Recalc：重新计算某节点输出；Wire：把某输出值传播到某输入端口
        quint64 time;
        // 事件发生时间
        quint64 seq;
        // 同时间事件的序号：保证 FIFO 稳定顺序，避免同刻事件乱序导致结果不确定
        int     nodeIdx;
        // Recalc 时为待重算节点；Wire 时为源节点
        int     port;
        // Wire 时为源输出端口
        int     toIdx;
        // Wire 时为目标节点；Recalc 时为 -1
        int     toPort;
        // Wire 时为目标输入端口
        QString value;
        // Wire 时携带的输出值
        bool operator<(const Event& o) const {
            // 优先队列默认大顶堆，这里反转比较实现小顶堆：
            // 时间早者优先；同时间序号小者优先，保证稳定
            if (time != o.time) return time > o.time;
            return seq > o.seq;
        }
    };

    struct Driver { int fromIdx; int fromPort; };
    // 驱动源：某输入端口的一个上游 (源节点, 源输出端口)

    QVector<Node> m_nodes;
    // 节点数组：索引即 nodeIdx，build 后不再增删
    QHash<QString, int> m_idToIdx;
    // qid → 节点索引：事件与查询都通过它定位节点
    QHash<quint64, QVector<Driver>> m_inDrivers;
    // 输入端口 → 所有驱动源：key 为 pkey(节点, 输入端口)
    QHash<quint64, QVector<QPair<int,int>>> m_outDests;
    // 输出端口 → 所有目标：key 为 pkey(节点, 输出端口)，value 为 (目标节点, 目标端口) 列表
    QHash<quint64, QHash<qint64, QString>> m_portDriverValues;
    // 输入端口 → (驱动源 dkey → 该源最近一次输出值)：
    // 多驱合并时需知道每个驱动源当前值，才能正确算出 0/1/E/Z

    std::priority_queue<Event> m_queue;
    // 事件优先队列：按时间与序号排序
    quint64 m_time = 0;
    // 当前仿真时间
    quint64 m_seq = 0;
    // 事件序号发生器：每入队一个事件自增，保证同刻稳定
    quint64 m_totalEventsProcessed = 0;
    // 累计处理事件计数

    QHash<QString, quint64> m_gateDelays;
    // 按门类型的延迟覆盖表
    Options m_opts;
    // 运行参数

    QVector<QString> m_dirtyQids;
    // 脏节点队列：待 Circuit 增量 apply 的节点
    QSet<QString>    m_dirtySet;      // 去重
    // 脏节点去重集合：与 m_dirtyQids 同步维护
    QVector<QString> m_driverValBuf;   // 复用，避免热路径堆分配
    // 多驱合并临时缓冲区：processEvent 中复用，避免每事件构造 QVector

    static quint64 pkey(int idx, int port) {
        // 端口键打包：高 32 位放节点索引，低 16 位放端口号；
        // 打包成单个整数作为 QHash key，比 QPair 更快且省内存
        return (quint64(quint32(idx)) << 16) | quint64(port & 0xFFFF);
    }
    static qint64 dkey(int idx, int port) {
        // 驱动源键：与 pkey 同样布局，但用 qint64 以适配嵌套 QHash 的 value key 类型
        return (qint64(qint32(idx)) << 16) | qint64(port & 0xFFFF);
    }
    int idxOf(const QString& id) const { return m_idToIdx.value(id, -1); }
    // ID → 索引快捷查询：不存在返回 -1，调用方据此跳过

    void buildRecursive(const QVariantList& comps,
                        const QVariantList& wires,
                        const QHash<QString, QVariantList>& subComps,
                        const QHash<QString, QVariantList>& subWires,
                        const QString& prefix,
                        int depth);
    // 递归建图：先为本层元件建节点（prefix 拼 qid），再递归展开 sub 实例，
    // 最后处理本层连线；sub 元件的端口需映射到子电路内部 input/output 节点

    void initValues();
    // 初始化节点初值：input 从 inputBits 读取，clock 从 outputPorts 读取，其余置零
    void initDriverValues();
    // 初始化驱动表与各输入端口值：建立多驱合并结果，并做 32 轮迭代传播，
    // 使 build 后各节点输出接近稳定，减少首帧事件数量

    void scheduleRecalc(int idx, quint64 at);
    // 调度一次 Recalc 事件：at 时刻重算节点 idx
    void scheduleWirePropagation(int fromIdx, int fromPort,
                                 int toIdx, int toPort,
                                 const QString& value, quint64 at);
    // 调度一次 Wire 事件：at 时刻把 fromIdx 端口值传播到 (toIdx, toPort)

    void processEvent(const Event& ev);
    // 处理单个事件：Recalc 走 recalcNode；Wire 走多驱合并并可能触发 recalc
    void recalcNode(int idx);
    // 重算节点输出：与旧值比较，变化则记录脏节点并按门延迟调度下游传播

    QVector<QString> computeOutputs(const Node& n,
                                    const QVector<QString>& inputs) const;
    // 纯函数计算节点输出：数据源类型自行处理，组合逻辑委托 ComponentLogic
    static QString resolveMultiDriver(const QVector<QString>& vals, int bw);
    // 多驱合并：逐位判断 0/1 冲突（→E）、全 Z、含 E 等，返回合并结果
    static QString mapUnary(const QString& src, int bw);
    // 逐位取反并处理三态：Z/E 映射为 E，0↔1；
    // 目前主要为 not 提供支持，保留以便复用
};

#endif // EVENTENGINE_H
// 结束头文件保护