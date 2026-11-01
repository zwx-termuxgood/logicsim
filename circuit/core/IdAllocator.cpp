#include "IdAllocator.h"
// 引入 IdAllocator 命名空间声明：实现其两个发号函数前必须看到函数原型与 CircuitContext 定义

namespace IdAllocator {
// 打开 IdAllocator 命名空间，与头文件声明保持一致

// 定义"生成下一个元件 ID"接口
QString nextComponentId(CircuitContext& ctx) {
    // 自增该上下文的元件计数器后格式化为 "C<编号>" 返回
    return QString("C%1").arg(++ctx.compCounter);
}

// 定义"生成下一个连线 ID"接口
QString nextWireId(CircuitContext& ctx) {
    // 自增该上下文的连线计数器后格式化为 "W<编号>" 返回
    return QString("W%1").arg(++ctx.wireCounter);
}

} // namespace IdAllocator
// 关闭 IdAllocator 命名空间