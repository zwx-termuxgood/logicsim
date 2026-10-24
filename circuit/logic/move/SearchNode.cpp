#include "SearchNode.h"
#include "ConnectionData.h"
#include <cmath>
#include <QHash>

SearchNode::SearchNode(ConnectionData* conn, const QVariantMap& src,
                       const QString& srcDir, const QVariantMap& dst)
: m_loc(src), m_dir(srcDir), m_conn(conn), m_dest(dst),
m_dist(0), m_extendsWire(!srcDir.isEmpty()), m_prev(nullptr)
{
    m_heur = m_dist + getHeuristic();
}

SearchNode::SearchNode(const QVariantMap& loc, const QString& dir,
                       ConnectionData* conn, const QVariantMap& dest,
                       int dist, bool extendsWire, SearchNode* prev)
: m_loc(loc), m_dir(dir), m_conn(conn), m_dest(dest),
m_dist(dist), m_extendsWire(extendsWire), m_prev(prev)
{
    m_heur = m_dist + getHeuristic();
}

bool SearchNode::isDestination() const {
    return std::abs(m_loc.value("x").toDouble() - m_dest.value("x").toDouble()) < 0.01
    && std::abs(m_loc.value("y").toDouble() - m_dest.value("y").toDouble()) < 0.01;
}

bool SearchNode::equals(const SearchNode& o) const {
    return m_loc == o.m_loc && m_dir == o.m_dir && m_dest == o.m_dest;
}

int SearchNode::hash() const {
    int dh = qHash(m_dir);
    return ((qHash(m_loc.value("x").toDouble()) * 31 + qHash(m_loc.value("y").toDouble())) * 31 + dh) * 31
    + (qHash(m_dest.value("x").toDouble()) * 31 + qHash(m_dest.value("y").toDouble()));
}

int SearchNode::getHeuristic() {
    double curX = m_loc.value("x").toDouble();
    double curY = m_loc.value("y").toDouble();
    double dstX = m_dest.value("x").toDouble();
    double dstY = m_dest.value("y").toDouble();
    double dx = dstX - curX;
    double dy = dstY - curY;
    int ret = -1;

    if (m_extendsWire) {
        if (m_dir == "east") {
            if (dx > 0) ret = int(dx / 10.0) * 9 + int(std::abs(dy));
        } else if (m_dir == "west") {
            if (dx < 0) ret = int(-dx / 10.0) * 9 + int(std::abs(dy));
        } else if (m_dir == "south") {
            if (dy > 0) ret = int(std::abs(dx)) + int(dy / 10.0) * 9;
        } else if (m_dir == "north") {
            if (dy < 0) ret = int(std::abs(dx)) + int(-dy / 10.0) * 9;
        }
    }

    if (ret < 0) {
        ret = int(std::abs(dx) + std::abs(dy));
    }

    bool penalizeDoubleTurn = false;
    if (m_dir == "east") penalizeDoubleTurn = dx < 0;
    else if (m_dir == "west") penalizeDoubleTurn = dx > 0;
    else if (m_dir == "north") penalizeDoubleTurn = dy > 0;
    else if (m_dir == "south") penalizeDoubleTurn = dy < 0;
    else if (m_dir.isEmpty()) {
        if (dx != 0 || dy != 0) ret += TURN_PENALTY;
    }

    if (penalizeDoubleTurn) ret += 2 * TURN_PENALTY;
    else if (dx != 0 && dy != 0) ret += TURN_PENALTY;

    return ret;
}

SearchNode* SearchNode::next(const QString& moveDir, bool crossing) {
    int newDist = m_dist;
    QString connDir = m_conn ? m_conn->direction() : QString();
    double curX = m_loc.value("x").toDouble();
    double curY = m_loc.value("y").toDouble();
    double nextX = curX, nextY = curY;
    if (moveDir == "east") nextX += 10;
    else if (moveDir == "west") nextX -= 10;
    else if (moveDir == "south") nextY += 10;
    else if (moveDir == "north") nextY -= 10;

    bool exWire = m_extendsWire && moveDir == connDir;
    newDist += exWire ? 9 : 10;
    if (crossing) newDist += CROSSING_PENALTY;
    if (moveDir != m_dir) newDist += TURN_PENALTY;

    if (nextX < 0 || nextY < 0) return nullptr;

    QVariantMap newLoc;
    newLoc["x"] = nextX;
    newLoc["y"] = nextY;
    return new SearchNode(newLoc, moveDir, m_conn, m_dest, newDist, exWire, this);
}
