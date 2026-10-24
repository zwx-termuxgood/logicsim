#ifndef CONNECTIONDATA_H
#define CONNECTIONDATA_H

#include <QVariantMap>
#include <QList>
#include <QString>
#include <QHash>

class ConnectionData {
public:
    ConnectionData(const QVariantMap& loc, const QString& dir,
                   const QList<QVariantMap>& wirePath,
                   const QVariantMap& wirePathStart)
    : m_loc(loc), m_dir(dir), m_wirePath(wirePath), m_wirePathStart(wirePathStart) {}

    QVariantMap location() const { return m_loc; }
    QString direction() const { return m_dir; }
    const QList<QVariantMap>& wirePath() const { return m_wirePath; }
    QVariantMap wirePathStart() const { return m_wirePathStart; }

    bool equals(const ConnectionData& o) const {
        return m_loc == o.m_loc && m_dir == o.m_dir;
    }

    int hash() const {
        return qHash(m_loc.value("x").toDouble()) * 31
        + qHash(m_loc.value("y").toDouble()) * 31
        + qHash(m_dir);
    }

private:
    QVariantMap m_loc;
    QString m_dir;
    QList<QVariantMap> m_wirePath;
    QVariantMap m_wirePathStart;
};

#endif
