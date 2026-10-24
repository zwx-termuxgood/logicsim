#include "Connector.h"
#include "MoveGesture.h"
#include "AvoidanceMap.h"
#include "SearchNode.h"
#include "ConnectionData.h"
#include "ConnectorThread.h"
#include "../../core/CircuitContext.h"
#include <QSet>
#include <QHash>
#include <QList>
#include <QDateTime>
#include <algorithm>
#include <random>
#include <cmath>
#include <QElapsedTimer>

static QString dirReverse(const QString& d) {
    if (d == "east") return "west";
    if (d == "west") return "east";
    if (d == "north") return "south";
    if (d == "south") return "north";
    return QString();
}

static QString dirLeft(const QString& d) {
    if (d == "east") return "north";
    if (d == "west") return "south";
    if (d == "north") return "west";
    if (d == "south") return "east";
    return QString();
}

static QString dirRight(const QString& d) {
    if (d == "east") return "south";
    if (d == "west") return "north";
    if (d == "north") return "east";
    if (d == "south") return "west";
    return QString();
}

// 沿 wire 生成所有网格点（用于 processConnection 的起点）
static QList<QVariantMap> wireLocations(const QVariantMap& wire) {
    QList<QVariantMap> out;
    QVariantList path = wire.value("path").toList();
    for (int i = 0; i + 1 < path.size(); ++i) {
        QVariantMap p0 = path[i].toMap();
        QVariantMap p1 = path[i + 1].toMap();
        double x0 = p0.value("x").toDouble(), y0 = p0.value("y").toDouble();
        double x1 = p1.value("x").toDouble(), y1 = p1.value("y").toDouble();
        double len = std::hypot(x1 - x0, y1 - y0);
        int n = int(len / 10);
        if (n < 1) n = 1;
        for (int s = 0; s <= n; ++s) {
            double t = double(s) / n;
            QVariantMap loc;
            loc["x"] = x0 + (x1 - x0) * t;
            loc["y"] = y0 + (y1 - y0) * t;
            out.append(loc);
        }
    }
    return out;
}

static void processConnection(const ConnectionData& conn, int dx, int dy,
                              QList<QVariantMap>& connLocs,
                              QList<SearchNode*>& connNodes,
                              AvoidanceMap* selAvoid,
                              QList<SearchNode*>& allNodes)
{
    QVariantMap cur = conn.location();
    QVariantMap dest;
    dest["x"] = cur.value("x").toDouble() + dx;
    dest["y"] = cur.value("y").toDouble() + dy;

    if (selAvoid->get(cur.value("x").toDouble(), cur.value("y").toDouble()).isEmpty()) {
        QString preferred = conn.direction();
        if (preferred.isEmpty()) {
            if (std::abs(dx) > std::abs(dy))
                preferred = dx > 0 ? "east" : "west";
            else
                preferred = dy > 0 ? "south" : "north";
        }
        connLocs.append(cur);
        SearchNode* n = new SearchNode(nullptr, cur, preferred, dest);
        connNodes.append(n);
        allNodes.append(n);
    }

    for (const auto& wire : conn.wirePath()) {
        QList<QVariantMap> locs = wireLocations(wire);
        for (const auto& loc : locs) {
            QString allow = selAvoid->get(loc.value("x").toDouble(), loc.value("y").toDouble());
            bool isDest = std::abs(loc.value("x").toDouble() - dest.value("x").toDouble()) < 0.01
                          && std::abs(loc.value("y").toDouble() - dest.value("y").toDouble()) < 0.01;
            if (allow.isEmpty() || isDest) {
                bool already = false;
                for (const auto& l : connLocs) {
                    if (l == loc) { already = true; break; }
                }
                if (!already) {
                    connLocs.append(loc);
                    QString dir;
                    QVariantList path = wire.value("path").toList();
                    if (!path.isEmpty()) {
                        QVariantMap p0 = path.first().toMap();
                        QVariantMap p1 = path.last().toMap();
                        bool isEnd = (std::abs(loc.value("x").toDouble() - p0.value("x").toDouble()) < 0.01 &&
                                      std::abs(loc.value("y").toDouble() - p0.value("y").toDouble()) < 0.01)
                                     || (std::abs(loc.value("x").toDouble() - p1.value("x").toDouble()) < 0.01 &&
                                         std::abs(loc.value("y").toDouble() - p1.value("y").toDouble()) < 0.01);
                        if (isEnd) {
                            double wx = p1.value("x").toDouble() - p0.value("x").toDouble();
                            double wy = p1.value("y").toDouble() - p0.value("y").toDouble();
                            if (std::abs(wx) > std::abs(wy))
                                dir = wx > 0 ? "west" : "east";
                            else
                                dir = wy > 0 ? "north" : "south";
                        }
                    }
                    SearchNode* n = new SearchNode(nullptr, loc, dir, dest);
                    connNodes.append(n);
                    allNodes.append(n);
                }
            }
        }
    }
}

static SearchNode* findShortestPath(QList<SearchNode*>& nodes,
                                    const QList<QVariantMap>& pathLocs,
                                    AvoidanceMap* avoid)
{
    const int MAX_ITERS = 20000;
    QList<SearchNode*> q;
    for (auto* n : nodes) q.append(n);
    std::sort(q.begin(), q.end(),
              [](SearchNode* a, SearchNode* b) {
                  return a->heuristicValue() < b->heuristicValue();
              });
    QSet<int> visited;
    int iters = 0;
    while (!q.isEmpty() && iters < MAX_ITERS) {
        iters++;
        if (iters % 64 == 0 && ConnectorThread::isOverrideRequested()) return nullptr;
        SearchNode* node = q.takeFirst();
        if (!node) return nullptr;
        if (node->isDestination()) return node;
        int h = node->hash();
        if (visited.contains(h)) continue;
        visited.insert(h);

        QString dir = node->direction();
        int neighbors = 3;
        QVariantMap loc = node->location();
        QString allowed = avoid->get(loc.value("x").toDouble(), loc.value("y").toDouble());

        bool isStart = node->isStart();
        bool inPathLocs = false;
        if (isStart) {
            for (const auto& pl : pathLocs) {
                if (pl == loc) { inPathLocs = true; break; }
            }
        }
        if (!allowed.isEmpty() && isStart && inPathLocs) allowed.clear();

        if (allowed == "neither") neighbors = 0;
        else if (allowed == "vert") {
            if (dir.isEmpty()) { dir = "north"; neighbors = 2; }
            else if (dir == "north" || dir == "south") neighbors = 1;
            else neighbors = 0;
        } else if (allowed == "horz") {
            if (dir.isEmpty()) { dir = "east"; neighbors = 2; }
            else if (dir == "east" || dir == "west") neighbors = 1;
            else neighbors = 0;
        } else {
            if (dir.isEmpty()) { dir = "north"; neighbors = 4; }
            else neighbors = 3;
        }

        for (int i = 0; i < neighbors; ++i) {
            QString oDir;
            if (i == 0) oDir = dir;
            else if (i == 1) oDir = (neighbors == 2) ? dirReverse(dir) : dirLeft(dir);
            else if (i == 2) oDir = dirRight(dir);
            else oDir = dirReverse(dir);

            SearchNode* nextNode = node->next(oDir, !allowed.isEmpty());
            if (nextNode && !visited.contains(nextNode->hash())) {
                q.append(nextNode);
                std::sort(q.begin(), q.end(),
                          [](SearchNode* a, SearchNode* b) {
                              return a->heuristicValue() < b->heuristicValue();
                          });
            }
        }
    }
    return nullptr;
}

static QList<QVariantMap> convertToPath(SearchNode* last) {
    QList<QVariantMap> ret;
    SearchNode* next = last;
    SearchNode* prev = last->previous();
    ret.append(next->location());
    while (prev != nullptr) {
        if (prev->direction() != next->direction()) {
            ret.append(prev->location());
        }
        next = prev;
        prev = prev->previous();
    }
    if (ret.last() != next->location()) {
        ret.append(next->location());
    }
    std::reverse(ret.begin(), ret.end());
    return ret;
}

static void processPath(const QList<QVariantMap>& path, const ConnectionData& conn,
                        AvoidanceMap* avoid, MoveResult* result)
{
    int idx = 0;
    QVariantMap loc0 = path[idx++];
    QVariantMap connLoc = conn.location();

    if (loc0 != connLoc) {
        QVariantMap pathLoc = conn.wirePathStart();
        bool found = (loc0 == pathLoc);

        for (const auto& wire : conn.wirePath()) {
            QVariantList wpath = wire.value("path").toList();
            QVariantMap wireEnd = wpath.isEmpty() ? QVariantMap() : wpath.last().toMap();
            QVariantMap nextLoc = wireEnd;
            if (wpath.size() > 0) {
                QVariantMap f = wpath.first().toMap();
                QVariantMap l = wpath.last().toMap();
                if (pathLoc == f) nextLoc = l;
                else nextLoc = f;
            }

            if (found) {
                result->removals.append(wire);
                avoid->unmarkWire(wire, nextLoc);
            } else {
                bool contains = false;
                for (const auto& pv : wpath) {
                    if (pv.toMap() == loc0) { contains = true; break; }
                }
                if (contains) {
                    found = true;
                    if (loc0 != nextLoc) {
                        avoid->unmarkWire(wire, nextLoc);
                        QVariantMap shortened;
                        shortened["wireType"] = "ortho";
                        QVariantList newPath;
                        newPath.append(pathLoc);
                        newPath.append(loc0);
                        shortened["path"] = newPath;
                        QVariantMap oldW = wire;
                        result->replacements.append({oldW, shortened});
                        avoid->markWire(shortened, 0, 0);
                    }
                }
            }
            pathLoc = nextLoc;
        }
    }

    while (idx < path.size()) {
        QVariantMap loc1 = path[idx++];
        QVariantMap newWire;
        newWire["wireType"] = "ortho";
        QVariantList newPath;
        newPath.append(loc0);
        newPath.append(loc1);
        newWire["path"] = newPath;
        result->additions.append(newWire);
        avoid->markWire(newWire, 0, 0);
        loc0 = loc1;
    }
}

static void sortConnects(QList<ConnectionData>& connects, int dx, int dy) {
    std::sort(connects.begin(), connects.end(),
              [dx, dy](const ConnectionData& a, const ConnectionData& b) {
                  QVariantMap l1 = a.location();
                  QVariantMap l2 = b.location();
                  double ax = l1.value("x").toDouble() - l2.value("x").toDouble();
                  double ay = l1.value("y").toDouble() - l2.value("y").toDouble();
                  return (ax * dx + ay * dy) < 0;
              });
}

static MoveResult* tryList(const MoveRequest& req, MoveGesture* gesture,
                           QList<ConnectionData>& connects, int dx, int dy,
                           QHash<int, QList<QVariantMap>>& pathLocs,
                           QHash<int, QList<SearchNode*>>& initNodes,
                           qint64 stopTime)
{
    AvoidanceMap* avoid = gesture->getFixedAvoidanceMap()->cloneMap();
    avoid->markAll(gesture->getSelected(), dx, dy);

    MoveResult* result = new MoveResult();

    for (const auto& conn : connects) {
        if (ConnectorThread::isOverrideRequested()) { delete avoid; delete result; return nullptr; }
        if (QDateTime::currentMSecsSinceEpoch() > stopTime) {
            result->unsatisfied.append(conn);
            continue;
        }
        int h = conn.hash();
        QList<SearchNode*>& connNodes = initNodes[h];
        QList<QVariantMap>& connPathLocs = pathLocs[h];
        SearchNode* node = findShortestPath(connNodes, connPathLocs, avoid);
        if (node) {
            result->totalDistance += node->distance();
            QList<QVariantMap> path = convertToPath(node);
            processPath(path, conn, avoid, result);
        } else if (ConnectorThread::isOverrideRequested()) {
            delete avoid; delete result; return nullptr;
        } else {
            result->unsatisfied.append(conn);
        }
    }
    delete avoid;
    return result;
}

MoveResult* Connector::computeWires(const MoveRequest& req) {
    MoveGesture* gesture = req.gesture();
    if (!gesture) return new MoveResult();

    int dx = req.dx(), dy = req.dy();
    QList<ConnectionData> baseConnects = gesture->getConnections();
    AvoidanceMap* fixedAvoid = gesture->getFixedAvoidanceMap();

    // 剔除不可能的连接
    QList<ConnectionData> connects;
    for (const auto& conn : baseConnects) {
        QVariantMap dest;
        dest["x"] = conn.location().value("x").toDouble() + dx;
        dest["y"] = conn.location().value("y").toDouble() + dy;
        QString avoidVal = fixedAvoid->get(dest.value("x").toDouble(), dest.value("y").toDouble());
        if (!avoidVal.isEmpty()) {
            bool inPath = false;
            for (const auto& wire : conn.wirePath()) {
                QVariantList path = wire.value("path").toList();
                for (const auto& pv : path) {
                    QVariantMap p = pv.toMap();
                    if (p == dest) { inPath = true; break; }
                }
                if (inPath) break;
            }
            if (!inPath) continue;
        }
        connects.append(conn);
    }

    AvoidanceMap* selAvoid = AvoidanceMap::create(gesture->getSelected(), dx, dy);
    QHash<int, QList<QVariantMap>> pathLocs;
    QHash<int, QList<SearchNode*>> initNodes;
    QList<SearchNode*> allNodes;

    for (const auto& conn : connects) {
        QList<QVariantMap> connLocs;
        QList<SearchNode*> connNodes;
        processConnection(conn, dx, dy, connLocs, connNodes, selAvoid, allNodes);
        pathLocs[conn.hash()] = connLocs;
        initNodes[conn.hash()] = connNodes;
    }
    delete selAvoid;

    int tries;
    if (connects.size() == 0) tries = 0;
    else if (connects.size() == 1) tries = 1;
    else if (connects.size() == 2) tries = 2;
    else if (connects.size() == 3) tries = 8;
    else tries = 10;

    qint64 stopTime = QDateTime::currentMSecsSinceEpoch() + 10000;

    MoveResult* bestResult = nullptr;
    for (int tryNum = 0; tryNum < tries; ++tryNum) {
        if (ConnectorThread::isOverrideRequested()) {
            if (bestResult) delete bestResult;
            for (auto* n : allNodes) delete n;
            return nullptr;
        }
        if (QDateTime::currentMSecsSinceEpoch() > stopTime) break;

        QList<ConnectionData> copy = connects;
        if (tryNum < 2) {
            sortConnects(copy, dx, dy);
            if (tryNum == 1) std::reverse(copy.begin(), copy.end());
        } else {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(copy.begin(), copy.end(), g);
        }

        MoveResult* candidate = tryList(req, gesture, copy, dx, dy, pathLocs, initNodes, stopTime);
        if (!candidate) {
            if (bestResult) delete bestResult;
            for (auto* n : allNodes) delete n;
            return nullptr;
        }
        if (!bestResult) bestResult = candidate;
        else {
            if (candidate->unsatisfied.size() < bestResult->unsatisfied.size())
            { delete bestResult; bestResult = candidate; }
            else if (candidate->unsatisfied.size() == bestResult->unsatisfied.size() &&
                     candidate->totalDistance < bestResult->totalDistance)
            { delete bestResult; bestResult = candidate; }
            else delete candidate;
        }
    }

    if (!bestResult) bestResult = new MoveResult();

    for (auto* n : allNodes) delete n;
    return bestResult;
}