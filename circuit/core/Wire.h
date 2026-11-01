#ifndef WIRE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define WIRE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：所有存取器的入参都是连线 map（字段名 → QVariant）
#include <QString>
// 引入 QString：ID、fromComp、toComp 及 connects 的参数都需要它

/**
 * @brief 连线（QVariantMap）的通用存取器。
 */
// 上方注释点明职责：连线在内存中就是 QVariantMap，这里把字段名集中成一组 inline 函数，
// 避免各处硬编码 "fromPort" 等字符串导致拼写不一致；与 Component.h 对称设计

namespace Wire {
// 用命名空间而非类：全部是无状态纯函数，调用形式 Wire::id(w) 更自然

inline QString id(const QVariantMap& w)       { return w.value("id").toString(); }
// 读取连线唯一 ID（如 W1）；inline 省去跨 TU 调用开销

inline QString fromComp(const QVariantMap& w) { return w.value("fromComp").toString(); }
// 读取源端元件 ID；ortho 线的 T 型引出端可能没有源元件，此时返回空串，调用方需容忍空值

inline QString toComp(const QVariantMap& w)   { return w.value("toComp").toString(); }
// 读取目标端元件 ID；同理可能为空（只连到另一根线的悬空端）

inline int fromPort(const QVariantMap& w)     { return w.value("fromPort").toInt(); }
// 读取源端输出端口索引；缺失时 toInt() 返回 0，
// 由于调用方通常先确认 fromComp 非空，默认 0 不会造成误判

inline int toPort(const QVariantMap& w)       { return w.value("toPort").toInt(); }
// 读取目标端输入端口索引

inline bool connects(const QVariantMap& w, const QString& compId) {
    // 判断连线是否与某元件相连：源端或目标端任一匹配即可；
    // 封装成函数避免调用方重复写 || 判断，也便于将来扩展（如中间穿过的元件）
    return fromComp(w) == compId || toComp(w) == compId;
}

} // namespace Wire
// 结束 Wire 命名空间

#endif // WIRE_H
// 结束头文件保护