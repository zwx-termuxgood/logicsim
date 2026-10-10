#ifndef WIRE_H
#define WIRE_H

#include <QVariantMap>
#include <QString>

/**
 * @brief 连线（QVariantMap）的通用存取器。
 */
namespace Wire {

inline QString id(const QVariantMap& w)       { return w.value("id").toString(); }
inline QString fromComp(const QVariantMap& w) { return w.value("fromComp").toString(); }
inline QString toComp(const QVariantMap& w)   { return w.value("toComp").toString(); }
inline int fromPort(const QVariantMap& w)     { return w.value("fromPort").toInt(); }
inline int toPort(const QVariantMap& w)       { return w.value("toPort").toInt(); }

inline bool connects(const QVariantMap& w, const QString& compId) {
    return fromComp(w) == compId || toComp(w) == compId;
}

} // namespace Wire

#endif // WIRE_H
