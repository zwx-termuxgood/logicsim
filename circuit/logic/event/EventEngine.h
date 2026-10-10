#ifndef EVENTENGINE_H
#define EVENTENGINE_H

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QVector>
#include <QPair>
#include <queue>
#include <cstdint>

// 事件驱动电路仿真引擎
// 关键设计：
// - 时间只随事件推进；没有事件时时间可安全跳过空隙
// - run(units, budget)：推进 units 个逻辑时间单位，最多处理 budget 个事件
//     预算耗尽 → 时间停在最后处理的事件时间，返回 false，下次继续
//     处理完 → 时间推进到 m_time+units，返回 true
// - 没有 runIdle：不存在"跑到稳定"这回事，振荡电路永远跑不完
// - initDriverValues 末尾给所有非 input/clock 节点排一个 recalc 事件，
//   作为振荡电路的引爆点；否则自环电路启动不了
class EventEngine
{
public:
    struct Options {
        quint64 globalGateDelay = 1;
        quint64 wireDelay       = 1;
        quint64 runBudget       = 200000;   // 默认每帧事件预算
    };

    EventEngine();
    ~EventEngine();

    void setGlobalGateDelay(quint64 d);
    void setWireDelay(quint64 d);
    void setGateTypeDelay(const QString& type, quint64 d);
    void removeGateTypeDelay(const QString& type);
    void clearGateTypeDelays();
    void setDefaultRunBudget(quint64 n) { if (n < 1) n = 1; m_opts.runBudget = n; }

    quint64 globalGateDelay() const { return m_opts.globalGateDelay; }
    quint64 wireDelay() const       { return m_opts.wireDelay; }
    quint64 defaultRunBudget() const { return m_opts.runBudget; }
    quint64 gateDelayFor(const QString& type) const;
    QHash<QString, quint64> gateTypeDelays() const { return m_gateDelays; }

    void build(const QVariantList& rootComps,
               const QVariantList& rootWires,
               const QHash<QString, QVariantList>& subComps,
               const QHash<QString, QVariantList>& subWires);
    void clear();
    void reset();

    // ---- 外部触发 ----
    void triggerInputBit(const QString& compId, int bit, bool value);
    void triggerInputString(const QString& compId, const QString& binStr);
    void triggerClockToggle(const QString& compId);
    void triggerForceOutput(const QString& compId, int port, const QString& value);

    // ---- 推进 ----
    // 处理一个事件；队列为空返回 false
    bool step();

    // 推进 units 个逻辑时间单位；最多处理 budget 个事件
    // 返回 true  = 时间已推进到 m_time+units（窗口内无更多事件 / 队列空）
    // 返回 false = 预算耗尽，时间停在最后处理的事件时间
    bool run(quint64 units, quint64 budget = 0);

    // 只处理 m_time 时刻的已排队事件，不推进时间
    int drainNow(quint64 budget = 0);

    // ---- 查询 ----
    quint64 time() const { return m_time; }
    bool isIdle() const { return m_queue.empty(); }
    quint64 totalEventsProcessed() const { return m_totalEventsProcessed; }
    int nodeCount() const { return m_nodes.size(); }
    QString getOutput(const QString& compId, int port = 0) const;
    QStringList getOutputs(const QString& compId) const;

private:
    struct Node {
        QString qid;
        QString localId;
        QString type;
        int bitWidth = 1;
        QVariantMap raw;
        QVector<QString> inVals;
        QVector<QString> outVals;
        bool isInput = false;
        bool isClock = false;
        bool isOutput = false;
        bool isLed = false;
    };

    struct Event {
        enum Kind { Recalc, Wire } kind;
        quint64 time;
        quint64 seq;
        int     nodeIdx;   // Recalc: 目标节点；Wire: 源节点
        int     port;      // Wire: 源端口
        int     toIdx;     // Wire: 目标节点
        int     toPort;    // Wire: 目标端口
        QString value;     // Wire: 携带的值
        bool operator<(const Event& o) const {
            if (time != o.time) return time > o.time;
            return seq > o.seq;
        }
    };

    struct Driver { int fromIdx; int fromPort; };

    QVector<Node> m_nodes;
    QHash<QString, int> m_idToIdx;
    QHash<quint64, QVector<Driver>> m_inDrivers;
    QHash<quint64, QVector<QPair<int,int>>> m_outDests;
    QHash<quint64, QHash<qint64, QString>> m_portDriverValues;

    std::priority_queue<Event> m_queue;
    quint64 m_time = 0;
    quint64 m_seq = 0;
    quint64 m_totalEventsProcessed = 0;

    QHash<QString, quint64> m_gateDelays;
    Options m_opts;

    static quint64 pkey(int idx, int port) {
        return (quint64(quint32(idx)) << 16) | quint64(port & 0xFFFF);
    }
    static qint64 dkey(int idx, int port) {
        return (qint64(qint32(idx)) << 16) | qint64(port & 0xFFFF);
    }
    int idxOf(const QString& id) const { return m_idToIdx.value(id, -1); }

    void buildRecursive(const QVariantList& comps,
                        const QVariantList& wires,
                        const QHash<QString, QVariantList>& subComps,
                        const QHash<QString, QVariantList>& subWires,
                        const QString& prefix,
                        int depth);

    void initValues();
    void initDriverValues();

    void scheduleRecalc(int idx, quint64 at);
    void scheduleWirePropagation(int fromIdx, int fromPort,
                                 int toIdx, int toPort,
                                 const QString& value, quint64 at);

    void processEvent(const Event& ev);
    void recalcNode(int idx);

    QVector<QString> computeOutputs(const Node& n,
                                    const QVector<QString>& inputs) const;
    static QString resolveMultiDriver(const QVector<QString>& vals, int bw);
    static QString mapUnary(const QString& src, int bw);
};

#endif // EVENTENGINE_H
