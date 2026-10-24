#include "ClipboardCodec.h"
#include "../core/IdAllocator.h"
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <cmath>
#include <algorithm>

namespace ClipboardCodec {

namespace {

QString posKeyOf(const QVariantMap& p) {
    return QString("%1,%2").arg(int(std::round(p.value("x").toDouble())))
    .arg(int(std::round(p.value("y").toDouble())));
}

// 点是否严格落在正交线段内部（不含端点）
bool pointOnSegInterior(double px, double py,
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

// 点是否落在 path 的任意段内部（不含端点）
bool pointOnPathInterior(double px, double py, const QVariantList& path) {
    for (int i = 0; i + 1 < path.size(); ++i) {
        QVariantMap p0 = path[i].toMap();
        QVariantMap p1 = path[i + 1].toMap();
        if (pointOnSegInterior(px, py,
                               p0.value("x").toDouble(), p0.value("y").toDouble(),
                               p1.value("x").toDouble(), p1.value("y").toDouble())) {
            return true;
        }
    }
    return false;
}

} // namespace

QVariantList encode(const CircuitContext& ctx, const QStringList& ids) {
    QSet<QString> idSet;
    for (const auto& id : ids) idSet.insert(id);

    // 区分：只选线 vs 选元件
    bool hasComponentsSelected = false;
    for (auto& cv : ctx.components) {
        if (idSet.contains(cv.toMap().value("id").toString())) {
            hasComponentsSelected = true;
            break;
        }
    }

    QVariantList compsOut;
    for (auto& cv : ctx.components) {
        auto c = cv.toMap();
        if (idSet.contains(c.value("id").toString())) compsOut.append(c);
    }

    QSet<QString> selectedWireIds;

    if (!hasComponentsSelected) {
        // 只选线：只复制明确选中的线
        for (auto& wv : ctx.wires) {
            auto w = wv.toMap();
            if (idSet.contains(w.value("id").toString()))
                selectedWireIds.insert(w.value("id").toString());
        }
    } else {
        // 选元件：第一轮直接相连
        for (auto& wv : ctx.wires) {
            auto w = wv.toMap();
            QString wid = w.value("id").toString();
            if (idSet.contains(wid)) { selectedWireIds.insert(wid); continue; }
            QString fromComp = w.value("fromComp").toString();
            QString toComp = w.value("toComp").toString();
            if ((!fromComp.isEmpty() && idSet.contains(fromComp)) ||
                (!toComp.isEmpty() && idSet.contains(toComp))) {
                selectedWireIds.insert(wid);
            }
        }

        // 第二轮：端点对端点 + 正反 T 型接入
        bool changed = true;
        int guard = 0;
        while (changed && guard++ < 200) {
            changed = false;

            QSet<QString> selVertexKeys;
            QVector<QVariantList> selPaths;
            for (auto& wv : ctx.wires) {
                auto w = wv.toMap();
                if (!selectedWireIds.contains(w.value("id").toString())) continue;
                QVariantList path = w.value("path").toList();
                if (path.isEmpty()) continue;
                selPaths.append(path);
                for (auto& pv : path)
                    selVertexKeys.insert(posKeyOf(pv.toMap()));
            }

            for (auto& wv : ctx.wires) {
                auto w = wv.toMap();
                QString wid = w.value("id").toString();
                if (selectedWireIds.contains(wid)) continue;
                QVariantList path = w.value("path").toList();
                if (path.isEmpty()) continue;

                bool matched = false;

                if (selVertexKeys.contains(posKeyOf(path.first().toMap())) ||
                    selVertexKeys.contains(posKeyOf(path.last().toMap()))) {
                    matched = true;
                }

                if (!matched) {
                    QVariantMap p0 = path.first().toMap();
                    QVariantMap pn = path.last().toMap();
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

                if (!matched) {
                    for (const auto& selPath : selPaths) {
                        QVariantMap a = selPath.first().toMap();
                        QVariantMap b = selPath.last().toMap();
                        if (pointOnPathInterior(a.value("x").toDouble(),
                                                a.value("y").toDouble(), path) ||
                            pointOnPathInterior(b.value("x").toDouble(),
                                                b.value("y").toDouble(), path)) {
                            matched = true;
                            break;
                        }
                    }
                }

                if (matched) {
                    selectedWireIds.insert(wid);
                    changed = true;
                }
            }
        }
    }

    QVariantList wiresOut;
    for (auto& wv : ctx.wires) {
        auto w = wv.toMap();
        if (selectedWireIds.contains(w.value("id").toString()))
            wiresOut.append(w);
    }

    QVariantMap data;
    data["components"] = compsOut;
    data["wires"] = wiresOut;
    QVariantList out;
    out.append(data);
    return out;
}

PasteResult paste(CircuitContext& ctx,
                  const QVariantList& data,
                  double dx, double dy)
{
    PasteResult result;
    if (data.isEmpty()) return result;

    QVariantMap d = data[0].toMap();
    QVariantList comps = d.value("components").toList();
    QVariantList wires = d.value("wires").toList();

    QHash<QString, QString> idMap;
    for (auto& cv : comps) {
        QVariantMap c = cv.toMap();
        QString oldId = c.value("id").toString();
        QString newId = IdAllocator::nextComponentId(ctx);
        idMap[oldId] = newId;
        c["id"] = newId;
        c["x"] = c.value("x").toDouble() + dx;
        c["y"] = c.value("y").toDouble() + dy;
        if (c.value("type").toString() == "clock")
            result.newClockStates[newId] = false;
        ctx.components.append(c);
        result.newIds.append(newId);
    }

    for (auto& wv : wires) {
        QVariantMap w = wv.toMap();
        QString oldFrom = w.value("fromComp").toString();
        QString oldTo = w.value("toComp").toString();
        QString from = idMap.value(oldFrom, "");
        QString to = idMap.value(oldTo, "");

        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";

        // ★ 修复：ortho 线只要一端映射成功就创建（另一端自然悬空，path 已被平移）
        //   bezier 线两端都必须映射成功（贝塞尔依赖两端元件坐标计算曲线）
        if (wt != "ortho") {
            if (!oldFrom.isEmpty() && from.isEmpty()) continue;
            if (!oldTo.isEmpty() && to.isEmpty()) continue;
        }

        QVariantMap nw;
        nw["id"] = IdAllocator::nextWireId(ctx);
        nw["wireType"] = wt;

        if (!from.isEmpty()) {
            nw["fromComp"] = from;
            nw["fromPort"] = w.value("fromPort");
        }
        if (!to.isEmpty()) {
            nw["toComp"] = to;
            nw["toPort"] = w.value("toPort");
        }

        if (wt == "ortho") {
            QVariantList path = w.value("path").toList();
            QVariantList newPath;
            newPath.reserve(path.size());
            for (auto& pv : path) {
                QVariantMap p = pv.toMap();
                QVariantMap np;
                np["x"] = p.value("x").toDouble() + dx;
                np["y"] = p.value("y").toDouble() + dy;
                newPath.append(np);
            }
            nw["path"] = newPath;
        }

        ctx.wires.append(nw);
    }

    return result;
}

} // namespace ClipboardCodec