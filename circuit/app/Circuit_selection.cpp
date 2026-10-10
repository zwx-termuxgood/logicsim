#include "../Circuit.h"
#include "../io/ClipboardCodec.h"

QVariantList Circuit::copySelection(const QStringList& ids) {
    return ClipboardCodec::encode(currentCtx(), ids);
}

QStringList Circuit::pasteSelection(const QVariantList& data, double dx, double dy) {
    QStringList newIds;
    if (m_viewOnly) return newIds;
    if (data.isEmpty()) return newIds;

    pushUndo();

    Context& ctx = currentCtx();
    ClipboardCodec::PasteResult r = ClipboardCodec::paste(ctx, data, dx, dy);

    for (auto it = r.newClockStates.begin(); it != r.newClockStates.end(); ++it)
        m_clockStates[it.key()] = it.value();

    evaluateAll();
    emit changed();
    return r.newIds;
}
