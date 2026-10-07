#ifndef PORTGEOMETRY_H
#define PORTGEOMETRY_H

#include <QVariantList>
#include <QVariantMap>

/**
 * @brief 计算元件各端口在元件本地坐标系下的相对位置与位宽。
 *
 * 每个端口以 QVariantMap 描述：{ index, x, y, bitWidth }。
 * 元件中心为原点。
 */
namespace PortGeometry {

QVariantList outputPorts(const QVariantMap& comp);
QVariantList inputPorts(const QVariantMap& comp);

} // namespace PortGeometry

#endif // PORTGEOMETRY_H