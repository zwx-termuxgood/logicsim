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

class EventEngine
{
public:
    struct Options {
        quint64 globalGateDelay = 1;
        quint64 wireDelay       = 1;
        quint64 runBudget       = 200000;
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

    void triggerInputBit(const QString& compId, int bit, bool value);
    void triggerInputString(const QString& compId, const QString& binStr);
    void triggerClockToggle(const QString& compId);
    void triggerForceOutput(const QString& compId, int port, const QString& value);

    bool step();
    bool run(quint64 units, quint64 budget = 0);
    int drainNow(quint64 budget = 0);

    quint64 time() const { return m_time; }
    bool isIdle() const { return m_queue.empty(); }
    quint64 totalEventsProcessed() const { return m_totalEventsProcessed; }
    int nodeCount() const { return m_nodes.size(); }
    QString getOutput(const QString& compId, int port = 0) const;
    QStringList getOutputs(const QString& compId) const;

    // ---- 脏节点追踪（性能优化）----
    // 每次 recalcNode 输出变化 / trigger* 修改输出时，把节点 qid 加入
    // takeDirtyQids() 取走并清空
    QVector<QString> takeDirtyQids() {
        QVector<QString> r;
        r.swap(m_dirtyQids);
        return r;
    }
    void clearDirtyQids() { m_dirtyQids.clear(); }
    int dirtyCount() const { return m_dirtyQids.size(); }

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
        int     nodeIdx;
        int     port;
        int     toIdx;
        int     toPort;
        QString value;
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

    QVector<QString> m_dirtyQids;

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
