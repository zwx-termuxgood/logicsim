#ifndef CLIPBOARDCODEC_H
#define CLIPBOARDCODEC_H

#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QString>
#include <QHash>
#include "../core/CircuitContext.h"

/**
 * @brief 复制 / 粘贴的编解码。
 *
 * encode：从 ctx 中抽取指定 id 集合的元件及其内部连线。
 * paste：把 data 中的元件与连线追加到 ctx，返回新 id 与新增 clock 状态。
 */
namespace ClipboardCodec {

QVariantList encode(const CircuitContext& ctx, const QStringList& ids);

struct PasteResult {
    QStringList newIds;
    QHash<QString, bool> newClockStates;   // 新增 clock 元件的初始状态
};

PasteResult paste(CircuitContext& ctx,
                  const QVariantList& data,
                  double dx, double dy);

} // namespace ClipboardCodec

#endif // CLIPBOARDCODEC_H
