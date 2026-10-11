#include "../Circuit.h"
#include "../io/ClipboardCodec.h"

QVariantList Circuit::copySelection(const QStringList& ids) {
    m_clipboardCache = ClipboardCodec::encode(currentCtx(), ids);
    return m_clipboardCache;
}

QStringList Circuit::pasteSelection(const QVariantList& data, double dx, double dy) {
    Q_UNUSED(data);

    QStringList newIds;
    if (m_viewOnly) return newIds;
    if (m_clipboardCache.isEmpty()) return newIds;

    pushUndo();

    Context& ctx = currentCtx();
    ClipboardCodec::PasteResult r = ClipboardCodec::paste(ctx, m_clipboardCache, dx, dy);

    for (auto it = r.newClockStates.begin(); it != r.newClockStates.end(); ++it)
        m_clockStates[it.key()] = it.value();

    evaluateAll();
    emit changed();
    // ★ 关键修复：QML 需要重建端口缓存，否则新元件的引脚/连线在画布上不可见
    emit structureChanged();
    return r.newIds;
}
