#ifndef VALUECODEC_H
#define VALUECODEC_H

#include <QString>
#include <QVariantList>

/**
 * @brief 二进制串与各种进制之间的编解码。
 */
namespace ValueCodec {

QString bitsToString(const QVariantList& bits, int bw);
bool bitAt(const QString& s, int idx);
QString format(const QString& binStr, const QString& base, int bw);
QVariantList parse(const QString& text, const QString& base, int bw, QString* err);

} // namespace ValueCodec

#endif // VALUECODEC_H