#include "AvoidanceMap.h"
// 引入本文件要实现的 AvoidanceMap 类完整声明：定义其成员函数前必须看到类定义
#include "Connector.h"
// 引入 Connector 头：保持与寻路模块其它文件的依赖一致（本文件按需使用）
#include <cmath>
// 引入 C 数学库：std::hypot（线段长度）、std::fmod（网格对齐取模）在此使用

const QString AvoidanceMap::ALLOW_NEITHER    = "neither";
// 定义常量：该格完全禁止穿越
const QString AvoidanceMap::ALLOW_VERTICAL   = "vert";
// 定义常量：该格只允许垂直方向穿越（被水平线占用）
const QString AvoidanceMap::ALLOW_HORIZONTAL = "horz";
// 定义常量：该格只允许水平方向穿越（被垂直线占用）

// 定义"工厂函数：新建地图并一次性标记全部元素"接口
AvoidanceMap* AvoidanceMap::create(const QList<QVariantMap>& elements, int dx, int dy) {
    // 私有构造：通过静态函数内部 new 出对象
    AvoidanceMap* m = new AvoidanceMap();
    // 批量标记所有元件与连线（带整体平移偏移）
    m->markAll(elements, dx, dy);
    // 返回新地图，所有权交给调用方
    return m;
}

// 定义"克隆地图"接口：复制 m_map 生成独立地图
AvoidanceMap* AvoidanceMap::cloneMap() const {
    // 私有构造：新建空地图
    AvoidanceMap* m = new AvoidanceMap();
    // 直接拷贝占用表，得到内容相同的独立副本
    m->m_map = m_map;
    // 返回克隆结果，所有权交给调用方
    return m;
}

// 定义"查询某坐标的允许方向"接口
QString AvoidanceMap::get(double x, double y) const {
    // 按坐标取占用标记；未标记返回空串表示完全自由
    return m_map.value(keyOf(x, y), QString());
}

// 定义"批量标记"接口：按元素类别分派到 markWire 或 markComponent
void AvoidanceMap::markAll(const QList<QVariantMap>& elements, int dx, int dy) {
    // 遍历每个元素
    for (const auto& e : elements) {
        // 取 wireType 字段：有该字段说明是连线
        QString wt = e.value("wireType").toString();
        // 三种线型都按连线标记
        if (wt == "ortho" || wt == "bezier" || wt == "virtual") {
            // 标记连线占用
            markWire(e, dx, dy);
        } else {
            // 否则视为元件，标记元件包围盒占用
            markComponent(e, dx, dy);
        }
    }
}

// 定义"标记元件占用"接口：按类型估算包围盒，对盒内网格点写 ALLOW_NEITHER
void AvoidanceMap::markComponent(const QVariantMap& comp, int dx, int dy) {
    // 取元件类型
    QString type = comp.value("type").toString();
    // 文本元件不占网格，直接返回
    if (type == "text") return;
    // 元件中心坐标（叠加整体平移）
    double cx = comp.value("x").toDouble() + dx;
    double cy = comp.value("y").toDouble() + dy;

    // 默认包围盒宽高
    double w = 80, h = 60;
    // 输入/输出/常量/LED：宽度随位宽增长，高度固定
    if (type == "input" || type == "output" || type == "const" || type == "led") {
        // 取位宽，下限钳到 1
        int bw = comp.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
        // 宽度取基础值与按位宽估算值的较大者
        w = qMax(type == "led" ? 50.0 : (type == "output" ? 60.0 : 60.0), bw * 22.0 + 8.0);
        // 高度固定
        h = 40;
        // 子电路实例：宽度固定，高度随端口数增长
    } else if (type == "sub") {
        // 宽度固定
        w = 110;
        // 输入端口数量
        int ic = comp.value("subInputNames").toList().size();
        // 输出端口数量
        int oc = comp.value("subOutputNames").toList().size();
        // 高度按端口数估算，至少 60
        h = qMax(60.0, (double)qMax(ic, oc) * 24.0);
        // 分线器/汇线器：宽度固定，高度随分段数增长
    } else if (type == "splitter" || type == "hub") {
        // 分段数量，至少 1
        int n = comp.value("outputSplits").toList().size();
        if (n < 1) n = 1;
        // 宽度固定，高度按分段数估算
        w = 80; h = qMax(60.0, (double)n * 24.0);
        // 时钟：尺寸固定
    } else if (type == "clock") {
        // 宽度与高度
        w = 60; h = 40;
    }

    // 包围盒左上角
    double x0 = cx - w / 2.0;
    double y0 = cy - h / 2.0;
    // 包围盒右下角
    double x1 = cx + w / 2.0;
    double y1 = cy + h / 2.0;
    // 左上角对齐到 10px 网格（向下取整到网格点）
    x0 += 9 - std::fmod(x0 + 9, 10);
    y0 += 9 - std::fmod(y0 + 9, 10);
    // 逐格遍历包围盒
    for (double x = x0; x <= x1; x += 10) {
        // 逐行遍历
        for (double y = y0; y <= y1; y += 10) {
            // 格子键
            QString k = keyOf(x, y);
            // 取该格原有标记
            QString prev = m_map.value(k);
            // 先置为完全禁止
            m_map[k] = ALLOW_NEITHER;
            // 若原本不是 ALLOW_NEITHER，需判断该格是否真的落在盒内
            if (prev != ALLOW_NEITHER) {
                // 若该点不在组件包围盒内，恢复
                // 还原到未平移坐标，避免因 dx/dy 偏移误判
                double baseX = x - dx;
                double baseY = y - dy;
                // 落在盒外则恢复原标记（或删除）
                if (baseX < cx - w / 2.0 || baseX > cx + w / 2.0 + dx * 0 ||
                    baseY < cy - h / 2.0 || baseY > cy + h / 2.0 + dy * 0) {
                    // 原本无标记则删除；否则恢复
                    if (prev.isEmpty()) m_map.remove(k);
                    else m_map[k] = prev;
                }
            }
        }
    }
}

// 定义"标记连线占用"接口
void AvoidanceMap::markWire(const QVariantMap& wire, int dx, int dy) {
    // 取线型
    QString wt = wire.value("wireType").toString();
    // ortho 线需要沿每段采样标记
    if (wt == "ortho") {
        // 取顶点列表
        QVariantList path = wire.value("path").toList();
        // 相邻顶点两两成段
        for (int i = 0; i + 1 < path.size(); ++i) {
            // 段起点
            QVariantMap p0 = path[i].toMap();
            // 段终点
            QVariantMap p1 = path[i + 1].toMap();
            // 段起点坐标（叠加平移）
            double x0 = p0.value("x").toDouble() + dx;
            double y0 = p0.value("y").toDouble() + dy;
            // 段终点坐标（叠加平移）
            double x1 = p1.value("x").toDouble() + dx;
            double y1 = p1.value("y").toDouble() + dy;
            // 两端点直接置为完全禁止
            m_map[keyOf(x0, y0)] = ALLOW_NEITHER;
            m_map[keyOf(x1, y1)] = ALLOW_NEITHER;
            // 段长度
            double len = std::hypot(x1 - x0, y1 - y0);
            // 采样步数（每 10px 一步）
            int n = int(len / 10);
            // 除零保护
            if (n < 1) n = 1;    // ★ 除零保护
            // 垂直段：占的是"只许垂直穿越"格
            if (x0 == x1) {
                // 逐采样点标记
                for (int s = 0; s <= n; ++s) {
                    // 采样点 y 坐标
                    double yy = y0 + (y1 - y0) * s / n;
                    // 格子键
                    QString k = keyOf(x0, yy);
                    // 取该格原有标记
                    QString prev = m_map.value(k);
                    // 先置为只许垂直穿越
                    m_map[k] = ALLOW_HORIZONTAL;
                    // 若该格已被完全禁止或被相反方向占用，则升级为完全禁止
                    if (prev == ALLOW_NEITHER || prev == ALLOW_VERTICAL)
                        m_map[k] = ALLOW_NEITHER;
                }
                // 水平段：占的是"只许水平穿越"格
            } else if (y0 == y1) {
                // 逐采样点标记
                for (int s = 0; s <= n; ++s) {
                    // 采样点 x 坐标
                    double xx = x0 + (x1 - x0) * s / n;
                    // 格子键
                    QString k = keyOf(xx, y0);
                    // 取该格原有标记
                    QString prev = m_map.value(k);
                    // 先置为只许水平穿越
                    m_map[k] = ALLOW_VERTICAL;
                    // 若该格已被完全禁止或被相反方向占用，则升级为完全禁止
                    if (prev == ALLOW_NEITHER || prev == ALLOW_HORIZONTAL)
                        m_map[k] = ALLOW_NEITHER;
                }
            }
        }
        // 贝塞尔/未知线型：曲线难以精确离散，且寻路只对 ortho 生效，不占网格
    } else if (wt == "bezier" || wt.isEmpty()) {
        // 贝塞尔：端点不占网格
    }
}

// 定义"取消某坐标标记"接口
void AvoidanceMap::unmarkLocation(double x, double y) {
    // 直接移除该坐标的占用条目
    m_map.remove(keyOf(x, y));
}

// 定义"取消某连线的标记"接口
void AvoidanceMap::unmarkWire(const QVariantMap& wire, const QVariantMap& deletedEnd) {
    // 取线型
    QString wt = wire.value("wireType").toString();
    // 只处理 ortho 线
    if (wt != "ortho") return;
    // 取顶点列表
    QVariantList path = wire.value("path").toList();
    // 相邻顶点两两成段
    for (int i = 0; i + 1 < path.size(); ++i) {
        // 段起点
        QVariantMap p0 = path[i].toMap();
        // 段终点
        QVariantMap p1 = path[i + 1].toMap();
        // 段起点坐标
        double x0 = p0.value("x").toDouble();
        double y0 = p0.value("y").toDouble();
        // 段终点坐标
        double x1 = p1.value("x").toDouble();
        double y1 = p1.value("y").toDouble();
        // 段长度
        double len = std::hypot(x1 - x0, y1 - y0);
        // 采样步数（每 10px 一步）
        int n = int(len / 10);
        // 除零保护
        if (n < 1) n = 1;    // ★ 除零保护
        // 垂直段：原本标记为 ALLOW_HORIZONTAL
        if (x0 == x1) {
            // 逐采样点处理
            for (int s = 0; s <= n; ++s) {
                // 采样点 y 坐标
                double yy = y0 + (y1 - y0) * s / n;
                // 格子键
                QString k = keyOf(x0, yy);
                // 取该格原有标记
                QString prev = m_map.value(k);
                // 先移除该格
                m_map.remove(k);
                // 若原本是只许水平穿越，移除后恢复为只许垂直穿越
                if (prev == ALLOW_HORIZONTAL) m_map[k] = ALLOW_VERTICAL;
            }
            // 水平段：原本标记为 ALLOW_VERTICAL
        } else if (y0 == y1) {
            // 逐采样点处理
            for (int s = 0; s <= n; ++s) {
                // 采样点 x 坐标
                double xx = x0 + (x1 - x0) * s / n;
                // 格子键
                QString k = keyOf(xx, y0);
                // 取该格原有标记
                QString prev = m_map.value(k);
                // 先移除该格
                m_map.remove(k);
                // 若原本不是只许垂直穿越，移除后恢复为只许水平穿越
                if (prev != ALLOW_VERTICAL) m_map[k] = ALLOW_HORIZONTAL;
            }
        }
    }
}