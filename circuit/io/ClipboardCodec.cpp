#include "ClipboardCodec.h"
// 引入 ClipboardCodec 命名空间声明：实现 encode/paste 前必须看到函数原型与 PasteResult 结构
#include "../core/IdAllocator.h"
// 引入 ID 分配器：paste 为粘贴出的元件/连线分配新 id 时使用
#include <QSet>
// 引入 QSet：encode 中按 id 做 O(1) 命中判断、收集选中线 id 时使用
#include <QVariantList>
// 引入 QVariantList：元件/连线列表与最终返回结构都用它
#include <QVariantMap>
// 引入 QVariantMap：单个元件/连线/点都以 map 表示
#include <cmath>
// 引入 C 数学库：std::round/std::abs 用于位置键取整与线段内部判断
#include <algorithm>
// 引入算法库：std::min/std::max 用于线段区间计算

namespace ClipboardCodec {
// 打开 ClipboardCodec 命名空间，与头文件声明保持一致

namespace {
// 匿名命名空间：内部辅助函数只在本编译单元可见，避免符号污染

// 定义"把点坐标取整拼成字符串键"的辅助函数，供顶点重合判断使用
QString posKeyOf(const QVariantMap& p) {
    // 对 x、y 四舍五入取整后拼成 "x,y"，容忍浮点误差
    return QString("%1,%2").arg(int(std::round(p.value("x").toDouble())))
        .arg(int(std::round(p.value("y").toDouble())));
}

// 定义"点是否严格落在正交线段内部（不含端点）"的辅助函数
bool pointOnSegInterior(double px, double py,
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

// 定义"点是否落在 path 任意段内部"的辅助函数
bool pointOnPathInterior(double px, double py, const QVariantList& path) {
    // 相邻顶点两两组成线段逐个判断
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 线段起点
        QVariantMap p0 = path[i].toMap();
        // 线段终点
        QVariantMap p1 = path[i + 1].toMap();
        // 命中任意线段内部即返回 true
        if (pointOnSegInterior(px, py,
                               p0.value("x").toDouble(), p0.value("y").toDouble(),
                               p1.value("x").toDouble(), p1.value("y").toDouble())) {
            return true;
        }
    }
    // 所有线段都未命中返回 false
    return false;
}

} // namespace
// 匿名命名空间结束：以上辅助函数仅本文件可用

// 定义"编码选中内容"接口：抽取指定 id 的元件及其相关连线，打包成 QVariantList
QVariantList encode(const CircuitContext& ctx, const QStringList& ids) {
    // 选中 id 集合，便于 O(1) 命中判断
    QSet<QString> idSet;
    // 逐个插入集合
    for (const auto& id : ids) idSet.insert(id);

    // 标记选中的是否包含元件（用于区分"只选线"与"选了元件"两种语义）
    bool hasComponentsSelected = false;
    // 遍历元件判断是否命中选中集合
    for (auto& cv : ctx.components) {
        if (idSet.contains(cv.toMap().value("id").toString())) {
            hasComponentsSelected = true;
            break;
        }
    }

    // 准备输出的元件列表
    QVariantList compsOut;
    // 遍历元件，命中的复制进输出列表
    for (auto& cv : ctx.components) {
        // 元件转 map
        auto c = cv.toMap();
        if (idSet.contains(c.value("id").toString())) compsOut.append(c);
    }

    // 准备选中的连线 id 集合
    QSet<QString> selectedWireIds;

    // 未选中任何元件：只复制明确选中的线
    if (!hasComponentsSelected) {
        // 遍历连线，命中的加入选中集合
        for (auto& wv : ctx.wires) {
            auto w = wv.toMap();
            if (idSet.contains(w.value("id").toString()))
                selectedWireIds.insert(w.value("id").toString());
        }
        // 选中了元件：需要按连通性扩散收集相关连线
    } else {
        // 第一轮：收集中被直接选中的线、以及端点直接连到选中元件的线
        for (auto& wv : ctx.wires) {
            // 连线转 map
            auto w = wv.toMap();
            // 连线 id
            QString wid = w.value("id").toString();
            // 连线本身被选中：直接加入
            if (idSet.contains(wid)) { selectedWireIds.insert(wid); continue; }
            // 取两端元件 id
            QString fromComp = w.value("fromComp").toString();
            QString toComp = w.value("toComp").toString();
            // 任一端是选中元件则加入
            if ((!fromComp.isEmpty() && idSet.contains(fromComp)) ||
                (!toComp.isEmpty() && idSet.contains(toComp))) {
                selectedWireIds.insert(wid);
            }
        }

        // 第二轮：迭代扩散——端点对端点、正反 T 型接入的线逐步纳入
        bool changed = true;
        // 迭代保护计数，防止异常数据导致死循环
        int guard = 0;
        while (changed && guard++ < 200) {
            // 本轮是否新增了线
            changed = false;

            // 已选线的所有顶点位置键集合
            QSet<QString> selVertexKeys;
            // 已选线的 path 列表，供内部点判断使用
            QVector<QVariantList> selPaths;
            // 收集已选线的顶点键与 path
            for (auto& wv : ctx.wires) {
                // 连线转 map
                auto w = wv.toMap();
                // 未选中的线跳过
                if (!selectedWireIds.contains(w.value("id").toString())) continue;
                // 取 path
                QVariantList path = w.value("path").toList();
                // 无 path 的线（如 bezier）跳过顶点比对
                if (path.isEmpty()) continue;
                // 记录 path 供内部点判断
                selPaths.append(path);
                // 逐个顶点取位置键放入集合
                for (auto& pv : path)
                    selVertexKeys.insert(posKeyOf(pv.toMap()));
            }

            // 遍历未选中的线，判断是否与已选线连通
            for (auto& wv : ctx.wires) {
                // 连线转 map
                auto w = wv.toMap();
                // 连线 id
                QString wid = w.value("id").toString();
                // 已选中的跳过
                if (selectedWireIds.contains(wid)) continue;
                // 取 path
                QVariantList path = w.value("path").toList();
                // 无 path 的线不参与扩散
                if (path.isEmpty()) continue;

                // 是否匹配
                bool matched = false;

                // 端点对端点：首/末顶点与已选线顶点重合即连通
                if (selVertexKeys.contains(posKeyOf(path.first().toMap())) ||
                    selVertexKeys.contains(posKeyOf(path.last().toMap()))) {
                    matched = true;
                }

                // 本线端点落在已选线内部（正向 T 型接入）
                if (!matched) {
                    // 本线首顶点
                    QVariantMap p0 = path.first().toMap();
                    // 本线末顶点
                    QVariantMap pn = path.last().toMap();
                    // 对每条已选线判断端点是否落在其内部
                    for (const auto& selPath : selPaths) {
                        if (pointOnPathInterior(p0.value("x").toDouble(),
                                                p0.value("y").toDouble(), selPath) ||
                            pointOnPathInterior(pn.value("x").toDouble(),
                                                pn.value("y").toDouble(), selPath)) {
                            matched = true;
                            break;
                        }
                    }
                }

                // 已选线端点落在本线内部（反向 T 型接入）
                if (!matched) {
                    // 对每条已选线取其首末顶点
                    for (const auto& selPath : selPaths) {
                        // 已选线首顶点
                        QVariantMap a = selPath.first().toMap();
                        // 已选线末顶点
                        QVariantMap b = selPath.last().toMap();
                        // 判断是否落在本线内部
                        if (pointOnPathInterior(a.value("x").toDouble(),
                                                a.value("y").toDouble(), path) ||
                            pointOnPathInterior(b.value("x").toDouble(),
                                                b.value("y").toDouble(), path)) {
                            matched = true;
                            break;
                        }
                    }
                }

                // 匹配则纳入选中集合并标记本轮有变化
                if (matched) {
                    selectedWireIds.insert(wid);
                    changed = true;
                }
            }
        }
    }

    // 准备输出的连线列表
    QVariantList wiresOut;
    // 遍历连线，命中的复制进输出列表
    for (auto& wv : ctx.wires) {
        // 连线转 map
        auto w = wv.toMap();
        // 选中的才输出
        if (selectedWireIds.contains(w.value("id").toString()))
            wiresOut.append(w);
    }

    // 准备最终数据 map
    QVariantMap data;
    // 写入元件列表
    data["components"] = compsOut;
    // 写入连线列表
    data["wires"] = wiresOut;
    // 准备返回列表（只含一个元素，与通用剪贴板列表格式对齐）
    QVariantList out;
    // 把数据 map 放入列表
    out.append(data);
    // 返回编码结果
    return out;
}

// 定义"粘贴"接口：把 encode 产生的数据追加到目标上下文，返回新 id 与新增时钟状态
PasteResult paste(CircuitContext& ctx,
                  const QVariantList& data,
                  double dx, double dy)
{
    // 准备返回结果
    PasteResult result;
    // 数据为空则无可粘贴内容
    if (data.isEmpty()) return result;

    // 取出编码数据的唯一元素（数据 map）
    QVariantMap d = data[0].toMap();
    // 取出元件列表
    QVariantList comps = d.value("components").toList();
    // 取出连线列表
    QVariantList wires = d.value("wires").toList();

    // 旧 id -> 新 id 映射表
    QHash<QString, QString> idMap;
    // 逐个处理元件
    for (auto& cv : comps) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 记录旧 id
        QString oldId = c.value("id").toString();
        // 分配新 id
        QString newId = IdAllocator::nextComponentId(ctx);
        // 建立旧->新映射
        idMap[oldId] = newId;
        // 写入新 id
        c["id"] = newId;
        // 坐标按位移平移
        c["x"] = c.value("x").toDouble() + dx;
        c["y"] = c.value("y").toDouble() + dy;
        // 新时钟元件登记初始状态 false，供调用者同步到 m_clockStates
        if (c.value("type").toString() == "clock")
            result.newClockStates[newId] = false;
        // 追加到目标上下文
        ctx.components.append(c);
        // 记录新 id 到返回列表
        result.newIds.append(newId);
    }

    // 逐个处理连线
    for (auto& wv : wires) {
        // 连线转 map
        QVariantMap w = wv.toMap();
        // 旧起点元件 id
        QString oldFrom = w.value("fromComp").toString();
        // 旧终点元件 id
        QString oldTo = w.value("toComp").toString();
        // 映射到新起点元件 id（失败为空）
        QString from = idMap.value(oldFrom, "");
        // 映射到新终点元件 id（失败为空）
        QString to = idMap.value(oldTo, "");

        // 取线型，缺省回退 bezier
        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";

        // ★ 修复：ortho 线只要一端映射成功就创建（另一端自然悬空，path 已被平移）
        //   bezier 线两端都必须映射成功（贝塞尔依赖两端元件坐标计算曲线）
        // 非 ortho 线：两端若原本有绑定但映射失败，则丢弃该线
        if (wt != "ortho") {
            if (!oldFrom.isEmpty() && from.isEmpty()) continue;
            if (!oldTo.isEmpty() && to.isEmpty()) continue;
        }

        // 准备新连线 map
        QVariantMap nw;
        // 分配新连线 id
        nw["id"] = IdAllocator::nextWireId(ctx);
        // 记录线型
        nw["wireType"] = wt;

        // 起点映射成功则写入新起点元件与端口
        if (!from.isEmpty()) {
            nw["fromComp"] = from;
            nw["fromPort"] = w.value("fromPort");
        }
        // 终点映射成功则写入新终点元件与端口
        if (!to.isEmpty()) {
            nw["toComp"] = to;
            nw["toPort"] = w.value("toPort");
        }

        // ortho 线需要平移 path 顶点
        if (wt == "ortho") {
            // 取出原 path
            QVariantList path = w.value("path").toList();
            // 准备新 path
            QVariantList newPath;
            // 预分配容量
            newPath.reserve(path.size());
            // 逐顶点平移
            for (auto& pv : path) {
                // 顶点转 map
                QVariantMap p = pv.toMap();
                // 准备新顶点
                QVariantMap np;
                // 坐标按位移平移
                np["x"] = p.value("x").toDouble() + dx;
                np["y"] = p.value("y").toDouble() + dy;
                // 追加到新 path
                newPath.append(np);
            }
            // 写入新 path
            nw["path"] = newPath;
        }

        // 追加新连线到目标上下文
        ctx.wires.append(nw);
    }

    // 返回粘贴结果
    return result;
}

} // namespace ClipboardCodec
// 关闭 ClipboardCodec 命名空间