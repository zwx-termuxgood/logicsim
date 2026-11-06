#include "WireNetworkSolver.h"
// 引入本文件要实现的 WireNetworkSolver 命名空间声明：实现其函数前必须看到函数原型
#include "ComponentLogic.h"
// 引入元件逻辑库：查询端口几何（outputPorts/inputPorts）的唯一来源
#include <QHash>
// 引入 QHash：并查集父表、位置/端口节点表、网络分类表都用它
#include <QSet>
// 引入 QSet：连接圆点统计中用于同一 wire 内部顶点去重
#include <QStringList>
// 引入 QStringList：拆分位置键/端口键时使用
#include <QVariantMap>
// 引入 QVariantMap：元件/连线/点都以 map 表示
#include <QList>
// 引入 QList：网络输出/输入列表使用
#include <QVector>
// 引入 QVector：orthoPaths、wireFirst 等连续容器使用
#include <algorithm>
// 引入算法库：std::min/std::max 用于线段区间计算
#include <cmath>
// 引入 C 数学库：std::round 用于位置键取整，std::abs 用于坐标比较

namespace {
// 匿名命名空间：内部辅助类型与函数只在本编译单元可见，避免符号污染

// 定义并查集：把电气上连通的位置/端口合并成同一网络
struct DSU {
    // 父表：节点 id → 父节点 id；采用惰性初始化，仅访问过的节点才有条目
    QHash<int, int> p;
    // 查找根节点（带路径压缩，两趟循环避免递归）
    int find(int x) {
        // 首次访问：把自己设为根
        if (!p.contains(x)) { p[x] = x; return x; }
        // 第一趟：找到根
        int r = x;
        while (p[r] != r) r = p[r];
        // 第二趟：路径压缩，把沿途节点直接挂到根上
        while (p[x] != r) { int n = p[x]; p[x] = r; x = n; }
        // 返回根
        return r;
    }
    // 合并两个节点所在集合
    void unite(int a, int b) {
        // 先找各自根
        a = find(a); b = find(b);
        // 根不同才合并
        if (a != b) p[a] = b;
    }
};

// 定义"把坐标取整拼成字符串键"辅助函数，供位置节点表与圆点统计使用
QString posKey(double x, double y) {
    // 四舍五入取整后拼成 "x,y"，容忍浮点误差
    return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
}

// 点 (px,py) 是否严格落在正交线段内部（不含端点）
bool pointOnSegmentInterior(double px, double py,
                            double x1, double y1, double x2, double y2) {
    // 线段近似水平：y 需贴合线段所在行
    if (std::abs(y1 - y2) < 0.5) {
        // 点 y 与线段 y 差超过容差则不在线上
        if (std::abs(py - y1) > 0.5) return false;
        // 线段 x 区间下界
        double lo = std::min(x1, x2);
        // 线段 x 区间上界
        double hi = std::max(x1, x2);
        // 点 x 必须严格落在区间内部（排除两端点）
        return px > lo + 0.5 && px < hi - 0.5;
    }
    // 线段近似竖直：x 需贴合线段所在列
    if (std::abs(x1 - x2) < 0.5) {
        // 点 x 与线段 x 差超过容差则不在线上
        if (std::abs(px - x1) > 0.5) return false;
        // 线段 y 区间下界
        double lo = std::min(y1, y2);
        // 线段 y 区间上界
        double hi = std::max(y1, y2);
        // 点 y 必须严格落在区间内部（排除两端点）
        return py > lo + 0.5 && py < hi - 0.5;
    }
    // 非正交线段（斜线）不做内部判断
    return false;
}

// 点是否落在 wire 的任意段的"内部"（不含端点）
bool pointOnWireInterior(double px, double py, const QVariantList& path) {
    // 相邻顶点两两组成线段逐个判断
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 线段起点
        QVariantMap p0 = path[i].toMap();
        // 线段终点
        QVariantMap p1 = path[i + 1].toMap();
        // 命中任意线段内部即返回 true
        if (pointOnSegmentInterior(px, py,
                                   p0.value("x").toDouble(), p0.value("y").toDouble(),
                                   p1.value("x").toDouble(), p1.value("y").toDouble())) {
            return true;
        }
    }
    // 所有线段都未命中返回 false
    return false;
}

} // namespace
// 匿名命名空间结束：以上辅助类型与函数仅本文件可用

namespace WireNetworkSolver {
// 打开 WireNetworkSolver 命名空间，与头文件声明保持一致

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
// 定义"生成虚拟线"接口：把电气连通的输出端口与输入端口两两连成虚拟线
QVariantList generateVirtualWires(const QVariantList& components,
                                  const QVariantList& wires)
{
    // 准备结果列表
    QVariantList result;
    // 元件与连线都为空则无需计算
    if (components.isEmpty() && wires.isEmpty()) return result;

    // 并查集：合并电气连通的位置/端口
    DSU dsu;
    // 节点 id 分配器
    int nextId = 0;

    // 端口节点表：key 为 "cid:port:isOut"，value 为节点 id
    QHash<QString, int> portNode;   // "cid:port:isOut" → nodeId
    // 位置节点表：key 为 "x,y"，value 为节点 id
    QHash<QString, int> posNode;    // "x,y" → nodeId
    // 每根 ortho 线首顶点对应的节点 id（-1 表示非 ortho 或无效）
    QVector<int> wireFirst(wires.size(), -1);

    // 内部 lambda：取（或创建）某坐标对应的位置节点
    auto getPosNode = [&](double x, double y) -> int {
        // 位置键
        QString k = posKey(x, y);
        // 未创建则新建节点并惰性初始化并查集
        if (!posNode.contains(k)) {
            int nid = nextId++;
            posNode[k] = nid;
            dsu.find(nid);
        }
        // 返回节点 id
        return posNode[k];
    };
    // 内部 lambda：取（或创建）某元件端口对应的节点
    auto getPortNode = [&](const QString& cid, int port, bool isOut) -> int {
        // 端口键：元件 id + 端口号 + 是否输出
        QString k = QString("%1:%2:%3").arg(cid).arg(port).arg(isOut ? 1 : 0);
        // 未创建则新建节点并惰性初始化并查集
        if (!portNode.contains(k)) {
            int nid = nextId++;
            portNode[k] = nid;
            dsu.find(nid);
        }
        // 返回节点 id
        return portNode[k];
    };

    // ============ 1. 元件端口 → 位置节点 ============
    // 遍历所有元件
    for (const auto& cv : components) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 元件 id
        QString cid = c.value("id").toString();
        // 元件类型
        QString t = c.value("type").toString();
        // 文本元件无端口，跳过
        if (t == "text") continue;
        // 元件世界坐标
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();

        // 查询输出端口几何
        QVariantList outs = ComponentLogic::outputPorts(c);
        // 逐个输出端口：端口节点与位置节点合并
        for (int i = 0; i < outs.size(); ++i) {
            // 端口转 map
            QVariantMap p = outs[i].toMap();
            // 端口节点
            int pn = getPortNode(cid, i, true);
            // 端口世界坐标对应的位置节点
            int loc = getPosNode(cx + p.value("x").toDouble(),
                                 cy + p.value("y").toDouble());
            // 合并端口与其位置
            dsu.unite(pn, loc);
        }
        // 查询输入端口几何
        QVariantList ins = ComponentLogic::inputPorts(c);
        // 逐个输入端口：端口节点与位置节点合并
        for (int i = 0; i < ins.size(); ++i) {
            // 端口转 map
            QVariantMap p = ins[i].toMap();
            // 端口节点
            int pn = getPortNode(cid, i, false);
            // 端口世界坐标对应的位置节点
            int loc = getPosNode(cx + p.value("x").toDouble(),
                                 cy + p.value("y").toDouble());
            // 合并端口与其位置
            dsu.unite(pn, loc);
        }
    }

    // ============ 2. 每根 wire 内部 unite ============
    //   ortho：path 所有顶点 unite 到 first（端点 + 拐点）
    //          同时把 fromComp/toComp 的端口 unite 进来
    //   bezier：fromComp/output 和 toComp/input unite
    for (int wi = 0; wi < wires.size(); ++wi) {
        // 连线转 map
        QVariantMap w = wires[wi].toMap();
        // 线型
        QString wt = w.value("wireType").toString();
        // 虚拟线不参与（避免自引用）
        if (wt == "virtual") continue;

        // ortho 线：沿 path 合并所有顶点
        if (wt == "ortho") {
            // 取顶点列表
            QVariantList path = w.value("path").toList();
            // 少于两个顶点无效
            if (path.size() < 2) continue;

            // 首顶点位置节点
            int first = getPosNode(path[0].toMap().value("x").toDouble(),
                                   path[0].toMap().value("y").toDouble());
            // 记录该线首节点，供 T 型接入判断使用
            wireFirst[wi] = first;

            // 其余顶点都合并到 first
            for (int i = 1; i < path.size(); ++i) {
                int n = getPosNode(path[i].toMap().value("x").toDouble(),
                                   path[i].toMap().value("y").toDouble());
                dsu.unite(first, n);
            }

            // 起点绑定了元件端口则合并
            if (w.contains("fromComp")) {
                QString fc = w.value("fromComp").toString();
                int fp = w.value("fromPort").toInt();
                int pn = getPortNode(fc, fp, true);
                dsu.unite(pn, first);
            }
            // 终点绑定了元件端口则合并
            if (w.contains("toComp")) {
                QString tc = w.value("toComp").toString();
                int tp = w.value("toPort").toInt();
                int pn = getPortNode(tc, tp, false);
                dsu.unite(pn, first);
            }
        } else {
            // bezier：两端端口直接合并
            // 起点元件与端口
            QString fc = w.value("fromComp").toString();
            int fp = w.value("fromPort").toInt();
            // 终点元件与端口
            QString tc = w.value("toComp").toString();
            int tp = w.value("toPort").toInt();
            // 任一端缺失则跳过
            if (fc.isEmpty() || tc.isEmpty()) continue;
            // 两端端口节点
            int pn1 = getPortNode(fc, fp, true);
            int pn2 = getPortNode(tc, tp, false);
            // 合并
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
        // 拆分位置键
        QStringList parts = it.key().split(',');
        // 格式不对则跳过
        if (parts.size() != 2) continue;
        // 解析坐标
        double px = parts[0].toDouble();
        double py = parts[1].toDouble();
        // 该位置节点 id
        int pn = it.value();

        // 检查是否落在任意 ortho 线中间段
        for (int wi = 0; wi < wires.size(); ++wi) {
            // 非有效 ortho 线跳过
            if (wireFirst[wi] < 0) continue;
            // 连线转 map
            QVariantMap w = wires[wi].toMap();
            // 取 path
            QVariantList path = w.value("path").toList();
            // 落在中间段则把该位置与线合并
            if (pointOnWireInterior(px, py, path)) {
                dsu.unite(pn, wireFirst[wi]);
            }
        }
    }

    // ============ 4. 分类 output / input ============
    // 网络根 → 该网络内的输出端口列表（"cid:port" 形式）
    QHash<int, QList<QString>> netOutputs;
    // 网络根 → 该网络内的输入端口列表
    QHash<int, QList<QString>> netInputs;

    // 遍历所有端口节点，按网络根分类
    for (auto it = portNode.constBegin(); it != portNode.constEnd(); ++it) {
        // 端口键
        QString key = it.key();
        // 拆分 key
        QStringList parts = key.split(':');
        // 格式不对则跳过
        if (parts.size() != 3) continue;
        // 元件 id
        QString cid = parts[0];
        // 端口索引
        int port = parts[1].toInt();
        // 是否输出端口
        bool isOut = parts[2].toInt() == 1;
        // 该端口所在网络根
        int root = dsu.find(it.value());
        // 按方向加入对应列表
        if (isOut) netOutputs[root].append(cid + ":" + QString::number(port));
        else       netInputs[root].append(cid + ":" + QString::number(port));
    }

    // ============ 5. 生成虚拟线：每个网络 output × input ============
    for (auto it = netOutputs.constBegin(); it != netOutputs.constEnd(); ++it) {
        // 网络根
        int root = it.key();
        // 该网络没有输入端口则无需连线
        if (!netInputs.contains(root)) continue;
        // 该网络内每个输出端口
        for (const auto& out : it.value()) {
            // 与每个输入端口配对
            for (const auto& in : netInputs.value(root)) {
                // 拆分输出端口
                QStringList op = out.split(':');
                // 拆分输入端口
                QStringList ip = in.split(':');
                // 准备虚拟线
                QVariantMap vw;
                // 生成唯一虚拟线 id
                vw["id"] = QString("VW_%1_%2_%3_%4")
                               .arg(op[0]).arg(op[1]).arg(ip[0]).arg(ip[1]);
                // 标记为虚拟线
                vw["wireType"] = "virtual";
                // 记录起点
                vw["fromComp"] = op[0];
                vw["fromPort"] = op[1].toInt();
                // 记录终点
                vw["toComp"] = ip[0];
                vw["toPort"] = ip[1].toInt();
                // 追加到结果
                result.append(vw);
            }
        }
    }

    // 返回所有虚拟线
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
// 定义"计算连接圆点位置"接口
QVariantList connectionDots(const QVariantList& components,
                            const QVariantList& wires)
{
    // 位置键 → 连接计数
    QHash<QString, int> posCount;
    // 位置键 → 坐标 map（输出用）
    QHash<QString, QVariantMap> posMap;

    // ============ 元件端口 ============
    // 遍历所有元件
    for (const auto& cv : components) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 元件类型
        QString t = c.value("type").toString();
        // 文本元件无端口，跳过
        if (t == "text") continue;
        // 元件世界坐标
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();

        // 输出端口各计 1
        QVariantList outs = ComponentLogic::outputPorts(c);
        for (const auto& pv : outs) {
            // 端口转 map
            QVariantMap p = pv.toMap();
            // 端口世界坐标
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            // 位置键
            QString k = posKey(px, py);
            // 计数 +1
            posCount[k]++;
            // 记录坐标
            QVariantMap m; m["x"] = px; m["y"] = py;
            posMap[k] = m;
        }
        // 输入端口各计 1
        QVariantList ins = ComponentLogic::inputPorts(c);
        for (const auto& pv : ins) {
            // 端口转 map
            QVariantMap p = pv.toMap();
            // 端口世界坐标
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            // 位置键
            QString k = posKey(px, py);
            // 计数 +1
            posCount[k]++;
            // 记录坐标
            QVariantMap m; m["x"] = px; m["y"] = py;
            posMap[k] = m;
        }
    }

    // 收集所有 ortho 线 path，供顶点计数与 T 型判断
    QVector<QVariantList> orthoPaths;
    // 遍历所有连线
    for (const auto& wv : wires) {
        // 连线转 map
        QVariantMap w = wv.toMap();
        // 只收集 ortho 线
        if (w.value("wireType").toString() != "ortho") continue;
        // 收集其 path
        orthoPaths.append(w.value("path").toList());
    }

    // ============ wire 顶点 ============
    for (int wi = 0; wi < orthoPaths.size(); ++wi) {
        // 当前线的 path
        const QVariantList& path = orthoPaths[wi];
        // 同一根线内部顶点去重集合
        QSet<QString> seen;
        // 逐个顶点计数
        for (const auto& pv : path) {
            // 顶点转 map
            QVariantMap p = pv.toMap();
            // 顶点坐标
            double px = p.value("x").toDouble();
            double py = p.value("y").toDouble();
            // 位置键
            QString k = posKey(px, py);
            // 同一根线内同一位置只计一次
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
            // 顶点转 map
            QVariantMap p = pv.toMap();
            // 顶点坐标
            double px = p.value("x").toDouble();
            double py = p.value("y").toDouble();
            // 是否为 T 型接入点
            bool isT = false;
            // 检查是否落在其它线中间段
            for (int wj = 0; wj < orthoPaths.size(); ++wj) {
                // 跳过自身
                if (wi == wj) continue;
                // 落在其它线中间段即为 T 型
                if (pointOnWireInterior(px, py, orthoPaths[wj])) {
                    isT = true;
                    break;
                }
            }
            // T 型接入点额外 +2
            if (isT) {
                QString k = posKey(px, py);
                posCount[k] += 2;
            }
        }
    }

    // ============ 输出 ============
    // 准备结果列表
    QVariantList out;
    // 遍历所有位置
    for (auto it = posCount.constBegin(); it != posCount.constEnd(); ++it) {
        // 连接数 >= 3 才画圆点
        if (it.value() >= 3) {
            out.append(posMap.value(it.key()));
        }
    }
    // 返回圆点位置列表
    return out;
}

} // namespace WireNetworkSolver
// 关闭 WireNetworkSolver 命名空间