#ifndef CIRCUITCONTEXT_H
#define CIRCUITCONTEXT_H

#include <QVariantList>
#include <QString>

/**
 * @brief 电路上下文：一组元件与连线。
 *
 * 一个 Circuit 由 1 个根上下文（主电路）+ N 个子电路上下文组成。
 */
struct CircuitContext {
    QVariantList components;   // 元件列表（每个元素为 QVariantMap）
    QVariantList wires;        // 连线列表（每个元素为 QVariantMap）
    int compCounter = 0;       // 元件 ID 计数器
    int wireCounter = 0;       // 连线 ID 计数器

    int indexOfComponent(const QString& id) const;
    int indexOfWire(const QString& id) const;
    bool isEmpty() const { return components.isEmpty() && wires.isEmpty(); }
};

#endif // CIRCUITCONTEXT_H
