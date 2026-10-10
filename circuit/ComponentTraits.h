#ifndef COMPONENTTRAITS_H
#define COMPONENTTRAITS_H

#include <QVariantMap>
#include <QString>

/**
 * @brief 元件类型的基础属性查询（纯函数，无状态）。
 */
namespace ComponentTraits {

int inputCount(const QVariantMap& comp);
int outputCount(const QVariantMap& comp);
int outputBitWidth(const QVariantMap& comp, int portIdx);
int inputBitWidth(const QVariantMap& comp, int portIdx);

} // namespace ComponentTraits

#endif // COMPONENTTRAITS_H