#include "Connector.h"
// 引入本文件要实现的 Connector 类完整声明：定义其成员函数前必须看到类定义
#include "MoveGesture.h"
// 引入移动手势完整定义：computeWires 需要访问连接描述、选中元件、固定避让地图
#include "AvoidanceMap.h"
// 引入避让地图完整定义：A* 寻路读取/标记网格占用需要完整类型
#include "SearchNode.h"
// 引入搜索节点完整定义：A* 扩展与回溯路径需要构造/访问节点
#include "ConnectionData.h"
// 引入连接数据完整定义：processConnection/processPath 访问位置、方向、wire 链
#include "ConnectorThread.h"
// 引入连接线程：查询 isOverrideRequested 以便及时中断过期计算
#include "core/CircuitContext.h"
// 引入电路上下文完整定义：本文件按需访问（保持与移动模块其它文件一致依赖）
#include <QSet>
// 引入 QSet：A* 用 visited 集合按状态哈希去重
#include <QHash>
// 引入 QHash：pathLocs/initNodes 以连接哈希为 key 组织起点信息
#include <QList>
// 引入 QList：连接列表、节点列表、路径点列表都用它
#include <QDateTime>
// 引入 QDateTime：用当前毫秒时间实现计算超时与截止判断
#include <algorithm>
// 引入算法库：std::sort/std::reverse/std::shuffle 在此使用
#include <random>
// 引入随机库：多轮尝试时打乱连接顺序，探索不同方案
#include <cmath>
// 引入 C 数学库：std::abs/std::hypot 用于距离与长度计算
#include <QElapsedTimer>
// 引入 QElapsedTimer：本文件按需使用（计时工具，保留以对齐包含）

// 定义"取反方向"辅助函数：east↔west、north↔south，其它返回空串
static QString dirReverse(const QString& d) {
    // 东的反向是西
    if (d == "east") return "west";
    // 西的反向是东
    if (d == "west") return "east";
    // 北的反向是南
    if (d == "north") return "south";
    // 南的反向是北
    if (d == "south") return "north";
    // 未知方向返回空串
    return QString();
}

// 定义"取左转方向"辅助函数
static QString dirLeft(const QString& d) {
    // 向东左转是北
    if (d == "east") return "north";
    // 向西左转是南
    if (d == "west") return "south";
    // 向北左转是西
    if (d == "north") return "west";
    // 向南左转是东
    if (d == "south") return "east";
    // 未知方向返回空串
    return QString();
}

// 定义"取右转方向"辅助函数
static QString dirRight(const QString& d) {
    // 向东右转是南
    if (d == "east") return "south";
    // 向西右转是北
    if (d == "west") return "north";
    // 向北右转是东
    if (d == "north") return "east";
    // 向南右转是西
    if (d == "south") return "west";
    // 未知方向返回空串
    return QString();
}

// 沿 wire 生成所有网格点（用于 processConnection 的起点）
static QList<QVariantMap> wireLocations(const QVariantMap& wire) {
    // 准备输出点列表
    QList<QVariantMap> out;
    // 取顶点列表
    QVariantList path = wire.value("path").toList();
    // 相邻顶点两两成段
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 段起点
        QVariantMap p0 = path[i].toMap();
        // 段终点
        QVariantMap p1 = path[i + 1].toMap();
        // 段起点坐标
        double x0 = p0.value("x").toDouble(), y0 = p0.value("y").toDouble();
        // 段终点坐标
        double x1 = p1.value("x").toDouble(), y1 = p1.value("y").toDouble();
        // 段长度
        double len = std::hypot(x1 - x0, y1 - y0);
        // 采样步数（每 10px 一步）
        int n = int(len / 10);
        // 除零保护
        if (n < 1) n = 1;
        // 逐采样点生成
        for (int s = 0; s <= n; ++s) {
            // 归一化参数
            double t = double(s) / n;
            // 采样点 map
            QVariantMap loc;
            // 线性插值 x
            loc["x"] = x0 + (x1 - x0) * t;
            // 线性插值 y
            loc["y"] = y0 + (y1 - y0) * t;
            // 追加
            out.append(loc);
        }
    }
    // 返回所有采样点
    return out;
}

// 定义"为一条连接生成 A* 起点"辅助函数
static void processConnection(const ConnectionData& conn, int dx, int dy,
                              QList<QVariantMap>& connLocs,
                              QList<SearchNode*>& connNodes,
                              AvoidanceMap* selAvoid,
                              QList<SearchNode*>& allNodes)
{
    // 连接当前位置
    QVariantMap cur = conn.location();
    // 计算移动后的目标位置
    QVariantMap dest;
    dest["x"] = cur.value("x").toDouble() + dx;
    dest["y"] = cur.value("y").toDouble() + dy;

    // 当前位置未被选中元件新位置占用：可作起点
    if (selAvoid->get(cur.value("x").toDouble(), cur.value("y").toDouble()).isEmpty()) {
        // 首选方向
        QString preferred = conn.direction();
        // 无首选方向则按位移主分量推断
        if (preferred.isEmpty()) {
            if (std::abs(dx) > std::abs(dy))
                preferred = dx > 0 ? "east" : "west";
            else
                preferred = dy > 0 ? "south" : "north";
        }
        // 记录该起点
        connLocs.append(cur);
        // 以该起点构造搜索节点
        SearchNode* n = new SearchNode(nullptr, cur, preferred, dest);
        // 加入本连接节点列表
        connNodes.append(n);
        // 加入全部节点列表（统一释放）
        allNodes.append(n);
    }

    // 沿连接已有的 wire 链生成候选起点
    for (const auto& wire : conn.wirePath()) {
        // 取该 wire 的所有网格采样点
        QList<QVariantMap> locs = wireLocations(wire);
        // 逐个采样点判断
        for (const auto& loc : locs) {
            // 查询该点占用
            QString allow = selAvoid->get(loc.value("x").toDouble(), loc.value("y").toDouble());
            // 该点是否就是目标位置
            bool isDest = std::abs(loc.value("x").toDouble() - dest.value("x").toDouble()) < 0.01
                          && std::abs(loc.value("y").toDouble() - dest.value("y").toDouble()) < 0.01;
            // 未被占用或是目标位置，才可作为起点
            if (allow.isEmpty() || isDest) {
                // 去重标记
                bool already = false;
                // 检查是否已收集该点
                for (const auto& l : connLocs) {
                    if (l == loc) { already = true; break; }
                }
                // 未收集过才加入
                if (!already) {
                    // 记录候选起点
                    connLocs.append(loc);
                    // 该起点的首选方向
                    QString dir;
                    // 取该 wire 的 path
                    QVariantList path = wire.value("path").toList();
                    // 有 path 才推断方向
                    if (!path.isEmpty()) {
                        // 首顶点
                        QVariantMap p0 = path.first().toMap();
                        // 末顶点
                        QVariantMap p1 = path.last().toMap();
                        // 该点是否为 wire 端点
                        bool isEnd = (std::abs(loc.value("x").toDouble() - p0.value("x").toDouble()) < 0.01 &&
                                      std::abs(loc.value("y").toDouble() - p0.value("y").toDouble()) < 0.01)
                                     || (std::abs(loc.value("x").toDouble() - p1.value("x").toDouble()) < 0.01 &&
                                         std::abs(loc.value("y").toDouble() - p1.value("y").toDouble()) < 0.01);
                        // 端点按 wire 走向取"向外"方向
                        if (isEnd) {
                            // wire 走向 x 分量
                            double wx = p1.value("x").toDouble() - p0.value("x").toDouble();
                            // wire 走向 y 分量
                            double wy = p1.value("y").toDouble() - p0.value("y").toDouble();
                            // 横向为主：向远离 wire 的一侧延伸
                            if (std::abs(wx) > std::abs(wy))
                                dir = wx > 0 ? "west" : "east";
                            // 纵向为主：向远离 wire 的一侧延伸
                            else
                                dir = wy > 0 ? "north" : "south";
                        }
                    }
                    // 以该候选起点构造搜索节点
                    SearchNode* n = new SearchNode(nullptr, loc, dir, dest);
                    // 加入本连接节点列表
                    connNodes.append(n);
                    // 加入全部节点列表
                    allNodes.append(n);
                }
            }
        }
    }
}

// 定义"A* 最短路径搜索"辅助函数
static SearchNode* findShortestPath(QList<SearchNode*>& nodes,
                                    const QList<QVariantMap>& pathLocs,
                                    AvoidanceMap* avoid)
{
    // 迭代上限，防止病态地图无限搜索
    const int MAX_ITERS = 20000;
    // 开放列表（按启发值排序的简单优先队列）
    QList<SearchNode*> q;
    // 初始节点全部入队
    for (auto* n : nodes) q.append(n);
    // 按启发值升序排序
    std::sort(q.begin(), q.end(),
              [](SearchNode* a, SearchNode* b) {
                  return a->heuristicValue() < b->heuristicValue();
              });
    // 已访问状态集合（按哈希去重）
    QSet<int> visited;
    // 迭代计数
    int iters = 0;
    // 开放列表非空且未超上限时循环
    while (!q.isEmpty() && iters < MAX_ITERS) {
        // 迭代计数
        iters++;
        // 每 64 次检查是否已有更高优先级请求，若有则中止
        if (iters % 64 == 0 && ConnectorThread::isOverrideRequested()) return nullptr;
        // 取启发值最小节点
        SearchNode* node = q.takeFirst();
        // 空指针保护
        if (!node) return nullptr;
        // 到达目标则返回
        if (node->isDestination()) return node;
        // 状态哈希
        int h = node->hash();
        // 已访问过则跳过
        if (visited.contains(h)) continue;
        // 标记已访问
        visited.insert(h);

        // 当前朝向
        QString dir = node->direction();
        // 默认邻居数
        int neighbors = 3;
        // 当前位置
        QVariantMap loc = node->location();
        // 当前位置占用标记
        QString allowed = avoid->get(loc.value("x").toDouble(), loc.value("y").toDouble());

        // 是否起点
        bool isStart = node->isStart();
        // 是否位于本连接路径候选点
        bool inPathLocs = false;
        // 起点才需判断是否在路径候选点集合内
        if (isStart) {
            for (const auto& pl : pathLocs) {
                if (pl == loc) { inPathLocs = true; break; }
            }
        }
        // 起点且位于候选路径点上：放宽占用限制，允许从原线上出发
        if (!allowed.isEmpty() && isStart && inPathLocs) allowed.clear();

        // 按占用标记决定可扩展邻居数与方向
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

        // 逐个扩展邻居方向
        for (int i = 0; i < neighbors; ++i) {
            // 本次移动方向
            QString oDir;
            // 第一个方向：保持直行
            if (i == 0) oDir = dir;
            // 第二个方向：两端可走则反向，否则左转
            else if (i == 1) oDir = (neighbors == 2) ? dirReverse(dir) : dirLeft(dir);
            // 第三个方向：右转
            else if (i == 2) oDir = dirRight(dir);
            // 第四个方向：反向
            else oDir = dirReverse(dir);

            // 扩展新节点：是否穿越受限格作为惩罚
            SearchNode* nextNode = node->next(oDir, !allowed.isEmpty());
            // 新节点有效且未访问则入队
            if (nextNode && !visited.contains(nextNode->hash())) {
                q.append(nextNode);
                // 重新按启发值排序
                std::sort(q.begin(), q.end(),
                          [](SearchNode* a, SearchNode* b) {
                              return a->heuristicValue() < b->heuristicValue();
                          });
            }
        }
    }
    // 未找到路径
    return nullptr;
}

// 定义"把搜索节点链转成拐点路径"辅助函数
static QList<QVariantMap> convertToPath(SearchNode* last) {
    // 准备路径点列表（逆序收集，最后反转）
    QList<QVariantMap> ret;
    // 当前节点
    SearchNode* next = last;
    // 前驱节点
    SearchNode* prev = last->previous();
    // 先放入终点
    ret.append(next->location());
    // 沿父链回溯
    while (prev != nullptr) {
        // 方向发生变化说明是拐点，记录
        if (prev->direction() != next->direction()) {
            ret.append(prev->location());
        }
        // 向前推进
        next = prev;
        prev = prev->previous();
    }
    // 补上起点
    if (ret.last() != next->location()) {
        ret.append(next->location());
    }
    // 逆序成从起点到终点
    std::reverse(ret.begin(), ret.end());
    // 返回拐点路径
    return ret;
}

// 定义"把 A* 路径转成连线增删改结果"辅助函数
static void processPath(const QList<QVariantMap>& path, const ConnectionData& conn,
                        AvoidanceMap* avoid, MoveResult* result)
{
    // 路径索引
    int idx = 0;
    // 当前段起点
    QVariantMap loc0 = path[idx++];
    // 连接原位置
    QVariantMap connLoc = conn.location();

    // 路径未从连接原点出发：需要处理原 wire 链的删除/缩短
    if (loc0 != connLoc) {
        // 沿 wire 链走到尽头的坐标
        QVariantMap pathLoc = conn.wirePathStart();
        // 是否已定位到接入点
        bool found = (loc0 == pathLoc);

        // 逐根遍历连接已有的 wire 链
        for (const auto& wire : conn.wirePath()) {
            // 取该 wire 的 path
            QVariantList wpath = wire.value("path").toList();
            // wire 末端坐标
            QVariantMap wireEnd = wpath.isEmpty() ? QVariantMap() : wpath.last().toMap();
            // 下一段接入点，默认取末端
            QVariantMap nextLoc = wireEnd;
            // 有顶点则根据当前 pathLoc 决定向哪一端走
            if (wpath.size() > 0) {
                // 首顶点
                QVariantMap f = wpath.first().toMap();
                // 末顶点
                QVariantMap l = wpath.last().toMap();
                if (pathLoc == f) nextLoc = l;
                else nextLoc = f;
            }

            // 已定位到接入点：本 wire 及之后全部删除
            if (found) {
                result->removals.append(wire);
                avoid->unmarkWire(wire, nextLoc);
            } else {
                // 判断接入点是否落在本 wire 路径上
                bool contains = false;
                for (const auto& pv : wpath) {
                    if (pv.toMap() == loc0) { contains = true; break; }
                }
                // 落在本 wire 上：本 wire 需缩短，只保留远离接入点的一段
                if (contains) {
                    found = true;
                    // 接入点不在末端才需缩短
                    if (loc0 != nextLoc) {
                        avoid->unmarkWire(wire, nextLoc);
                        // 构造缩短后的新线
                        QVariantMap shortened;
                        shortened["wireType"] = "ortho";
                        QVariantList newPath;
                        newPath.append(pathLoc);
                        newPath.append(loc0);
                        shortened["path"] = newPath;
                        // 记录替换对
                        QVariantMap oldW = wire;
                        result->replacements.append({oldW, shortened});
                        // 标记缩短后的新线占用
                        avoid->markWire(shortened, 0, 0);
                    }
                }
            }
            // 推进到下一段接入点
            pathLoc = nextLoc;
        }
    }

    // 剩余路径点逐段生成新线
    while (idx < path.size()) {
        // 下一段终点
        QVariantMap loc1 = path[idx++];
        // 准备新连线
        QVariantMap newWire;
        newWire["wireType"] = "ortho";
        QVariantList newPath;
        newPath.append(loc0);
        newPath.append(loc1);
        newWire["path"] = newPath;
        // 记录新增
        result->additions.append(newWire);
        // 标记新线占用
        avoid->markWire(newWire, 0, 0);
        // 推进段起点
        loc0 = loc1;
    }
}

// 定义"按位移方向排序连接"辅助函数
static void sortConnects(QList<ConnectionData>& connects, int dx, int dy) {
    // 按连接位置在位移方向上的投影排序
    std::sort(connects.begin(), connects.end(),
              [dx, dy](const ConnectionData& a, const ConnectionData& b) {
                  // 连接 a 的位置
                  QVariantMap l1 = a.location();
                  // 连接 b 的位置
                  QVariantMap l2 = b.location();
                  // 位置差 x 分量
                  double ax = l1.value("x").toDouble() - l2.value("x").toDouble();
                  // 位置差 y 分量
                  double ay = l1.value("y").toDouble() - l2.value("y").toDouble();
                  // 点积为负者排前（沿位移方向靠后者优先）
                  return (ax * dx + ay * dy) < 0;
              });
}

// 定义"按给定连接顺序尝试一次完整布线"辅助函数
static MoveResult* tryList(const MoveRequest& req, MoveGesture* gesture,
                           QList<ConnectionData>& connects, int dx, int dy,
                           QHash<int, QList<QVariantMap>>& pathLocs,
                           QHash<int, QList<SearchNode*>>& initNodes,
                           qint64 stopTime)
{
    // 克隆固定避让地图，避免污染缓存
    AvoidanceMap* avoid = gesture->getFixedAvoidanceMap()->cloneMap();
    // 叠加选中元件新位置的占用
    avoid->markAll(gesture->getSelected(), dx, dy);

    // 准备本次尝试的结果
    MoveResult* result = new MoveResult();

    // 按顺序逐条连接布线
    for (const auto& conn : connects) {
        // 已有更高优先级请求：放弃本次尝试
        if (ConnectorThread::isOverrideRequested()) { delete avoid; delete result; return nullptr; }
        // 超时：该连接标记为未满足，继续下一条
        if (QDateTime::currentMSecsSinceEpoch() > stopTime) {
            result->unsatisfied.append(conn);
            continue;
        }
        // 连接哈希
        int h = conn.hash();
        // 该连接的起点节点列表
        QList<SearchNode*>& connNodes = initNodes[h];
        // 该连接的候选路径点
        QList<QVariantMap>& connPathLocs = pathLocs[h];
        // 执行 A* 搜索
        SearchNode* node = findShortestPath(connNodes, connPathLocs, avoid);
        // 找到路径：累计距离并生成增删改
        if (node) {
            result->totalDistance += node->distance();
            QList<QVariantMap> path = convertToPath(node);
            processPath(path, conn, avoid, result);
            // 搜索中被中断：放弃本次尝试
        } else if (ConnectorThread::isOverrideRequested()) {
            delete avoid; delete result; return nullptr;
            // 搜索失败：标记该连接未满足
        } else {
            result->unsatisfied.append(conn);
        }
    }
    // 释放临时地图
    delete avoid;
    // 返回本次尝试结果
    return result;
}

// 定义"计算连线重连方案"入口
MoveResult* Connector::computeWires(const MoveRequest& req) {
    // 取手势对象
    MoveGesture* gesture = req.gesture();
    // 无手势直接返回空结果
    if (!gesture) return new MoveResult();

    // 请求位移
    int dx = req.dx(), dy = req.dy();
    // 基础连接列表
    QList<ConnectionData> baseConnects = gesture->getConnections();
    // 固定避让地图
    AvoidanceMap* fixedAvoid = gesture->getFixedAvoidanceMap();

    // 剔除不可能的连接
    QList<ConnectionData> connects;
    // 逐条检查
    for (const auto& conn : baseConnects) {
        // 计算移动后的目标位置
        QVariantMap dest;
        dest["x"] = conn.location().value("x").toDouble() + dx;
        dest["y"] = conn.location().value("y").toDouble() + dy;
        // 查询目标位置占用
        QString avoidVal = fixedAvoid->get(dest.value("x").toDouble(), dest.value("y").toDouble());
        // 目标被占用：除非它就落在原 wire 路径上，否则该连接无法达成，剔除
        if (!avoidVal.isEmpty()) {
            // 是否落在原 wire 路径上
            bool inPath = false;
            // 遍历连接已有 wire 链
            for (const auto& wire : conn.wirePath()) {
                // 取该 wire 的 path
                QVariantList path = wire.value("path").toList();
                // 检查目标是否等于某个顶点
                for (const auto& pv : path) {
                    QVariantMap p = pv.toMap();
                    if (p == dest) { inPath = true; break; }
                }
                // 找到即可停止
                if (inPath) break;
            }
            // 不在路径上则剔除该连接
            if (!inPath) continue;
        }
        // 保留该连接
        connects.append(conn);
    }

    // 创建选中元件新位置的避让地图
    AvoidanceMap* selAvoid = AvoidanceMap::create(gesture->getSelected(), dx, dy);
    // 连接哈希 → 候选路径点
    QHash<int, QList<QVariantMap>> pathLocs;
    // 连接哈希 → 起点节点列表
    QHash<int, QList<SearchNode*>> initNodes;
    // 所有起点节点（统一释放）
    QList<SearchNode*> allNodes;

    // 为每条连接生成起点
    for (const auto& conn : connects) {
        // 本连接的候选路径点
        QList<QVariantMap> connLocs;
        // 本连接的起点节点
        QList<SearchNode*> connNodes;
        // 生成起点
        processConnection(conn, dx, dy, connLocs, connNodes, selAvoid, allNodes);
        // 记录候选路径点
        pathLocs[conn.hash()] = connLocs;
        // 记录起点节点
        initNodes[conn.hash()] = connNodes;
    }
    // 释放临时地图
    delete selAvoid;

    // 根据连接数量决定尝试轮数
    int tries;
    if (connects.size() == 0) tries = 0;
    else if (connects.size() == 1) tries = 1;
    else if (connects.size() == 2) tries = 2;
    else if (connects.size() == 3) tries = 8;
    else tries = 10;

    // 整体计算截止时间：10 秒
    qint64 stopTime = QDateTime::currentMSecsSinceEpoch() + 10000;

    // 当前最优结果
    MoveResult* bestResult = nullptr;
    // 多轮尝试
    for (int tryNum = 0; tryNum < tries; ++tryNum) {
        // 已有更高优先级请求：释放资源并放弃
        if (ConnectorThread::isOverrideRequested()) {
            if (bestResult) delete bestResult;
            for (auto* n : allNodes) delete n;
            return nullptr;
        }
        // 超时则停止尝试
        if (QDateTime::currentMSecsSinceEpoch() > stopTime) break;

        // 复制连接列表，本轮可重排
        QList<ConnectionData> copy = connects;
        // 前两轮：按位移方向排序（正序/反序各一次）
        if (tryNum < 2) {
            sortConnects(copy, dx, dy);
            if (tryNum == 1) std::reverse(copy.begin(), copy.end());
            // 其余轮：随机打乱，探索不同顺序
        } else {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(copy.begin(), copy.end(), g);
        }

        // 按本轮顺序执行一次完整布线
        MoveResult* candidate = tryList(req, gesture, copy, dx, dy, pathLocs, initNodes, stopTime);
        // 尝试被中断：释放资源并放弃
        if (!candidate) {
            if (bestResult) delete bestResult;
            for (auto* n : allNodes) delete n;
            return nullptr;
        }
        // 首个结果直接作为最优
        if (!bestResult) bestResult = candidate;
        else {
            // 未满足更少者更优
            if (candidate->unsatisfied.size() < bestResult->unsatisfied.size())
            { delete bestResult; bestResult = candidate; }
            // 未满足数量相同则比较总距离，更短者更优
            else if (candidate->unsatisfied.size() == bestResult->unsatisfied.size() &&
                     candidate->totalDistance < bestResult->totalDistance)
            { delete bestResult; bestResult = candidate; }
            // 否则丢弃本轮结果
            else delete candidate;
        }
    }

    // 一轮都没成功：返回空结果
    if (!bestResult) bestResult = new MoveResult();

    // 释放所有起点节点
    for (auto* n : allNodes) delete n;
    // 返回最优方案
    return bestResult;
}