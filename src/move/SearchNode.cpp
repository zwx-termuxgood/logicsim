#include "SearchNode.h"
// 引入本文件要实现的 SearchNode 类完整声明：定义其成员函数前必须看到类定义
#include "ConnectionData.h"
// 引入连接数据完整定义：next/getHeuristic 需要访问 conn->direction() 等成员
#include <cmath>
// 引入 C 数学库：std::abs 用于坐标差与距离计算
#include <QHash>
// 引入 QHash：qHash 重载函数在此声明，用于坐标与方向的哈希计算

// 定义"起点构造函数"
SearchNode::SearchNode(ConnectionData* conn, const QVariantMap& src,
                       const QString& srcDir, const QVariantMap& dst)
    : m_loc(src), m_dir(srcDir), m_conn(conn), m_dest(dst),
    m_dist(0), m_extendsWire(!srcDir.isEmpty()), m_prev(nullptr)
{
    // 累计距离为 0（起点）；扩展标记取决于是否已有方向；
    // 计算并缓存启发值 f = g + h
    m_heur = m_dist + getHeuristic();
}

// 定义"扩展构造函数"：由 next() 调用，携带父节点与新状态
SearchNode::SearchNode(const QVariantMap& loc, const QString& dir,
                       ConnectionData* conn, const QVariantMap& dest,
                       int dist, bool extendsWire, SearchNode* prev)
    : m_loc(loc), m_dir(dir), m_conn(conn), m_dest(dest),
    m_dist(dist), m_extendsWire(extendsWire), m_prev(prev)
{
    // 计算并缓存启发值 f = g + h
    m_heur = m_dist + getHeuristic();
}

// 定义"判断是否到达目标"接口
bool SearchNode::isDestination() const {
    // 当前位置与目标位置在容差 0.01 内重合即到达
    return std::abs(m_loc.value("x").toDouble() - m_dest.value("x").toDouble()) < 0.01
           && std::abs(m_loc.value("y").toDouble() - m_dest.value("y").toDouble()) < 0.01;
}

// 定义"状态判等"接口：比较位置、方向、目标三者
bool SearchNode::equals(const SearchNode& o) const {
    // 三者都相同才视为同一状态；不含距离/父指针，因为 A* 中同一状态只应保留最优到达方式
    return m_loc == o.m_loc && m_dir == o.m_dir && m_dest == o.m_dest;
}

// 定义"状态哈希"接口
int SearchNode::hash() const {
    // 方向哈希
    int dh = qHash(m_dir);
    // 把位置 x/y、方向、目标 x/y 按 31 逐级组合，使各分量错开位
    return ((qHash(m_loc.value("x").toDouble()) * 31 + qHash(m_loc.value("y").toDouble())) * 31 + dh) * 31
           + (qHash(m_dest.value("x").toDouble()) * 31 + qHash(m_dest.value("y").toDouble()));
}

// 定义"计算启发值"接口：A* 的 h 分量
int SearchNode::getHeuristic() {
    // 当前坐标
    double curX = m_loc.value("x").toDouble();
    double curY = m_loc.value("y").toDouble();
    // 目标坐标
    double dstX = m_dest.value("x").toDouble();
    double dstY = m_dest.value("y").toDouble();
    // 当前到目标的偏移
    double dx = dstX - curX;
    double dy = dstY - curY;
    // 启发值，初值 -1 表示尚未估算
    int ret = -1;

    // 仍在延伸原线且目标在该方向前方：用轴向距离估计（乐观估计）
    if (m_extendsWire) {
        // 向东：目标在右方
        if (m_dir == "east") {
            // 轴向步数按每格 9 计（延伸优惠），加横向偏差
            if (dx > 0) ret = int(dx / 10.0) * 9 + int(std::abs(dy));
            // 向西：目标在左方
        } else if (m_dir == "west") {
            if (dx < 0) ret = int(-dx / 10.0) * 9 + int(std::abs(dy));
            // 向南：目标在下方
        } else if (m_dir == "south") {
            if (dy > 0) ret = int(std::abs(dx)) + int(dy / 10.0) * 9;
            // 向北：目标在上方
        } else if (m_dir == "north") {
            if (dy < 0) ret = int(std::abs(dx)) + int(-dy / 10.0) * 9;
        }
    }

    // 未按轴向估算（不在延伸或目标在反方向）：回退曼哈顿距离
    if (ret < 0) {
        ret = int(std::abs(dx) + std::abs(dy));
    }

    // 是否需要惩罚"回头路"（当前朝向与目标方向相反）
    bool penalizeDoubleTurn = false;
    // 朝东但目标在左：回头
    if (m_dir == "east") penalizeDoubleTurn = dx < 0;
    // 朝西但目标在右：回头
    else if (m_dir == "west") penalizeDoubleTurn = dx > 0;
    // 朝北但目标在下：回头
    else if (m_dir == "north") penalizeDoubleTurn = dy > 0;
    // 朝南但目标在上：回头
    else if (m_dir == "south") penalizeDoubleTurn = dy < 0;
    // 起点尚未定方向：只要目标不在原地，就要先转一次
    else if (m_dir.isEmpty()) {
        if (dx != 0 || dy != 0) ret += TURN_PENALTY;
    }

    // 回头路惩罚两倍转弯代价（强烈抑制）
    if (penalizeDoubleTurn) ret += 2 * TURN_PENALTY;
    // 否则若横向纵向都有偏差，还需一次转弯
    else if (dx != 0 && dy != 0) ret += TURN_PENALTY;

    // 返回启发值
    return ret;
}

// 定义"扩展一个邻居"接口
SearchNode* SearchNode::next(const QString& moveDir, bool crossing) {
    // 新节点的累计距离，初值继承当前
    int newDist = m_dist;
    // 连接的首选方向（无连接则为空）
    QString connDir = m_conn ? m_conn->direction() : QString();
    // 当前坐标
    double curX = m_loc.value("x").toDouble();
    double curY = m_loc.value("y").toDouble();
    // 新坐标，初值同当前
    double nextX = curX, nextY = curY;
    // 按移动方向前进一格（10px）
    if (moveDir == "east") nextX += 10;
    else if (moveDir == "west") nextX -= 10;
    else if (moveDir == "south") nextY += 10;
    else if (moveDir == "north") nextY -= 10;

    // 是否仍在沿原线方向延伸
    bool exWire = m_extendsWire && moveDir == connDir;
    // 步进代价：延伸原线 9，普通 10（略优惠，鼓励顺着原线走）
    newDist += exWire ? 9 : 10;
    // 穿越受限格加交叉惩罚
    if (crossing) newDist += CROSSING_PENALTY;
    // 方向改变加转弯惩罚
    if (moveDir != m_dir) newDist += TURN_PENALTY;

    // 越界（负坐标）返回空指针，表示该方向不可走
    if (nextX < 0 || nextY < 0) return nullptr;

    // 新位置 map
    QVariantMap newLoc;
    newLoc["x"] = nextX;
    newLoc["y"] = nextY;
    // 构造并返回新节点，父指针指向当前节点
    return new SearchNode(newLoc, moveDir, m_conn, m_dest, newDist, exWire, this);
}