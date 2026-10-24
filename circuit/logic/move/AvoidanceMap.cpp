#include "AvoidanceMap.h"
#include "Connector.h"
#include <cmath>

const QString AvoidanceMap::ALLOW_NEITHER    = "neither";
const QString AvoidanceMap::ALLOW_VERTICAL   = "vert";
const QString AvoidanceMap::ALLOW_HORIZONTAL = "horz";

AvoidanceMap* AvoidanceMap::create(const QList<QVariantMap>& elements, int dx, int dy) {
    AvoidanceMap* m = new AvoidanceMap();
    m->markAll(elements, dx, dy);
    return m;
}

AvoidanceMap* AvoidanceMap::cloneMap() const {
    AvoidanceMap* m = new AvoidanceMap();
    m->m_map = m_map;
    return m;
}

QString AvoidanceMap::get(double x, double y) const {
    return m_map.value(keyOf(x, y), QString());
}

void AvoidanceMap::markAll(const QList<QVariantMap>& elements, int dx, int dy) {
    for (const auto& e : elements) {
        QString wt = e.value("wireType").toString();
        if (wt == "ortho" || wt == "bezier" || wt == "virtual") {
            markWire(e, dx, dy);
        } else {
            markComponent(e, dx, dy);
        }
    }
}

void AvoidanceMap::markComponent(const QVariantMap& comp, int dx, int dy) {
    QString type = comp.value("type").toString();
    if (type == "text") return;
    double cx = comp.value("x").toDouble() + dx;
    double cy = comp.value("y").toDouble() + dy;

    double w = 80, h = 60;
    if (type == "input" || type == "output" || type == "const" || type == "led") {
        int bw = comp.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
        w = qMax(type == "led" ? 50.0 : (type == "output" ? 60.0 : 60.0), bw * 22.0 + 8.0);
        h = 40;
    } else if (type == "sub") {
        w = 110;
        int ic = comp.value("subInputNames").toList().size();
        int oc = comp.value("subOutputNames").toList().size();
        h = qMax(60.0, (double)qMax(ic, oc) * 24.0);
    } else if (type == "splitter" || type == "hub") {
        int n = comp.value("outputSplits").toList().size();
        if (n < 1) n = 1;
        w = 80; h = qMax(60.0, (double)n * 24.0);
    } else if (type == "clock") {
        w = 60; h = 40;
    }

    double x0 = cx - w / 2.0;
    double y0 = cy - h / 2.0;
    double x1 = cx + w / 2.0;
    double y1 = cy + h / 2.0;
    x0 += 9 - std::fmod(x0 + 9, 10);
    y0 += 9 - std::fmod(y0 + 9, 10);
    for (double x = x0; x <= x1; x += 10) {
        for (double y = y0; y <= y1; y += 10) {
            QString k = keyOf(x, y);
            QString prev = m_map.value(k);
            m_map[k] = ALLOW_NEITHER;
            if (prev != ALLOW_NEITHER) {
                // 若该点不在组件包围盒内，恢复
                double baseX = x - dx;
                double baseY = y - dy;
                if (baseX < cx - w / 2.0 || baseX > cx + w / 2.0 + dx * 0 ||
                    baseY < cy - h / 2.0 || baseY > cy + h / 2.0 + dy * 0) {
                    if (prev.isEmpty()) m_map.remove(k);
                    else m_map[k] = prev;
                }
            }
        }
    }
}

void AvoidanceMap::markWire(const QVariantMap& wire, int dx, int dy) {
    QString wt = wire.value("wireType").toString();
    if (wt == "ortho") {
        QVariantList path = wire.value("path").toList();
        for (int i = 0; i + 1 < path.size(); ++i) {
            QVariantMap p0 = path[i].toMap();
            QVariantMap p1 = path[i + 1].toMap();
            double x0 = p0.value("x").toDouble() + dx;
            double y0 = p0.value("y").toDouble() + dy;
            double x1 = p1.value("x").toDouble() + dx;
            double y1 = p1.value("y").toDouble() + dy;
            m_map[keyOf(x0, y0)] = ALLOW_NEITHER;
            m_map[keyOf(x1, y1)] = ALLOW_NEITHER;
            double len = std::hypot(x1 - x0, y1 - y0);
            int n = int(len / 10);
            if (n < 1) n = 1;    // ★ 除零保护
            if (x0 == x1) {
                for (int s = 0; s <= n; ++s) {
                    double yy = y0 + (y1 - y0) * s / n;
                    QString k = keyOf(x0, yy);
                    QString prev = m_map.value(k);
                    m_map[k] = ALLOW_HORIZONTAL;
                    if (prev == ALLOW_NEITHER || prev == ALLOW_VERTICAL)
                        m_map[k] = ALLOW_NEITHER;
                }
            } else if (y0 == y1) {
                for (int s = 0; s <= n; ++s) {
                    double xx = x0 + (x1 - x0) * s / n;
                    QString k = keyOf(xx, y0);
                    QString prev = m_map.value(k);
                    m_map[k] = ALLOW_VERTICAL;
                    if (prev == ALLOW_NEITHER || prev == ALLOW_HORIZONTAL)
                        m_map[k] = ALLOW_NEITHER;
                }
            }
        }
    } else if (wt == "bezier" || wt.isEmpty()) {
        // 贝塞尔：端点不占网格
    }
}

void AvoidanceMap::unmarkLocation(double x, double y) {
    m_map.remove(keyOf(x, y));
}

void AvoidanceMap::unmarkWire(const QVariantMap& wire, const QVariantMap& deletedEnd) {
    QString wt = wire.value("wireType").toString();
    if (wt != "ortho") return;
    QVariantList path = wire.value("path").toList();
    for (int i = 0; i + 1 < path.size(); ++i) {
        QVariantMap p0 = path[i].toMap();
        QVariantMap p1 = path[i + 1].toMap();
        double x0 = p0.value("x").toDouble();
        double y0 = p0.value("y").toDouble();
        double x1 = p1.value("x").toDouble();
        double y1 = p1.value("y").toDouble();
        double len = std::hypot(x1 - x0, y1 - y0);
        int n = int(len / 10);
        if (n < 1) n = 1;    // ★ 除零保护
        if (x0 == x1) {
            for (int s = 0; s <= n; ++s) {
                double yy = y0 + (y1 - y0) * s / n;
                QString k = keyOf(x0, yy);
                QString prev = m_map.value(k);
                m_map.remove(k);
                if (prev == ALLOW_HORIZONTAL) m_map[k] = ALLOW_VERTICAL;
            }
        } else if (y0 == y1) {
            for (int s = 0; s <= n; ++s) {
                double xx = x0 + (x1 - x0) * s / n;
                QString k = keyOf(xx, y0);
                QString prev = m_map.value(k);
                m_map.remove(k);
                if (prev != ALLOW_VERTICAL) m_map[k] = ALLOW_HORIZONTAL;
            }
        }
    }
}