#include "CircuitContext.h"
// 引入本文件要实现的 CircuitContext 结构体完整定义：定义其成员函数前必须看到结构声明

// 定义"在当前上下文中按 ID 查找元件索引"接口
int CircuitContext::indexOfComponent(const QString& id) const {
    // 线性遍历元件列表
    for (int i = 0; i < components.size(); ++i)
        // 逐个比较元件 map 的 id 字段，命中即返回索引
        if (components[i].toMap().value("id").toString() == id) return i;
    // 遍历完仍未命中，返回 -1 表示不存在
    return -1;
}

// 定义"在当前上下文中按 ID 查找连线索引"接口
int CircuitContext::indexOfWire(const QString& id) const {
    // 线性遍历连线列表
    for (int i = 0; i < wires.size(); ++i)
        // 逐个比较连线 map 的 id 字段，命中即返回索引
        if (wires[i].toMap().value("id").toString() == id) return i;
    // 遍历完仍未命中，返回 -1 表示不存在
    return -1;
}