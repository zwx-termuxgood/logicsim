#ifndef SIMENGINE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define SIMENGINE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QString>
// 引入 QString：字符串互转函数的参数与返回类型

/**
 * @brief 引擎种类枚举 + 字符串互转。
 *
 * 只有两套引擎，用 enum 分支即可，不引入虚接口。
 */
// 上方注释解释设计决策：
//   - 用枚举而非多态接口：只有两种实现，虚函数与工厂模式的抽象成本不划算；
//   - Circuit 内部只需 if (engineType == "event") 分支，简单直接；
//   - 本文件作为"类型定义层"，避免各模块各自硬编码 "event"/"iter" 字符串

enum class EngineKind { Event, Iter };
// 强类型枚举：限定作用域（必须写 EngineKind::Event），
// 不与整数隐式互转，避免误传；两个值对应两套引擎

inline EngineKind engineKindFromString(const QString& s) {
    // 字符串 → 枚举：
    // 只有 "iter" 映射为 Iter，其余任何值（含空串、拼写错误）都回退 Event；
    // 这种"默认安全"策略保证配置文件被篡改时仍能用主引擎启动
    return s == "iter" ? EngineKind::Iter : EngineKind::Event;
}

inline QString engineKindToString(EngineKind k) {
    // 枚举 → 字符串：
    // 用 QStringLiteral 而非 QString("iter")，编译期生成静态字符串，避免运行时分配；
    // 与 engineKindFromString 严格互逆，保证存盘再读取后引擎不漂移
    return k == EngineKind::Iter ? QStringLiteral("iter") : QStringLiteral("event");
}

#endif // SIMENGINE_H
// 结束头文件保护