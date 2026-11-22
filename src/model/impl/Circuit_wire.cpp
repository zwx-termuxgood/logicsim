#include "model/Circuit.h"
// 引入本文件要实现的 Circuit 类完整声明：定义成员函数前必须看到类定义
#include "core/Grid.h"
// 引入整数网格几何：模型中的连接判断只使用离散位置
#include "core/IdAllocator.h"
// 引入 ID 分配器：addOrthoWire/addWire 给新连线分配唯一 id 时使用
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：查询端口几何（outputPorts/inputPorts）的唯一来源
#include "logic/WireNetworkSolver.h"
// 引入连线网络求解器：connectionDots 计算连接圆点位置时委托给它
#include <QVariantMap>
// 引入 QVariantMap：元件/连线/端口都以 map 表示
#include <QPointF>
// 引入 QPointF：辅助函数与部分临时点计算使用浮点二维点
#include <QHash>
// 引入 QHash：内部哈希容器（本文件按需使用）
#include <QSet>
// 引入 QSet：去重集合（本文件按需使用）
#include <cmath>
// 引入 C 数学库：std::hypot/std::round/std::abs 等用于距离与网格化计算

// 匿名命名空间：内部辅助函数只在本编译单元可见，避免符号污染
namespace {
// 定义"构造一个 {x,y} 点 map"的辅助函数，供 path 顶点复用
QVariantMap makePoint(double x, double y) {
    return Grid::point(Grid::snap(x), Grid::snap(y));
}
// 定义"点到线段最短距离平方"的辅助函数，用于 ortho 线命中测试
double pointSegDistSq(double px, double py, double x1, double y1, double x2, double y2) {
    // 线段方向向量分量
    double dx = x2 - x1, dy = y2 - y1;
    // 线段长度平方
    double len2 = dx * dx + dy * dy;
    // 投影参数 t，默认 0（退化为起点）
    double t = 0;
    // 线段非退化时才计算投影
    if (len2 > 1e-9) {
        // 点在直线上的投影参数
        t = ((px - x1) * dx + (py - y1) * dy) / len2;
        // 投影参数钳到 [0,1]，限制在线段内
        if (t < 0) t = 0; else if (t > 1) t = 1;
    }
    // 线段上离点最近的点坐标
    double cx = x1 + t * dx, cy = y1 + t * dy;
    // 点到最近点的偏移
    double ddx = px - cx, ddy = py - cy;
    // 返回距离平方（避免开方，比较用）
    return ddx * ddx + ddy * ddy;
}
// 匿名命名空间结束：以上辅助函数仅本文件可用
}

// 定义"端口命中测试"接口：在容差内找最近端口，返回 compId/port/isOutput/坐标
QVariantMap Circuit::findPortAt(double x, double y, double tol) const {
    // 取当前上下文引用
    const Context& ctx = currentCtx();
    // 准备最佳命中结果
    QVariantMap best;
    // 最佳距离平方，初值为容差平方（超出即不命中）
    double bestDist = tol * tol;
    // 遍历所有元件
    for (const auto& cv : ctx.components) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 取元件 id
        QString cid = c.value("id").toString();
        // 取类型
        QString t = c.value("type").toString();
        // 文本元件没有端口，跳过
        if (t == "text") continue;
        // 取元件世界坐标
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();
        // 查询输出端口几何
        QVariantList outs = ComponentLogic::outputPorts(c);
        // 遍历输出端口
        for (int i = 0; i < outs.size(); ++i) {
            // 端口转 map
            QVariantMap p = outs[i].toMap();
            // 端口世界 x = 元件 x + 端口偏移 x；y 同理
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            // 点与端口偏移
            double dx = x - px, dy = y - py;
            // 距离平方
            double d = dx * dx + dy * dy;
            // 更近则更新最佳结果
            if (d < bestDist) {
                // 更新最佳距离
                bestDist = d;
                // 清空旧结果
                best.clear();
                // 记录元件 id
                best["compId"] = cid;
                // 记录端口索引
                best["port"] = i;
                // 标记为输出端口
                best["isOutput"] = true;
                // 记录端口世界坐标
                best["x"] = px;
                best["y"] = py;
            }
        }
        // 查询输入端口几何
        QVariantList ins = ComponentLogic::inputPorts(c);
        // 遍历输入端口
        for (int i = 0; i < ins.size(); ++i) {
            // 端口转 map
            QVariantMap p = ins[i].toMap();
            // 端口世界坐标
            double px = cx + p.value("x").toDouble();
            double py = cy + p.value("y").toDouble();
            // 点与端口偏移
            double dx = x - px, dy = y - py;
            // 距离平方
            double d = dx * dx + dy * dy;
            // 更近则更新最佳结果
            if (d < bestDist) {
                // 更新最佳距离
                bestDist = d;
                // 清空旧结果
                best.clear();
                // 记录元件 id
                best["compId"] = cid;
                // 记录端口索引
                best["port"] = i;
                // 标记为输入端口
                best["isOutput"] = false;
                // 记录端口世界坐标
                best["x"] = px;
                best["y"] = py;
            }
        }
    }
    // 返回最佳命中（未命中为空 map）
    return best;
}

// 定义"枚举所有 ortho 线段"接口：供 QML 绘制或碰撞检测
QVariantList Circuit::allWireSegments() const {
    // 准备输出列表
    QVariantList out;
    // 取当前上下文引用
    const Context& ctx = currentCtx();
    // 遍历所有连线
    for (const auto& wv : ctx.wires) {
        // 连线转 map
        QVariantMap w = wv.toMap();
        // 只有 ortho 线有显式线段；其它线型跳过
        if (w.value("wireType").toString() != "ortho") continue;
        // 取连线 id
        QString id = w.value("id").toString();
        // 取顶点列表
        QVariantList path = w.value("path").toList();
        // 相邻顶点两两组成线段
        for (int i = 0; i + 1 < path.size(); ++i) {
            // 线段起点
            QVariantMap p0 = path[i].toMap();
            // 线段终点
            QVariantMap p1 = path[i + 1].toMap();
            // 准备线段 map
            QVariantMap seg;
            // 记录所属连线 id
            seg["wireId"] = id;
            // 记录两端坐标
            seg["x1"] = p0.value("x");
            seg["y1"] = p0.value("y");
            seg["x2"] = p1.value("x");
            seg["y2"] = p1.value("y");
            // 追加到输出列表
            out.append(seg);
        }
    }
    // 返回所有线段
    return out;
}

// 定义"计算连接圆点"接口：委托 WireNetworkSolver，连接数 >= 3 才返回
QVariantList Circuit::connectionDots() const {
    // 委托求解器根据元件与连线计算连接圆点位置
    return WireNetworkSolver::connectionDots(currentCtx().components, currentCtx().wires);
}

// ============================================================
// addOrthoWire
//   起点/终点坐标全部由 JS 侧决定（吸附端口 / 从线引出 / 网格化）
//   本函数只负责：
//     - 端口绑定（根据 isOutput 分配 fromComp/toComp）
//     - 反向时交换端点 + 镜像 direction
//     - 生成 L 型 path
// ============================================================
// 定义"添加正交线"接口：吸附端口后按方向生成 L 型 path，反向时自动交换端点
QString Circuit::addOrthoWire(double fromX, double fromY,
                              double toX, double toY,
                              int direction)
{
    // 非法世界坐标不能进入持久化拓扑。
    if (m_viewOnly || !Grid::valid(fromX) || !Grid::valid(fromY) ||
        !Grid::valid(toX) || !Grid::valid(toY)) return QString();

    // 端口命中使用精确整数位置；鼠标容差只应由前端命中层处理。
    fromX = Grid::snap(fromX); fromY = Grid::snap(fromY);
    toX = Grid::snap(toX); toY = Grid::snap(toY);
    QVariantMap fPort = findPortAt(fromX, fromY, 1);
    QVariantMap tPort = findPortAt(toX, toY, 1);
    // 自由端吸附 10px；已在线上的整数目标保留原行列。
    auto snapFree = [&](double& x, double& y, const QVariantMap& port) {
        if (!port.isEmpty()) return;
        for (const auto& value : currentCtx().wires) {
            auto wire = value.toMap();
            if (wire.value("wireType").toString() == "ortho" &&
                Grid::onPath(int(x), int(y), wire.value("path").toList())) return;
        }
        x = Grid::snapStep(x); y = Grid::snapStep(y);
    };
    snapFree(fromX, fromY, fPort);
    snapFree(toX, toY, tPort);

    // 起点元件 id（未命中端口时为空）
    QString fromComp, toComp;
    // 起点/终点端口索引（-1 表示未绑定）
    int fromPort = -1, toPort = -1;

    // 起点：命中端口用端口精确坐标，否则保留 JS 传的精确点（从线引出的 T 型点）
    if (!fPort.isEmpty()) {
        fromX = fPort.value("x").toDouble();
        fromY = fPort.value("y").toDouble();
    }
    // 不网格化（JS 侧已处理）

    // 终点：命中端口用端口精确坐标，否则保留 JS 传的精确点（已吸附或网格化）
    if (!tPort.isEmpty()) {
        toX = tPort.value("x").toDouble();
        toY = tPort.value("y").toDouble();
    }
    // 不网格化（JS 侧已处理）

    // 端口绑定：两端都命中时按输入/输出方向决定谁是起点
    if (!fPort.isEmpty() && !tPort.isEmpty()) {
        // 起点是否输出端口
        bool fIsOut = fPort.value("isOutput").toBool();
        // 终点是否输出端口
        bool tIsOut = tPort.value("isOutput").toBool();
        // 起点输出、终点输入：方向正确，直接绑定
        if (fIsOut && !tIsOut) {
            fromComp = fPort.value("compId").toString();
            fromPort = fPort.value("port").toInt();
            toComp   = tPort.value("compId").toString();
            toPort   = tPort.value("port").toInt();
            // 起点输入、终点输出：方向反了，交换端点并镜像方向
        } else if (!fIsOut && tIsOut) {
            fromComp = tPort.value("compId").toString();
            fromPort = tPort.value("port").toInt();
            toComp   = fPort.value("compId").toString();
            toPort   = fPort.value("port").toInt();
            // 交换起点/终点坐标
            std::swap(fromX, toX);
            std::swap(fromY, toY);
            // 镜像 L 型方向：1<->2 互换
            if (direction == 1) direction = 2;
            else if (direction == 2) direction = 1;
        }
        // 只命中起点且是输出端口：只绑定起点
    } else if (!fPort.isEmpty() && fPort.value("isOutput").toBool()) {
        fromComp = fPort.value("compId").toString();
        fromPort = fPort.value("port").toInt();
        // 只命中终点且是输入端口：只绑定终点
    } else if (!tPort.isEmpty() && !tPort.value("isOutput").toBool()) {
        toComp = tPort.value("compId").toString();
        toPort = tPort.value("port").toInt();
    }

    // 起终点几乎重合视为无效线，返回空 id
    if (fromX == toX && fromY == toY) return QString();

    // 取当前上下文引用
    Context& ctx = currentCtx();
    // 压撤销快照：添加连线可撤销
    pushUndo();

    // 准备 path 顶点列表
    QVariantList path;
    // 先放入起点
    path.append(makePoint(fromX, fromY));

    // 两端是否同一行
    bool sameRow = fromY == toY;
    // 两端是否同一列
    bool sameCol = fromX == toX;
    // 不在同一行也不在同一列：需要插入拐点形成 L 型
    if (!sameRow && !sameCol) {
        // 方向仅决定首个拐点的离散走向；所有坐标均已经是整数。
        if (direction == 2) {
            path.append(Grid::point(int(fromX), int(toY)));
        } else {
            path.append(Grid::point(int(toX), int(fromY)));
        }
    }
    // 最后放入终点
    path.append(makePoint(toX, toY));

    // 准备连线 map
    QVariantMap w;
    // 分配唯一连线 id
    w["id"] = IdAllocator::nextWireId(ctx);
    // 线型标记为 ortho
    w["wireType"] = "ortho";
    // 写入 path
    w["path"] = path;
    // 起点绑定了端口则写入 fromComp/fromPort
    if (!fromComp.isEmpty()) {
        w["fromComp"] = fromComp;
        w["fromPort"] = fromPort;
    }
    // 终点绑定了端口则写入 toComp/toPort
    if (!toComp.isEmpty()) {
        w["toComp"] = toComp;
        w["toPort"] = toPort;
    }
    // 追加到当前上下文连线列表
    ctx.wires.append(w);

    // 全量求值让新连线参与逻辑/网络计算
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
    // 返回新连线 id
    return w["id"].toString();
}

// 定义"缩短线的一端到新坐标"接口：用于从已有线拉出新线后截断原线
bool Circuit::shortenWireEnd(const QString& wireId, bool shrinkFront, double newX, double newY) {
    // 目标必须已经投影到原线；这里只做整数转换，不再次吸附到其它行列。
    if (m_viewOnly || !Grid::valid(newX) || !Grid::valid(newY)) return false;
    int idx = indexOfWire(wireId);
    if (idx < 0) return false;
    Context& ctx = currentCtx();
    QVariantMap w = ctx.wires[idx].toMap();
    if (w.value("wireType").toString() != "ortho") return false;
    QVariantList path = w.value("path").toList();
    if (path.size() < 2) return false;
    const int x = Grid::snap(newX), y = Grid::snap(newY);
    QVariantList cropped = Grid::clippedPath(path, shrinkFront, x, y);
    // 空结果表示目标不在原路径上；相同路径表示没有发生裁剪。
    if (cropped.isEmpty() || cropped == path) return false;
    pushUndo("shorten:" + wireId);
    if (cropped.size() < 2) {
        ctx.wires.removeAt(idx);
    } else {
        w["path"] = cropped;
        auto bindingStillAtEndpoint = [&](const QString& side) {
            const QString compKey = side + "Comp", portKey = side + "Port";
            const QString compId = w.value(compKey).toString();
            if (compId.isEmpty() || !w.contains(portKey)) return false;
            const int ci = indexOfComponent(compId);
            if (ci < 0) return false;
            const auto component = ctx.components[ci].toMap();
            const auto ports = side == "from" ? ComponentLogic::outputPorts(component)
                                                 : ComponentLogic::inputPorts(component);
            const int portIndex = w.value(portKey).toInt();
            if (portIndex < 0 || portIndex >= ports.size()) return false;
            const auto port = ports[portIndex].toMap();
            const int px = Grid::snap(component.value("x").toDouble() + port.value("x").toDouble());
            const int py = Grid::snap(component.value("y").toDouble() + port.value("y").toDouble());
            const auto first = cropped.first().toMap(), last = cropped.last().toMap();
            return (Grid::xi(first) == px && Grid::yi(first) == py)
                || (Grid::xi(last) == px && Grid::yi(last) == py);
        };
        if (!bindingStillAtEndpoint("from")) { w.remove("fromComp"); w.remove("fromPort"); }
        if (!bindingStillAtEndpoint("to")) { w.remove("toComp"); w.remove("toPort"); }
        ctx.wires[idx] = w;
    }
    evaluateAll();
    emit geometryChanged();
    emit changed();
    emit structureChanged();
    return true;
}

// 定义"添加 bezier 线"接口：只记录两端元件与端口索引，几何实时计算
QString Circuit::addWire(const QString& fromComp, int fromPort,
                         const QString& toComp, int toPort) {
    // 只读模式禁止添加，返回空 id
    if (m_viewOnly) return QString();
    // 起点元件不存在则失败
    if (indexOfComponent(fromComp) < 0) return QString();
    // 终点元件不存在则失败
    if (indexOfComponent(toComp) < 0) return QString();
    // 取当前上下文引用
    Context& ctx = currentCtx();
    // 压撤销快照
    pushUndo();
    // 准备连线 map
    QVariantMap w;
    // 分配唯一连线 id
    w["id"] = IdAllocator::nextWireId(ctx);
    // 记录起点元件与端口
    w["fromComp"] = fromComp;
    w["fromPort"] = fromPort;
    // 记录终点元件与端口
    w["toComp"] = toComp;
    w["toPort"] = toPort;
    // 追加到连线列表（未写 wireType，按约定回退 bezier）
    ctx.wires.append(w);
    // 全量求值
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
    // 返回新连线 id
    return w["id"].toString();
}

// 定义"删除连线"接口
void Circuit::removeWire(const QString& id) {
    // 只读模式禁止删除
    if (m_viewOnly) return;
    // 查连线索引
    int idx = indexOfWire(id);
    // 连线不存在则返回
    if (idx < 0) return;
    // 压撤销快照
    pushUndo();
    // 从当前上下文连线列表移除
    currentCtx().wires.removeAt(idx);
    // 全量求值
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 定义"连线命中测试"接口：ortho 用线段距离，bezier 用采样点距离，返回最近线 id
QString Circuit::wireAtPoint(double x, double y, double tol) const {
    // 取当前上下文引用
    const Context& ctx = currentCtx();
    // 遍历所有连线
    for (auto& wv : ctx.wires) {
        // 连线转 map
        auto w = wv.toMap();
        // 取连线 id
        QString id = w.value("id").toString();
        // 取线型，缺省回退 bezier
        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";
        // 虚拟线不参与命中
        if (wt == "virtual") continue;
        // ortho 线：逐线段做点到线段距离判断
        if (wt == "ortho") {
            // 取顶点列表
            QVariantList path = w.value("path").toList();
            // 相邻顶点两两判断
            for (int i = 0; i + 1 < path.size(); ++i) {
                QVariantMap p0 = path[i].toMap();
                QVariantMap p1 = path[i + 1].toMap();
                // 距离平方小于容差平方即命中
                if (pointSegDistSq(x, y,
                                   p0.value("x").toDouble(), p0.value("y").toDouble(),
                                   p1.value("x").toDouble(), p1.value("y").toDouble())
                    < tol * tol) return id;
            }
            // ortho 线判断完毕，继续下一条
            continue;
        }
        // bezier 线：从两端端口实时算控制点后采样判断
        QString fromId = w.value("fromComp").toString();
        QString toId = w.value("toComp").toString();
        int fromIdx = w.value("fromPort").toInt();
        int toIdx = w.value("toPort").toInt();
        // 内部 lambda：求某元件某端口的世界坐标
        auto compPos = [&](const QString& cid, int port, bool isOut,
                           double& px, double& py) -> bool {
            // 查元件索引
            int ci = indexOfComponent(cid);
            // 元件不存在返回失败
            if (ci < 0) return false;
            // 取出元件 map
            QVariantMap c = ctx.components[ci].toMap();
            // 按输出/输入取端口几何
            QVariantList ports = isOut ? ComponentLogic::outputPorts(c)
                                       : ComponentLogic::inputPorts(c);
            // 端口索引越界返回失败
            if (port < 0 || port >= ports.size()) return false;
            // 取出端口相对偏移
            QVariantMap p = ports[port].toMap();
            // 世界坐标 = 元件坐标 + 端口偏移
            px = c.value("x").toDouble() + p.value("x").toDouble();
            py = c.value("y").toDouble() + p.value("y").toDouble();
            return true;
        };
        // 两端世界坐标
        double x1, y1, x2, y2;
        // 起点取不到则跳过该线
        if (!compPos(fromId, fromIdx, true, x1, y1)) continue;
        // 终点取不到则跳过该线
        if (!compPos(toId, toIdx, false, x2, y2)) continue;
        // 控制点水平偏移量，至少 40
        double cx = std::max(std::abs(x2 - x1) / 2.0, 40.0);
        // 采样段数
        const int samples = 20;
        // 沿曲线采样判断
        for (int i = 0; i <= samples; ++i) {
            // 参数 t 归一化到 [0,1]
            double t = (double)i / samples;
            // 1-t，供三次贝塞尔公式复用
            double mt = 1.0 - t;
            // 三次贝塞尔 x 坐标
            double bx = mt*mt*mt*x1 + 3*mt*mt*t*(x1+cx) + 3*mt*t*t*(x2-cx) + t*t*t*x2;
            // 三次贝塞尔 y 坐标（控制点 y 与端点相同，简化）
            double by = mt*mt*mt*y1 + 3*mt*mt*t*y1 + 3*mt*t*t*y2 + t*t*t*y2;
            // 采样点与查询点偏移
            double dx = bx - x, dy = by - y;
            // 距离平方小于容差平方即命中
            if (dx*dx + dy*dy < tol*tol) return id;
        }
    }
    // 都没命中返回空串
    return QString();
}

// 定义"拖动 ortho 线中间拐点"接口：网格化后同时修正相邻点以保持正交
bool Circuit::setWirePathPoint(const QString& id, int index, double x, double y) {
    // 只读模式禁止修改
    if (m_viewOnly) return false;
    // 查连线索引
    int idx = indexOfWire(id);
    // 连线不存在返回失败
    if (idx < 0) return false;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出连线 map
    QVariantMap w = ctx.wires[idx].toMap();
    // 只有 ortho 线支持编辑顶点
    if (w.value("wireType").toString() != "ortho") return false;
    // 取顶点列表
    QVariantList path = w.value("path").toList();
    // 索引越界返回失败
    if (index < 0 || index >= path.size()) return false;
    // 首末顶点是端口绑定点，不允许直接拖动
    if (index == 0 || index == path.size() - 1) return false;
    if (!Grid::valid(x) || !Grid::valid(y)) return false;
    const int nx = Grid::snapStep(x), ny = Grid::snapStep(y);
    const auto old = path[index].toMap();
    if (Grid::xi(old) == nx && Grid::yi(old) == ny) return false;
    QVariantList edited;
    auto append = [&](const QVariantMap& p) {
        auto q = Grid::point(Grid::xi(p), Grid::yi(p));
        if (edited.isEmpty() || edited.last().toMap() != q) edited.append(q);
    };
    for (int i = 0; i < index; ++i) append(path[i].toMap());
    const auto prev = path[index - 1].toMap(), next = path[index + 1].toMap();
    // 两侧分别沿编辑前的方向接续，绑定端点不变；不按新点距离猜方向。
    if (Grid::yi(prev) == Grid::yi(old)) append(Grid::point(nx, Grid::yi(prev)));
    else append(Grid::point(Grid::xi(prev), ny));
    append(Grid::point(nx, ny));
    if (Grid::yi(next) == Grid::yi(old)) append(Grid::point(Grid::xi(next), ny));
    else append(Grid::point(nx, Grid::yi(next)));
    for (int i = index + 1; i < path.size(); ++i) append(path[i].toMap());
    pushUndo("editPath:" + id);
    path = edited;
    // 写回 path
    w["path"] = path;
    // 写回上下文
    ctx.wires[idx] = w;
    // 全量求值
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
    // 返回成功
    return true;
}

// 定义"读取 ortho 线顶点列表"接口：bezier 线通常为空
QVariantList Circuit::getWirePath(const QString& id) const {
    // 查连线索引
    int idx = indexOfWire(id);
    // 不存在返回空列表
    if (idx < 0) return {};
    // 返回连线 map 中的 path 字段
    return currentCtx().wires[idx].toMap().value("path").toList();
}

// 定义"查询连线线型"接口
QString Circuit::getWireType(const QString& id) const {
    // 查连线索引
    int idx = indexOfWire(id);
    // 不存在返回空串
    if (idx < 0) return "";
    // 取 wireType 字段
    QString wt = currentCtx().wires[idx].toMap().value("wireType").toString();
    // 字段缺失时回退 "bezier"
    return wt.isEmpty() ? "bezier" : wt;
}