#ifndef COMPONENTLOGIC_H
#define COMPONENTLOGIC_H

#include <QVariantMap>
#include <QVariantList>
#include <QString>

/**
 * @brief 元件类型的基础属性查询 + 端口几何计算。
 *
 * 这是 logic 层唯一的"元件知识入口"。
 * 引擎、QML 端口面板、错误检查都通过这里获取元件信息。
 */
namespace ComponentLogic {

int inputCount(const QVariantMap& comp);
int outputCount(const QVariantMap& comp);
int outputBitWidth(const QVariantMap& comp, int portIdx);
int inputBitWidth(const QVariantMap& comp, int portIdx);

QVariantList outputPorts(const QVariantMap& comp);
QVariantList inputPorts(const QVariantMap& comp);

} // namespace ComponentLogic

#endif // COMPONENTLOGIC_H
