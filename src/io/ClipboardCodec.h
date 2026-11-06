#ifndef CLIPBOARDCODEC_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define CLIPBOARDCODEC_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantList>
// 引入 QVariantList：encode 的返回类型与 paste 的 data 参数都是 QVariantList
#include <QVariantMap>
// 引入 QVariantMap：PasteResult 内部与 encode 包装结构使用（虽然本头文件未直接出现，但实现侧依赖）
#include <QStringList>
// 引入 QStringList：encode 的 ids 参数与 PasteResult::newIds 都是 QStringList
#include <QString>
// 引入 QString：QHash 的 key 类型以及注释中的 ID 概念
#include <QHash>
// 引入 QHash：PasteResult::newClockStates 是 QHash<QString, bool>
#include "core/CircuitContext.h"
// 引入 CircuitContext 完整定义：encode 以 const 引用接收，paste 以引用修改它的 components/wires

/**
 * @brief 复制 / 粘贴的编解码。
 *
 * encode：从 ctx 中抽取指定 id 集合的元件及其内部连线。
 * paste：把 data 中的元件与连线追加到 ctx，返回新 id 与新增 clock 状态。
 */
// 上方注释点明职责：本模块只处理"选择集 ↔ 可序列化数据"的转换与追加，
// 不负责撤销、不触发信号、不判断视图模式，这些交给 Circuit_selection.cpp 编排

namespace ClipboardCodec {
// 用命名空间而非类：encode/paste 是无状态函数，
// 仅 PasteResult 是一个纯数据返回结构

QVariantList encode(const CircuitContext& ctx, const QStringList& ids);
// 从上下文中抽取被选中的元件及其"相关连线"，打包成 QVariantList：
// 返回的列表只含一个元素（一个 QVariantMap），内含 components 与 wires 两个键；
// 之所以包一层列表而不是直接返回 map，是为了与 QML 剪贴板/系统剪贴板的通用列表格式对齐
// 相关连线的判定在实现中分两类：
//   1) 只选线：仅复制明确选中的线
//   2) 选了元件：先复制直接相连的线，再迭代扩散端点重合/T 型接入的线

struct PasteResult {
    // 粘贴操作的返回结果：调用者需要知道新生成的 ID 与时钟初始状态

    QStringList newIds;
    // 新生成的元件 ID 列表：按粘贴顺序排列，
    // 供 Circuit 侧选中新元件或后续操作使用

    QHash<QString, bool> newClockStates;   // 新增 clock 元件的初始状态
    // 新增 clock 元件的初始状态映射：
    // 粘贴时必须同步更新 Circuit::m_clockStates，否则新时钟没有状态项，
    // 求值时会被当作默认 false，也可能在时钟翻转时漏掉
};

PasteResult paste(CircuitContext& ctx,
                  const QVariantList& data,
                  double dx, double dy);
// 把 encode 产生的数据追加到目标上下文：
//   - 为每个元件分配新 ID（IdAllocator），建立旧→新 ID 映射
//   - 元件坐标整体平移 (dx, dy)
//   - 连线按映射重写 fromComp/toComp；ortho 线的 path 顶点同样平移
//   - ortho 线允许一端映射失败（自然悬空），bezier 线要求两端都成功
// 返回新 ID 与新增时钟状态；不修改撤销栈、不发信号，由调用者统一处理

} // namespace ClipboardCodec
// 结束 ClipboardCodec 命名空间

#endif // CLIPBOARDCODEC_H
// 结束头文件保护