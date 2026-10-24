#include "WireNetworkSolver.h"
#include "ComponentLogic.h"
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace {

struct DSU {
    QHash<int, int> p;
    int find(int x) {
        if (!p.contains(x)) { p[x] = x; return x; }
        int r = x;
        while (p[r] != r) r = p[r];
        while (p[x] != r) { int n = p[x]; p[x] = r; x = n; }
        return r;
    }
    void unite(int a, int b) {
        a = find(a); b = find(b);
        if (a != b) p[a] = b;
    }
};

QString posKey(double x, double y) {
    return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
}

// 点 (px,py) 是否严格落在正交线段内部（不含端点）
bool pointOnSegmentInterior(double px, double py,
                            double x1, double y1, double x2, double y2) {
    if (std::abs(y1 - y2) < 0.5) {
        if (std::abs(py - y1) > 0.5) return false;
        double lo = std::min(x1, x2);
        double hi = std::max(x1, x2);
        return px > lo + 0.5 && px < hi - 0.5;
    }
    if (std::abs(x1 - x2) < 0.5) {
        if (std::abs(px - x1) > 0.5) return false;
        double lo = std::min(y1, y2);
        double hi = std::max(y1, y2);
        return py > lo + 0.5 && py < hi - 0.5;
    }
    return false;
}

// 点是否落在 wire 的任意段的"内部"（不含端点）
bool pointOnWireInterior(double px, double py, const QVariantList& path) {
    for (int i = 0; i + 1 < path.size(); ++i) {
        QVariantMap p0 = path[i].toMap();
        QVariantMap p1 = path[i + 1].toMap();
        if (pointOnSegmentInterior(px, py,
                                   p0.value("x").toDouble(), p0.value("y").toDouble(),
                                   p1.value("x").toDouble(), p1.value("y").toDouble())) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace WireNetworkSolver {

// ============================================================
// 生成虚拟线（用于电气互联）
//   严格按照 Logisim CircuitWires.connectWires 的规则：
//     1. 元件端口 → 位置节点
//     2. wire 的两个端点 → unite（端点对端点）
//     3. T 型接入（端点落在另一根 wire 的中间段）→ unite
//        （等价于 Logisim 分割 wire 后变成端点对端点）
//     4. 中间对中间（十字交叉）→ 不 unite
//   问题："不是只要两根线有一个点坐标一样就算交叉"
//     → 本实现只 unite "端点重合" 和 "端点落在段中间"，
//       中间对中间交叉不 unite。
// ============================================================
QVariantList generateVirtualWires(const QVariantList& components,
                                  const QVariantList& wires)
{
    QVariantList result;
    if (components.isEmpty() && wires.isEmpty()) return result;

    DSU dsu;
    int nextId = 0;

    QHash<QString, int> portNode;   // "cid:port:isOut" → nodeId
    QHash<QString, int> posNode;    // "x,y" → nodeId
    QVector<int> wireFirst(wires.size(), -1);

    auto getPosNode = [&](double x, double y) -> int {
        QString k = posKey(x, y);
        if (!posNode.contains(k)) {
            int nid = nextId++;
            posNode[k] = nid;
            dsu.find(nid);
        }
        return posNode[k];
    };
    auto getPortNode = [&](const QString& cid, int port, bool isOut) -> int {
        QString k = QString("%1:%2:%3").arg(cid).arg(port).arg(isOut ? 1 : 0);
        if (!portNode.contains(k)) {
            int nid = nextId++;
            portNode[k] = nid;
            dsu.find(nid);
        }
        return portNode[k];
    };

    // ============ 1. 元件端口 → 位置节点 ============
    for (const auto& cv : components) {
        QVariantMap c = cv.toMap();
        QString cid = c.value("id").toString();
        QString t = c.value("type").toString();
        if (t == "text") continue;
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();

        QVariantList outs = ComponentLogic::outputPorts(c);
        for (int i = 0; i < outs.size(); ++i) {
            QVariantMap p = outs[i].toMap();
            int pn = getPortNode(cid, i, true);
            int loc = getPosNode(cx + p.value("x").toDouble(),
                                 cy + p.value("y").toDouble());
            dsu.unite(pn, loc);
        }
        QVariantList ins = ComponentLogic::inputPorts(c);
        for (int i = 0; i < ins.size(); ++i) {
            QVariantMap p = ins[i].toMap();
            int pn = getPortNode(cid, i, false);
            int loc = getPosNode(cx + p.value("x").toDouble(),
                                 cy + p.value("y").toDouble());
            dsu.unite(pn, loc);
        }
    }

    // ============ 2. 每根 wire 内部 unite ============
    //   ortho：path 所有顶点 unite 到 first（端点 + 拐点）
    //          同时把 fromComp/toComp 的端口 unite 进来
    //   bezier：fromComp/output 和 toComp/input unite
    for (int wi = 0; wi < wires.size(); ++wi) {
        QVariantMap w = wires[wi].toMap();
        QString wt = w.value("wireType").toString();
        if (wt == "virtual") continue;

        if (wt == "ortho") {
            QVariantList path = w.value("path").toList();
            if (path.size() < 2) continue;

            int first = getPosNode(path[0].toMap().value("x").toDouble(),
                                   path[0].toMap().value("y").toDouble());
            wireFirst[wi] = first;

            for (int i = 1; i < path.size(); ++i) {
                int n = getPosNode(path[i].toMap().value("x").toDouble(),
                                   path[i].toMap().value("y").toDouble());
                dsu.unite(first, n);
            }

            if (w.contains("fromComp")) {
                QString fc = w.value("fromComp").toString();
                int fp = w.value("fromPort").toInt();
                int pn = getPortNode(fc, fp, true);
                dsu.unite(pn, first);
            }
            if (w.contains("toComp")) {
                QString tc = w.value("toComp").toString();
                int tp = w.value("toPort").toInt();
                int pn = getPortNode(tc, tp, false);
                dsu.unite(pn, first);
            }
        } else {
            // bezier
            QString fc = w.value("fromComp").toString();
            int fp = w.value("fromPort").toInt();
            QString tc = w.value("toComp").toString();
            int tp = w.value("toPort").toInt();
            if (fc.isEmpty() || tc.isEmpty()) continue;
            int pn1 = getPortNode(fc, fp, true);
            int pn2 = getPortNode(tc, tp, false);
            dsu.unite(pn1, pn2);
        }
    }

    // ============ 3. T 型连接：位置 P 落在 wire 的中间段 ============
    //   严格按 Logisim 分割语义：
    //     位置 P 是某根 wire 的中间点（不是任何 wire 的端点）
    //     则该 wire 被 P 分割成两段，P 成为新端点 → unite
    //   ★ 遍历所有 posNode 中的位置（wire 顶点 + 元件端口），
    //     检查它们是否落在其他 wire 的"中间段"上。
    //   ★ 十字交叉：交叉点不是任何 wire 顶点或元件端口，
    //     不在 posNode 里，不会被处理 → 不 unite。
    for (auto it = posNode.constBegin(); it != posNode.constEnd(); ++it) {
        QStringList parts = it.key().split(',');
        if (parts.size() != 2) continue;
        double px = parts[0].toDouble();
        double py = parts[1].toDouble();
        int pn = it.value();

        for (int wi = 0; wi < wires.size(); ++wi) {
            if (wireFirst[wi] < 0) continue;
            QVariantMap w = wires[wi].toMap();
            QVariantList path = w.value("path").toList();
            if (pointOnWireInterior(px, py, path)) {
                dsu.unite(pn, wireFirst[wi]);
            }
        }
    }

    // ============ 4. 分类 output / input ============
    QHash<int, QList<QString>> netOutputs;
    QHash<int, QList<QString>> netInputs;

    for (auto it = portNode.constBegin(); it != portNode.constEnd(); ++it) {
        QString key = it.key();
        QStringList parts = key.split(':');
        if (parts.size() != 3) continue;
        QString cid = parts[0];
        int port = parts[1].toInt();
        bool isOut = parts[2].toInt() == 1;
        int root = dsu.find(it.value());
        if (isOut) netOutputs[root].append(cid + ":" + QString::number(port));
        else       netInputs[root].append(cid + ":" + QString::number(port));
    }

    // ============ 5. 生成虚拟线：每个网络 output × input ============
    for (auto it = netOutputs.constBegin(); it != netOutputs.constEnd(); ++it) {
        int root = it.key();
        if (!netInputs.contains(root)) continue;
        for (const auto& out : it.value()) {
            for (const auto& in : netInputs.value(root)) {
                QStringList op = out.split(':');
                QStringList ip = in.split(':');
                QVariantMap vw;
                vw["id"] = QString("VW_%1_%2_%3_%4")
                               .arg(op[0]).arg(op[1]).arg(ip[0]).arg(ip[1]);
                vw["wireType"] = "virtual";
                vw["fromComp"] = op[0];
                vw["fromPort"] = op[1].toInt();
                vw["toComp"] = ip[0];
                vw["toPort"] = ip[1].toInt();
                result.append(vw);
            }
        }
    }

    return result;
}

// ============================================================
// 连接圆点（junction dots）
//   Logisim 规则：连接数 >= 3 画圆点
//   本实现模拟 Logisim 分割语义：
//     - wire 端点 / 元件端口：+1
//     - T 型接入点（wire 顶点落在另一根 wire 中间）：+2
//       （模拟：被接入的 wire 被分割成两根 → 该 wire 贡献 2 个端点）
//   这样：
//     三根 wire 端点共点（3 端点）→ 3 → 画
//     T 型（1 端点 + 1 wire 被分割=2）→ 3 → 画
//     两根 wire 端点相连 → 2 → 不画（视觉上就是一条线，不需要圆点）
//     十字交叉（中间对中间）→ 0 → 不画
// ============================================================
QVariantList connectionDots(const QVariantList& components,
                            const QVariantList& wires)
{
    QHash<QString, int> posCount;
    QHash<QString, QVariantMap> posMap;

    // ============ 元件端口 ============
    for (const auto& cv : components) {
        QVariantMap c = cv.toMap();
        QString t = c.value("type").toString();
        if (t == "text") continue;
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();

        QVariantList outs = ComponentLogic::outputPorts(c);
        for (const auto& pv : outs) {
            QVariantMap p = pv.toMap();
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            QString k = posKey(px, py);
            posCount[k]++;
            QVariantMap m; m["x"] = px; m["y"] = py;
            posMap[k] = m;
        }
        QVariantList ins = ComponentLogic::inputPorts(c);
        for (const auto& pv : ins) {
            QVariantMap p = pv.toMap();
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            QString k = posKey(px, py);
            posCount[k]++;
            QVariantMap m; m["x"] = px; m["y"] = py;
            posMap[k] = m;
        }
    }

    // 收集 ortho wire paths
    QVector<QVariantList> orthoPaths;
    for (const auto& wv : wires) {
        QVariantMap w = wv.toMap();
        if (w.value("wireType").toString() != "ortho") continue;
        orthoPaths.append(w.value("path").toList());
    }

    // ============ wire 顶点 ============
    for (int wi = 0; wi < orthoPaths.size(); ++wi) {
        const QVariantList& path = orthoPaths[wi];
        QSet<QString> seen;
        for (const auto& pv : path) {
            QVariantMap p = pv.toMap();
            double px = p.value("x").toDouble();
            double py = p.value("y").toDouble();
            QString k = posKey(px, py);
            if (!seen.contains(k)) {
                seen.insert(k);
                posCount[k]++;
                QVariantMap m; m["x"] = px; m["y"] = py;
                posMap[k] = m;
            }
        }

        // T 型接入：wire wi 的顶点落在另一根 wire 的中间段
        // +2（模拟 wire 被分割成两根）
        for (const auto& pv : path) {
            QVariantMap p = pv.toMap();
            double px = p.value("x").toDouble();
            double py = p.value("y").toDouble();
            bool isT = false;
            for (int wj = 0; wj < orthoPaths.size(); ++wj) {
                if (wi == wj) continue;
                if (pointOnWireInterior(px, py, orthoPaths[wj])) {
                    isT = true;
                    break;
                }
            }
            if (isT) {
                QString k = posKey(px, py);
                posCount[k] += 2;
            }
        }
    }

    // ============ 输出 ============
    QVariantList out;
    for (auto it = posCount.constBegin(); it != posCount.constEnd(); ++it) {
        if (it.value() >= 3) {
            out.append(posMap.value(it.key()));
        }
    }
    return out;
}

} // namespace WireNetworkSolver