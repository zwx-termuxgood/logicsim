#ifndef COMPONENT_H
#define COMPONENT_H

#include <QVariantMap>
#include <QString>

namespace Component {

// ---- 字段读取 ----
inline QString id(const QVariantMap& c)     { return c.value("id").toString(); }
inline QString type(const QVariantMap& c)   { return c.value("type").toString(); }
inline QString name(const QVariantMap& c)   { return c.value("name").toString(); }
inline QString subId(const QVariantMap& c)  { return c.value("subId").toString(); }
inline QString displayBase(const QVariantMap& c) {
    QString b = c.value("displayBase").toString();
    return b.isEmpty() ? QStringLiteral("bin") : b;
}
inline int bitWidth(const QVariantMap& c) {
    int b = c.value("bitWidth").toInt();
    return b < 1 ? 1 : (b > 64 ? 64 : b);
}
inline int inputCount(const QVariantMap& c) { return c.value("inputCount").toInt(); }
inline int rotation(const QVariantMap& c)   { return c.value("rotation").toInt(); }
inline double x(const QVariantMap& c)       { return c.value("x").toDouble(); }
inline double y(const QVariantMap& c)       { return c.value("y").toDouble(); }

// ---- 类型判定 ----
inline bool isInput(const QVariantMap& c)  { QString t = type(c); return t == "input" || t == "const"; }
inline bool isEditable(const QVariantMap& c) {
    QString t = type(c);
    return t == "input" || t == "const";
}
inline bool isClock(const QVariantMap& c)  { return type(c) == "clock"; }
inline bool isOutput(const QVariantMap& c) { return type(c) == "output"; }
inline bool isLed(const QVariantMap& c)    { return type(c) == "led"; }
inline bool isSub(const QVariantMap& c)    { return type(c) == "sub"; }
inline bool isText(const QVariantMap& c)   { return type(c) == "text"; }
inline bool isSplitter(const QVariantMap& c) { return type(c) == "splitter"; }
inline bool isHub(const QVariantMap& c)    { return type(c) == "hub"; }
inline bool isGate(const QVariantMap& c) {
    QString t = type(c);
    return t == "and" || t == "or" || t == "nand" ||
           t == "nor" || t == "xor" || t == "xnor" || t == "not";
}
inline bool isTransistor(const QVariantMap& c) {
    QString t = type(c);
    return t == "tgate" || t == "ntran" || t == "ptran";
}

// ---- inline：不再需要 Component.cpp ----
inline QString qualifiedId(const QString& prefix, const QString& localId) {
    return prefix.isEmpty() ? localId : (prefix + "/" + localId);
}

} // namespace Component

#endif // COMPONENT_H
