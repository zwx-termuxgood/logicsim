#ifndef IDALLOCATOR_H
#define IDALLOCATOR_H

#include <QString>
#include "CircuitContext.h"

/**
 * @brief 元件 / 连线 ID 生成器。
 *
 * 只负责自增计数器与格式化，不涉及具体电路逻辑。
 */
namespace IdAllocator {

QString nextComponentId(CircuitContext& ctx);
QString nextWireId(CircuitContext& ctx);

} // namespace IdAllocator

#endif // IDALLOCATOR_H
