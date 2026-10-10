#ifndef JSONCODEC_H
#define JSONCODEC_H

#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QString>
#include "../core/CircuitContext.h"

/**
 * @brief 存档 JSON 的编解码。
 *
 * 只负责数据层转换，不涉及 Circuit 的成员状态。
 * 解析结果通过 DecodedData 结构返回，由调用者填入自身成员。
 */
namespace JsonCodec {

QVariantMap encode(const CircuitContext& root,
                   const QHash<QString, CircuitContext>& subContexts,
                   const QHash<QString, QString>& subNames,
                   int subCounter,
                   int clockFrequency,
                   int clockTickCount,
                   const QString& engineType,
                   qulonglong globalGateDelay,
                   qulonglong wireDelay,
                   qulonglong simulationSpeed,
                   int simulationSpeedExp,
                   bool autoSpeed,
                   const QHash<QString, quint64>& gateTypeDelays);

struct DecodedData {
    CircuitContext root;
    QHash<QString, CircuitContext> subContexts;
    QHash<QString, QString> subNames;
    int subCounter = 0;
    int clockFrequency = 2;
    int clockTickCount = 0;
    QString engineType = "event";
    qulonglong globalGateDelay = 1;
    qulonglong wireDelay = 1;
    qulonglong simulationSpeed = 1000;
    int simulationSpeedExp = 3;
    bool autoSpeed = true;
    QHash<QString, quint64> gateTypeDelays;
    QHash<QString, bool> clockStates;
};

DecodedData decode(const QVariantMap& data);

} // namespace JsonCodec

#endif // JSONCODEC_H
