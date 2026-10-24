#include "MoveGesture.h"
#include "AvoidanceMap.h"
#include "ConnectorThread.h"
#include "../ComponentLogic.h"
#include "../../core/CircuitContext.h"
#include <QSet>
#include <QPointF>
#include <cmath>
#include <QMutex>
#include <QMutexLocker>

static QMutex g_gestureMutex;

MoveGesture::MoveGesture(const CircuitContext* circuit,
                         const QList<QVariantMap>& selectedComponents)
: m_circuit(circuit), m_selected(selectedComponents) {}

MoveGesture::~MoveGesture() {
    for (auto* r : m_cachedResults) delete r;
    delete m_initAvoid;
}

static bool isSelected(const QList<QVariantMap>& selected, const QString& id) {
    for (const auto& s : selected)
        if (s.value("id").toString() == id) return true;
        return false;
}

const QList<ConnectionData>& MoveGesture::getConnections() const {
    if (!m_connectionsValid) {
        const_cast<MoveGesture*>(this)->computeConnections();
        m_connectionsValid = true;
    }
    return m_connections;
}

void MoveGesture::computeConnections() {
    m_connections.clear();
    if (m_selected.isEmpty()) return;

    // 收集所有选中元件的端口位置
    QList<QPointF> locs;
    for (const auto& c : m_selected) {
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();
        QVariantList outs = ComponentLogic::outputPorts(c);
        for (const auto& pv : outs) {
            QVariantMap p = pv.toMap();
            locs.append(QPointF(cx + p.value("x").toDouble(),
                                cy + p.value("y").toDouble()));
        }
        QVariantList ins = ComponentLogic::inputPorts(c);
        for (const auto& pv : ins) {
            QVariantMap p = pv.toMap();
            locs.append(QPointF(cx + p.value("x").toDouble(),
                                cy + p.value("y").toDouble()));
        }
    }

    // 对每个位置，判断是否连接到非选中组件
    for (const auto& loc : locs) {
        bool found = false;
        // 遍历所有"非选中组件"，检查其端口是否在 loc 上
        for (const auto& cv : m_circuit->components) {
            QVariantMap c = cv.toMap();
            if (isSelected(m_selected, c.value("id").toString())) continue;
            double cx = c.value("x").toDouble();
            double cy = c.value("y").toDouble();
            QVariantList outs = ComponentLogic::outputPorts(c);
            for (const auto& pv : outs) {
                QVariantMap p = pv.toMap();
                if (qAbs(cx + p.value("x").toDouble() - loc.x()) < 0.5 &&
                    qAbs(cy + p.value("y").toDouble() - loc.y()) < 0.5) {
                    found = true; break;
                    }
            }
            if (found) break;
            QVariantList ins = ComponentLogic::inputPorts(c);
            for (const auto& pv : ins) {
                QVariantMap p = pv.toMap();
                if (qAbs(cx + p.value("x").toDouble() - loc.x()) < 0.5 &&
                    qAbs(cy + p.value("y").toDouble() - loc.y()) < 0.5) {
                    found = true; break;
                    }
            }
            if (found) break;
        }
        if (!found) continue;

        // 找 wirePath：从这个位置出发，沿非选中 wire 走
        QList<QVariantMap> wirePath;
        QPointF pathStart = loc;
        QVariantMap lastOnPath;
        QSet<QString> visited;
        QPointF cur = loc;

        // 找第一条经过 cur 的、非选中的正交 wire
        auto findWire = [&](const QPointF& pt, const QString& ignoreId) -> QVariantMap {
            for (const auto& wv : m_circuit->wires) {
                QVariantMap w = wv.toMap();
                QString id = w.value("id").toString();
                if (id == ignoreId) continue;
                if (visited.contains(id)) continue;
                QString wt = w.value("wireType").toString();
                if (wt != "ortho") continue;
                // 检查 wire 是否有 path 顶点在 pt 上
                QVariantList path = w.value("path").toList();
                for (const auto& pv : path) {
                    QVariantMap p = pv.toMap();
                    if (qAbs(p.value("x").toDouble() - pt.x()) < 0.5 &&
                        qAbs(p.value("y").toDouble() - pt.y()) < 0.5) {
                        return w;
                        }
                }
            }
            return QVariantMap();
        };

        QVariantMap first = findWire(cur, QString());
        if (!first.isEmpty()) {
            visited.insert(first.value("id").toString());
            wirePath.append(first);
            lastOnPath = first;

            // 沿 path 向外走
            QVariantList path = first.value("path").toList();
            // 找 cur 在 path 中的位置
            int curIdx = -1;
            for (int i = 0; i < path.size(); ++i) {
                QVariantMap p = path[i].toMap();
                if (qAbs(p.value("x").toDouble() - cur.x()) < 0.5 &&
                    qAbs(p.value("y").toDouble() - cur.y()) < 0.5) {
                    curIdx = i; break;
                    }
            }
            if (curIdx >= 0) {
                if (curIdx == 0) {
                    // 从 path.first 开始，向 path.last 走
                    QVariantMap lastP = path.last().toMap();
                    cur = QPointF(lastP.value("x").toDouble(), lastP.value("y").toDouble());
                } else if (curIdx == path.size() - 1) {
                    QVariantMap firstP = path.first().toMap();
                    cur = QPointF(firstP.value("x").toDouble(), firstP.value("y").toDouble());
                }
            }

            // 继续向外找
            for (int iter = 0; iter < 100; ++iter) {
                QVariantMap next = findWire(cur, QString());
                if (next.isEmpty()) break;
                visited.insert(next.value("id").toString());
                wirePath.append(next);
                QVariantList p2 = next.value("path").toList();
                int idx2 = -1;
                for (int i = 0; i < p2.size(); ++i) {
                    QVariantMap p = p2[i].toMap();
                    if (qAbs(p.value("x").toDouble() - cur.x()) < 0.5 &&
                        qAbs(p.value("y").toDouble() - cur.y()) < 0.5) {
                        idx2 = i; break;
                        }
                }
                if (idx2 < 0) break;
                if (idx2 == 0) {
                    QVariantMap lp = p2.last().toMap();
                    cur = QPointF(lp.value("x").toDouble(), lp.value("y").toDouble());
                } else if (idx2 == p2.size() - 1) {
                    QVariantMap fp = p2.first().toMap();
                    cur = QPointF(fp.value("x").toDouble(), fp.value("y").toDouble());
                } else break;
            }
            pathStart = cur;
        }

        // 计算方向
        QString dir;
        if (!lastOnPath.isEmpty()) {
            QVariantList lp = lastOnPath.value("path").toList();
            if (lp.size() >= 2) {
                // 取 loc 附近的顶点与相邻点
                double dx = 0, dy = 0;
                QVariantMap a = lp.first().toMap();
                QVariantMap b = lp.last().toMap();
                dx = a.value("x").toDouble() - b.value("x").toDouble();
                dy = a.value("y").toDouble() - b.value("y").toDouble();
                if (std::abs(dx) > std::abs(dy))
                    dir = dx > 0 ? "east" : "west";
                else
                    dir = dy > 0 ? "south" : "north";
            }
        }

        QVariantMap locMap;
        locMap["x"] = loc.x();
        locMap["y"] = loc.y();
        QVariantMap startMap;
        startMap["x"] = pathStart.x();
        startMap["y"] = pathStart.y();

        m_connections.append(ConnectionData(locMap, dir, wirePath, startMap));
    }
}

AvoidanceMap* MoveGesture::getFixedAvoidanceMap() const {
    if (m_initAvoid) return m_initAvoid;

    QList<QVariantMap> elements;
    QSet<QString> selIds;
    for (const auto& s : m_selected) selIds.insert(s.value("id").toString());

    // 所有非选中组件
    for (const auto& cv : m_circuit->components) {
        QVariantMap c = cv.toMap();
        if (c.value("type").toString() == "text") continue;
        if (selIds.contains(c.value("id").toString())) continue;
        elements.append(c);
    }
    // 所有非选中 wire
    for (const auto& wv : m_circuit->wires) {
        QVariantMap w = wv.toMap();
        QString from = w.value("fromComp").toString();
        QString to = w.value("toComp").toString();
        if (selIds.contains(from) || selIds.contains(to)) continue;
        if (w.value("wireType").toString() != "ortho") continue;
        elements.append(w);
    }

    m_initAvoid = AvoidanceMap::create(elements, 0, 0);
    return m_initAvoid;
}

void MoveGesture::notifyResult(const MoveRequest& req, MoveResult* result) {
    QMutexLocker lk(&g_gestureMutex);
    m_cachedResults[req.hash()] = result;
}

MoveResult* MoveGesture::findResult(int dx, int dy) {
    QMutexLocker lk(&g_gestureMutex);
    MoveRequest req(const_cast<MoveGesture*>(this), dx, dy);
    return m_cachedResults.value(req.hash(), nullptr);
}

bool MoveGesture::enqueueRequest(int dx, int dy) {
    MoveRequest req(this, dx, dy);
    {
        QMutexLocker lk(&g_gestureMutex);
        if (m_cachedResults.contains(req.hash())) return false;
    }
    ConnectorThread::instance()->enqueueRequest(req, false);
    return true;
}

MoveResult* MoveGesture::forceRequest(int dx, int dy) {
    MoveRequest req(this, dx, dy);
    ConnectorThread::instance()->enqueueRequest(req, true);
    QMutexLocker lk(&g_gestureMutex);
    while (!m_cachedResults.contains(req.hash())) {
        lk.unlock();
        QThread::msleep(5);
        lk.relock();
    }
    return m_cachedResults.value(req.hash(), nullptr);
}
