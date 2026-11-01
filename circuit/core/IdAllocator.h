#ifndef IDALLOCATOR_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define IDALLOCATOR_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QString>
// 引入 QString：两个函数的返回类型都是 QString（新生成的 ID 字符串）

#include "CircuitContext.h"
// 引入 CircuitContext 完整定义：函数参数是 CircuitContext&，
// 需要知道它含有 compCounter / wireCounter 成员才能自增

/**
 * @brief 元件 / 连线 ID 生成器。
 *
 * 只负责自增计数器与格式化，不涉及具体电路逻辑。
 */
// 上方注释点明职责边界：这是纯粹的"发号器"，
// 不知道元件类型、不碰连线拓扑、不触发任何信号；
// 把 ID 生成独立出来，便于将来换成 UUID 或带命名空间前缀的方案

namespace IdAllocator {
// 用命名空间而非类：无状态工具函数集合，命名空间调用更自然（IdAllocator::nextComponentId）

QString nextComponentId(CircuitContext& ctx);
// 为指定上下文生成下一个元件 ID：
// 内部对 ctx.compCounter 自增，再格式化为 "C<n>"；
// 每个上下文独立计数，保证子电路内 ID 只在本上下文唯一

QString nextWireId(CircuitContext& ctx);
// 为指定上下文生成下一个连线 ID：
// 内部对 ctx.wireCounter 自增，再格式化为 "W<n>"；
// 与元件计数器分离，避免两类对象 ID 空间冲突

} // namespace IdAllocator
// 结束 IdAllocator 命名空间

#endif // IDALLOCATOR_H
// 结束头文件保护