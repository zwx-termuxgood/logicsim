#include "WireNetworkSolver.h"
#include "ComponentLogic.h"
#include "core/Grid.h"
// 共享网格入口只在读取几何时取整；进入拓扑以后全部使用整数精确比较。
#include <QHash>
#include <QSet>
#include <QVector>
#include <cmath>
#include <limits>
#include <algorithm>

namespace {

// 并查集保存电气网络，不拆改用户的整条折线及其 ID。
struct DSU {
    QVector<int> parent;
    int add() {
        const int id = parent.size();
        parent.append(id);
        return id;
    }
    int find(int id) {
        int root = id;
        while (parent[root] != root) root = parent[root];
        while (parent[id] != id) {
            const int next = parent[id];
            parent[id] = root;
            id = next;
        }
        return root;
    }
    void unite(int a, int b) {
        a = find(a); b = find(b);
        if (a != b) parent[a] = b;
    }
};

struct Point { int x; int y; };
struct Location {
    Point point;
    int node;
    // 上下左右各占一位；同方向的重叠段只贡献一条实际支路。
    unsigned rays = 0;
    QSet<int> ports;
};
struct Segment { Point a; Point b; int node; };
struct Port { QString component; int index; bool output; int node; };

// 拒绝无穷大和越界旧数据，避免取整转换及世界坐标相加溢出。
bool readPoint(double x, double y, Point& point) {
    if (!Grid::valid(x) || !Grid::valid(y)) return false;
    point = {Grid::snap(x), Grid::snap(y)};
    return true;
}
bool readPoint(const QVariantMap& map, Point& point) {
    return readPoint(map.value("x").toDouble(), map.value("y").toDouble(), point);
}

// 电气连通与圆点共用此拓扑：只把真实顶点/端口加入候选位置，
// 不主动生成线段交点，因此两段内部的纯十字穿越始终不连接。
struct Topology {
    DSU dsu;
    QVector<Location> locations;
    QVector<Segment> segments;
    QVector<Port> ports;
    QHash<quint64, int> locationIds;
    QHash<QString, int> portIds;

    int location(Point point) {
        // 两个有符号 32 位坐标按位拼接，不分配字符串且不会碰撞。
        const quint64 key = (quint64(quint32(point.x)) << 32) | quint32(point.y);
        auto it = locationIds.constFind(key);
        if (it != locationIds.constEnd()) return it.value();
        const int id = locations.size();
        locations.append({point, dsu.add(), 0, {}});
        locationIds.insert(key, id);
        return id;
    }
    int port(const QString& component, int index, bool output) {
        // 长度前缀使元件 ID 自带冒号时也不会与别的端口发生键冲突。
        const QString key = QString::number(component.size()) + ':' + component
            + ':' + QString::number(index) + ':' + QString::number(output);
        auto it = portIds.constFind(key);
        if (it != portIds.constEnd()) return it.value();
        const int id = ports.size();
        ports.append({component, index, output, dsu.add()});
        portIds.insert(key, id);
        return id;
    }

    Topology(const QVariantList& components, const QVariantList& wires) {
        // 先读端口几何。偏移已由 ComponentLogic 量化，元件位置仅在这里读取一次。
        for (const auto& value : components) {
            const QVariantMap comp = value.toMap();
            if (comp.value("type").toString() == "text") continue;
            Point origin;
            if (!readPoint(comp, origin)) continue;
            const QString cid = comp.value("id").toString();
            for (bool output : {false, true}) {
                const QVariantList geometry = output ? ComponentLogic::outputPorts(comp)
                                                     : ComponentLogic::inputPorts(comp);
                for (int index = 0; index < geometry.size(); ++index) {
                    const QVariantMap offset = geometry[index].toMap();
                    Point point;
                    if (!readPoint(double(origin.x) + offset.value("x").toInt(),
                                   double(origin.y) + offset.value("y").toInt(), point)) continue;
                    const int pid = port(cid, index, output);
                    const int lid = location(point);
                    locations[lid].ports.insert(pid);
                    dsu.unite(ports[pid].node, locations[lid].node);
                }
            }
        }

        for (const auto& value : wires) {
            const QVariantMap wire = value.toMap();
            const QString type = wire.value("wireType").toString();
            if (type == "virtual") continue;
            if (type != "ortho") {
                // 贝塞尔及历史非正交线接口仍以显式两端绑定建立电气关系。
                const QString from = wire.value("fromComp").toString();
                const QString to = wire.value("toComp").toString();
                if (from.isEmpty() || to.isEmpty()) continue;
                const int a = port(from, wire.value("fromPort").toInt(), true);
                const int b = port(to, wire.value("toPort").toInt(), false);
                dsu.unite(ports[a].node, ports[b].node);
                continue;
            }
            const QVariantList path = wire.value("path").toList();
            if (path.size() < 2) continue;
            QVector<Point> points;
            bool valid = true;
            for (const auto& vertex : path) {
                Point point;
                if (!readPoint(vertex.toMap(), point)) { valid = false; break; }
                points.append(point);
            }
            if (!valid) continue;
            const int first = locations[location(points.first())].node;
            for (int i = 0; i < points.size(); ++i) {
                dsu.unite(first, locations[location(points[i])].node);
                if (i == 0) continue;
                const Point a = points[i - 1], b = points[i];
                // 零长段不贡献支路；旧斜段保留整线身份，但不参与中途接入。
                if ((a.x == b.x && a.y == b.y) || (a.x != b.x && a.y != b.y)) continue;
                segments.append({a, b, first});
            }
            // 保留原有显式绑定语义，绑定失效的清理由模型编辑入口负责。
            if (wire.contains("fromComp")) {
                const int pid = port(wire.value("fromComp").toString(),
                                     wire.value("fromPort").toInt(), true);
                dsu.unite(first, ports[pid].node);
            }
            if (wire.contains("toComp")) {
                const int pid = port(wire.value("toComp").toString(),
                                     wire.value("toPort").toInt(), false);
                dsu.unite(first, ports[pid].node);
            }
        }

        // 每个真实顶点（含折点）/端口接入经过它的所有正交段。
        // 共线重叠必有至少一个端点在另一段上，因而也由同一规则自然合并。
        QHash<int, QVector<int>> horizontalRows, verticalColumns;
        for (int i = 0; i < segments.size(); ++i) {
            const auto& segment = segments[i];
            if (segment.a.y == segment.b.y) horizontalRows[segment.a.y].append(i);
            else verticalColumns[segment.a.x].append(i);
        }
        const QVector<int> empty;
        for (auto& loc : locations) {
            const Point p = loc.point;
            const auto row = horizontalRows.constFind(p.y);
            const auto column = verticalColumns.constFind(p.x);
            const auto& horizontal = row == horizontalRows.constEnd() ? empty : row.value();
            const auto& vertical = column == verticalColumns.constEnd() ? empty : column.value();
            qsizetype h = 0, v = 0;
            while (h < horizontal.size() || v < vertical.size()) {
                // 归并原始索引，保持并查集合并及虚拟线生成的原有顺序。
                const bool useHorizontal = v == vertical.size()
                    || (h < horizontal.size() && horizontal[h] < vertical[v]);
                const auto& segment = segments[useHorizontal ? horizontal[h++] : vertical[v++]];
                const Point a = segment.a, b = segment.b;
                if (!Grid::onSegment(p.x, p.y, a.x, a.y, b.x, b.y)) continue;
                dsu.unite(loc.node, segment.node);
                if (a.x < p.x || b.x < p.x) loc.rays |= 1;
                if (a.x > p.x || b.x > p.x) loc.rays |= 2;
                if (a.y < p.y || b.y < p.y) loc.rays |= 4;
                if (a.y > p.y || b.y > p.y) loc.rays |= 8;
            }
        }
    }
};
} // namespace

namespace WireNetworkSolver {

static QVariantList virtualWiresFrom(Topology& topology) {
    QHash<int, QVector<int>> outputs, inputs;
    for (int i = 0; i < topology.ports.size(); ++i) {
        const Port& port = topology.ports[i];
        const int root = topology.dsu.find(port.node);
        (port.output ? outputs[root] : inputs[root]).append(i);
    }
    QVariantList result;
    for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
        for (int out : it.value()) {
            for (int in : inputs.value(it.key())) {
                const Port& from = topology.ports[out];
                const Port& to = topology.ports[in];
                result.append(QVariantMap{
                    {"id", QString("VW_%1_%2_%3_%4").arg(from.component).arg(from.index)
                        .arg(to.component).arg(to.index)},
                    {"wireType", "virtual"}, {"fromComp", from.component},
                    {"fromPort", from.index}, {"toComp", to.component}, {"toPort", to.index}});
            }
        }
    }
    return result;
}

static QVariantList dotsFrom(const Topology& topology) {
    QVariantList result;
    for (const auto& loc : topology.locations) {
        // 支路按方向去重，端口按身份去重；直通/普通折角为两路，T 为三路，
        // 明确四路节点为四路。重复路径及重复顶点不会凭空增加圆点。
        int count = loc.ports.size();
        for (unsigned rays = loc.rays; rays; rays >>= 1) count += rays & 1;
        if (count >= 3) result.append(Grid::point(loc.point.x, loc.point.y));
    }
    return result;
}

Analysis analyze(const QVariantList& components, const QVariantList& wires) {
    Topology topology(components, wires);
    return {virtualWiresFrom(topology), dotsFrom(topology)};
}

QVariantList generateVirtualWires(const QVariantList& components, const QVariantList& wires) {
    Topology topology(components, wires);
    return virtualWiresFrom(topology);
}

QVariantList connectionDots(const QVariantList& components, const QVariantList& wires) {
    return dotsFrom(Topology(components, wires));
}

} // namespace WireNetworkSolver
