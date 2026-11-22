#ifndef LOGICSIM_GRID_H
#define LOGICSIM_GRID_H

// 引入 QVariant 容器：元件/连线/顶点都以 QVariant 表示
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
// 引入 QtGlobal：qMin/qMax 等工具
#include <QtGlobal>
// 引入 C 数学库：llround 用于坐标取整
#include <cmath>
// 引入 QVector：规范化时内部临时顶点序列
#include <QVector>

// ============================================================
// 整数网格工具（全项目坐标判定基础）
//
// 约定：所有持久化坐标（元件 x/y、ortho 线 path 顶点）一律为整数；
// 自由端点交互吸附 10px 网格，贴线/贴端口时使用精确整数行列。
// 所有"重合 / 在线上 / T 型"判定一律用整数精确比较，
// 不再使用 ±0.5 等浮点容差，杜绝拖动浮点漂移导致的判定失效。
// ============================================================
namespace Grid {

// 网格步长：自由端点吸附间距（画线/拖动/拖拐点用），与 Logisim 一致
constexpr int STEP = 10;

// 定义"任意坐标取整"接口：1px 粒度，作为持久化与判定的统一入口
// 坐标限制为 ±10 亿，两个极限点的二维距离平方仍可由 qint64 表示。
constexpr int MAX_COORD = 1000000000;
inline bool valid(double v) {
    return std::isfinite(v) && v >= -MAX_COORD && v <= MAX_COORD;
}
inline int snap(double v) {
    // 非法输入由模型/IO 边界拒绝；内部转换仍防止未定义的浮点转整数行为。
    if (!std::isfinite(v)) return 0;
    return int(std::llround(qBound(-double(MAX_COORD), v, double(MAX_COORD))));
}

// 定义"吸附到 10px 网格"接口：自由端点（画线落点、拖拐点）使用
inline int snapStep(double v) {
    // 先除以步长取整再乘回，得到最近的 10 的倍数
    if (!valid(v)) return snap(v);
    return int(std::llround(v / STEP)) * STEP;
}

// 定义"构造整数点 map"接口：path 顶点的唯一构造入口
inline QVariantMap point(int x, int y) {
    // 准备点 map
    QVariantMap p;
    // 写入整数坐标
    p["x"] = x; p["y"] = y;
    // 返回点 map
    return p;
}

// 定义"从顶点 map 读整数 x"接口：容忍旧数据里的浮点值
inline int xi(const QVariantMap& p) {
    // 取 x 字段并取整
    return snap(p.value("x").toDouble());
}

// 定义"从顶点 map 读整数 y"接口：容忍旧数据里的浮点值
inline int yi(const QVariantMap& p) {
    // 取 y 字段并取整
    return snap(p.value("y").toDouble());
}

// 定义"点是否在线段上（含端点）"接口：整数精确判定，仅正交段有效
inline bool onSegment(int px, int py, int x1, int y1, int x2, int y2) {
    // 竖直段：x 必须落在段所在列，y 在两端之间（含端点）
    if (x1 == x2) return px == x1 && py >= qMin(y1, y2) && py <= qMax(y1, y2);
    // 水平段：y 必须落在段所在行，x 在两端之间（含端点）
    if (y1 == y2) return py == y1 && px >= qMin(x1, x2) && px <= qMax(x1, x2);
    // 斜段不参与整数判定（规范化后不应存在）
    return false;
}

// 定义"点是否严格在线段内部（不含端点）"接口：T 型接入判定用
inline bool onSegmentInterior(int px, int py, int x1, int y1, int x2, int y2) {
    // 竖直段：同列且严格介于两端之间
    if (x1 == x2) return px == x1 && py > qMin(y1, y2) && py < qMax(y1, y2);
    // 水平段：同行且严格介于两端之间
    if (y1 == y2) return py == y1 && px > qMin(x1, x2) && px < qMax(x1, x2);
    // 斜段不参与整数判定
    return false;
}

// 定义"点是否落在折线任一段上（含端点）"接口：缩短/命中判定用
inline bool onPath(int px, int py, const QVariantList& path) {
    // 相邻顶点两两检查
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 取段两端
        QVariantMap a = path[i].toMap();
        QVariantMap b = path[i + 1].toMap();
        // 任一段命中即在线上
        if (onSegment(px, py, xi(a), yi(a), xi(b), yi(b))) return true;
    }
    // 所有段都未命中
    return false;
}

// 定义"点是否严格落在折线某段内部（不含顶点）"接口：T 型接入判定用
inline bool onPathInterior(int px, int py, const QVariantList& path) {
    // 相邻顶点两两检查
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 取段两端
        QVariantMap a = path[i].toMap();
        QVariantMap b = path[i + 1].toMap();
        // 任一段内部命中即为 T 型点
        if (onSegmentInterior(px, py, xi(a), yi(a), xi(b), yi(b))) return true;
    }
    // 所有段都未命中
    return false;
}

// 定义"把点投影到折线上最近位置"接口：返回 {x,y,distSq}，无命中时不含 x
// 投影结果一定落在某段的精确行列上（整数），供缩短原线时吸附用
inline QVariantMap projectOnPath(int px, int py, const QVariantList& path) {
    // 准备最佳结果
    QVariantMap best;
    // 最佳距离平方，初值为极大
    qint64 bestD = Q_INT64_C(0x7fffffffffffffff);
    // 遍历每一段
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 取段两端整数坐标
        QVariantMap a = path[i].toMap();
        QVariantMap b = path[i + 1].toMap();
        int x1 = xi(a), y1 = yi(a), x2 = xi(b), y2 = yi(b);
        // 候选投影点与距离平方
        int cx, cy;
        qint64 d;
        // 水平段：投影到该行的 [min,max] 区间内
        if (y1 == y2) {
            // x 钳到段区间，y 保持段所在行
            cx = qBound(qMin(x1, x2), px, qMax(x1, x2));
            cy = y1;
            // 距离平方（全部整数运算）
            d = qint64(px - cx) * (px - cx) + qint64(py - cy) * (py - cy);
            // 竖直段：投影到该列的 [min,max] 区间内
        } else if (x1 == x2) {
            // y 钳到段区间，x 保持段所在列
            cx = x1;
            cy = qBound(qMin(y1, y2), py, qMax(y1, y2));
            // 距离平方
            d = qint64(px - cx) * (px - cx) + qint64(py - cy) * (py - cy);
            // 斜段（旧数据）：跳过，不产生非法投影
        } else {
            // 继续下一段
            continue;
        }
        // 更近则记录
        if (d < bestD) {
            // 更新最佳距离
            bestD = d;
            // 清空旧结果
            best.clear();
            // 写入投影点（段上的精确整数坐标）
            best["x"] = cx;
            best["y"] = cy;
            // 写入距离平方，供调用方做容差判断
            best["distSq"] = d;
        }
    }
    // 返回最佳投影（空 path 时为空 map）
    return best;
}

// 定义"正交规范化折线"接口：网格化所有顶点、去零长段、合并共线段、
// 斜段按前段走向补拐点拉直。全项目写入 path 前的最后一道保证。
inline QVariantList normalizePath(const QVariantList& path) {
    // 内部顶点序列（整数）
    QVector<QPair<int, int>> pts;
    // 逐顶点取整收集
    for (const auto& pv : path) {
        // 顶点转 map 并取整
        QVariantMap p = pv.toMap();
        // 追加到序列
        pts.append(qMakePair(xi(p), yi(p)));
    }
    // 准备输出序列
    QVector<QPair<int, int>> out;
    // 逐段处理，斜段补拐点
    for (int i = 0; i < pts.size(); ++i) {
        // 当前顶点
        QPair<int, int> cur = pts[i];
        // 上一段是否水平（用于斜段补拐点时选择走向）
        bool prevHoriz = false;
        // 是否存在上一段
        bool hasPrev = false;
        // 输出非空时按输出末段判断走向
        if (out.size() >= 2) {
            // 末段两端
            QPair<int, int> a = out[out.size() - 2];
            QPair<int, int> b = out[out.size() - 1];
            // 末段水平则记为水平
            prevHoriz = (a.second == b.second);
            // 存在上一段
            hasPrev = true;
        }
        // 首顶点直接放入
        if (i == 0) {
            // 放入首顶点
            out.append(cur);
            // 继续下一个
            continue;
        }
        // 取前一个输出顶点
        QPair<int, int> last = out.last();
        // 与前顶点重合：跳过（去零长段）
        if (cur.first == last.first && cur.second == last.second) continue;
        // 与前顶点正交（同行或同列）：直接放入
        if (cur.first == last.first || cur.second == last.second) {
            // 放入正交顶点
            out.append(cur);
            // 继续下一个
            continue;
        }
        // 斜段：按上一段走向补拐点保持正交
        QPair<int, int> elbow;
        // 有上一段且为水平 → 拐点取 (cur.x, last.y)，先横后竖
        if (hasPrev && prevHoriz) elbow = qMakePair(cur.first, last.second);
        // 其它（竖直上一段或无上一段）→ 拐点取 (last.x, cur.y)
        // 无上一段时与 addOrthoWire 默认"先横后竖"一致
        else elbow = qMakePair(last.first, cur.second);
        // 拐点与前一顶点重合则跳过拐点
        if (elbow.first != last.first || elbow.second != last.second) {
            // 放入拐点
            out.append(elbow);
        }
        // 拐点与当前顶点重合则跳过当前顶点
        if (elbow.first != cur.first || elbow.second != cur.second) {
            // 放入当前顶点
            out.append(cur);
        }
    }
    // 共线顶点也可能是明确的电气接入节点，单条路径缺少外部拓扑信息，
    // 因此这里保留它们及回折极值，不能因几何简化而把连接十字变成纯穿越。
    const auto& merged = out;
    // 准备输出列表
    QVariantList result;
    // 逐点转 map
    for (const auto& p : merged) {
        // 构造整数点 map
        result.append(point(p.first, p.second));
    }
    // 返回规范化后的 path
    return result;
}

// 定义"沿原路径裁剪一端"接口：不重建 L 形，不猜测操作端。
inline QVariantList clippedPath(const QVariantList& path, bool front, int x, int y) {
    int hit = -1;
    // 从被裁剪的一端搜索，自交路径也保持确定的沿线语义。
    for (int n = 0; n + 1 < path.size(); ++n) {
        int i = front ? n : int(path.size()) - 2 - n;
        auto a = path[i].toMap(), b = path[i + 1].toMap();
        if (onSegment(x, y, xi(a), yi(a), xi(b), yi(b))) { hit = i; break; }
    }
    if (hit < 0) return {};
    QVariantList result;
    auto append = [&](const QVariantMap& p) {
        auto q = point(xi(p), yi(p));
        if (result.isEmpty() || result.last().toMap() != q) result.append(q);
    };
    if (front) {
        append(point(x, y));
        for (int i = hit + 1; i < path.size(); ++i) append(path[i].toMap());
    } else {
        for (int i = 0; i <= hit; ++i) append(path[i].toMap());
        append(point(x, y));
    }
    return result;
}

} // namespace Grid

#endif // LOGICSIM_GRID_H
