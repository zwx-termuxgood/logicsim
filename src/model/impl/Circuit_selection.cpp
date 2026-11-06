#include "model/Circuit.h"
// 引入本文件要实现的 Circuit 类完整声明：定义成员函数前必须看到类定义
#include "io/ClipboardCodec.h"
// 引入剪贴板编解码器：负责把选中元件/连线编码成可缓存的 QVariantList，以及把缓存粘贴进上下文

// 定义"复制选中元件及连线"接口：结果缓存到 m_clipboardCache 并返回，供 QML 显示/传递
QVariantList Circuit::copySelection(const QStringList& ids) {
    // 委托 ClipboardCodec::encode 把当前上下文中 ids 对应的元件与相关连线编码成 QVariantList，写入剪贴板缓存
    m_clipboardCache = ClipboardCodec::encode(currentCtx(), ids);
    // 返回缓存内容，便于调用方立即使用（如直接传给 pasteSelection）
    return m_clipboardCache;
}

// 定义"粘贴剪贴板内容"接口：参数 data 实际被忽略，使用内部缓存；返回新元件 ID 列表
QStringList Circuit::pasteSelection(const QVariantList& data, double dx, double dy) {
    // 参数 data 保留是为了 QML 调用签名稳定，实际以内部 m_clipboardCache 为准，显式标记未使用
    Q_UNUSED(data);
    // 准备返回的新元件 ID 列表
    QStringList newIds;
    // 只读浏览模式禁止粘贴，直接返回空列表表示失败
    if (m_viewOnly) return newIds;
    // 剪贴板缓存为空则无可粘贴内容，返回空列表
    if (m_clipboardCache.isEmpty()) return newIds;
    // 压撤销快照：整次粘贴作为一次可撤销操作（默认合并键）
    pushUndo();
    // 取当前上下文引用，粘贴目标写入这里
    Context& ctx = currentCtx();
    // 委托 ClipboardCodec::paste 把缓存内容按位移 (dx,dy) 插入上下文，返回粘贴结果（新 ID、新时钟状态等）
    ClipboardCodec::PasteResult r = ClipboardCodec::paste(ctx, m_clipboardCache, dx, dy);
    // 遍历粘贴产生的新时钟元件初始状态
    for (auto it = r.newClockStates.begin(); it != r.newClockStates.end(); ++it)
        // 逐个登记到 m_clockStates，保证时钟状态记录与元件一致
        m_clockStates[it.key()] = it.value();
    // 粘贴后全量求值，让新元件输出收敛、界面立即显示正确值
    evaluateAll();
    // 通知 QML 数据变化，需要重绘
    emit changed();
    // 通知结构变化（元件/连线集合变化），QML 需重建连线网络/端口索引
    emit structureChanged();
    // 返回新元件 ID 列表，供调用方继续操作（如选中粘贴出的元件）
    return r.newIds;
}