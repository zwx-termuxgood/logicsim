#ifndef SIMENGINE_H
#define SIMENGINE_H

#include <QString>

/**
 * @brief 引擎种类枚举 + 字符串互转。
 *
 * 只有两套引擎，用 enum 分支即可，不引入虚接口。
 */
enum class EngineKind { Event, Iter };

inline EngineKind engineKindFromString(const QString& s) {
    return s == "iter" ? EngineKind::Iter : EngineKind::Event;
}

inline QString engineKindToString(EngineKind k) {
    return k == EngineKind::Iter ? QStringLiteral("iter") : QStringLiteral("event");
}

#endif // SIMENGINE_H
