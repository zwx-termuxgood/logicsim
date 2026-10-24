#ifndef SEARCHNODE_H
#define SEARCHNODE_H

#include <QVariantMap>
#include <QString>

class ConnectionData;

class SearchNode {
public:
    static const int CROSSING_PENALTY = 20;
    static const int TURN_PENALTY = 50;

    SearchNode(ConnectionData* conn, const QVariantMap& src,
               const QString& srcDir, const QVariantMap& dst);

    SearchNode(const QVariantMap& loc, const QString& dir, ConnectionData* conn,
               const QVariantMap& dest, int dist, bool extendsWire, SearchNode* prev);

    int heuristicValue() const { return m_heur; }
    QVariantMap location() const { return m_loc; }
    QVariantMap destination() const { return m_dest; }
    QString direction() const { return m_dir; }
    int distance() const { return m_dist; }
    SearchNode* previous() const { return m_prev; }
    bool isDestination() const;
    bool isStart() const { return m_prev == nullptr; }
    bool isExtendingWire() const { return m_extendsWire; }
    ConnectionData* connection() const { return m_conn; }

    bool equals(const SearchNode& o) const;
    int hash() const;

    SearchNode* next(const QString& moveDir, bool crossing);

private:
    int getHeuristic();

    QVariantMap m_loc;
    QString m_dir;
    ConnectionData* m_conn;
    QVariantMap m_dest;
    int m_dist;
    int m_heur;
    bool m_extendsWire;
    SearchNode* m_prev;
};

#endif
