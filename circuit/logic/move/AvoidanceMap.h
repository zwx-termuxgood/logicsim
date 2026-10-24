#ifndef AVOIDANCEMAP_H
#define AVOIDANCEMAP_H

#include <QVariantMap>
#include <QList>
#include <QHash>
#include <QString>

class AvoidanceMap {
public:
    static const QString ALLOW_NEITHER;
    static const QString ALLOW_VERTICAL;
    static const QString ALLOW_HORIZONTAL;

    static AvoidanceMap* create(const QList<QVariantMap>& elements, int dx, int dy);
    AvoidanceMap* cloneMap() const;

    QString get(double x, double y) const;
    void markAll(const QList<QVariantMap>& elements, int dx, int dy);
    void markComponent(const QVariantMap& comp, int dx, int dy);
    void markWire(const QVariantMap& wire, int dx, int dy);
    void unmarkLocation(double x, double y);
    void unmarkWire(const QVariantMap& wire, const QVariantMap& deletedEnd);

private:
    AvoidanceMap() {}
    QHash<QString, QString> m_map;

    static QString keyOf(double x, double y) {
        return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
    }
};

#endif
