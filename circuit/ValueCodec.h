#ifndef VALUECODEC_H
#define VALUECODEC_H

#include <QString>
#include <QVariantList>

/**
 * @brief 二进制串与各种进制之间的编解码。
 *
 * 支持的进制：bin / hex / udec / sdec / f32 / f64
 */
namespace ValueCodec {

// 位列表 → 二进制字符串
QString bitsToString(const QVariantList& bits, int bw);

// 二进制串单字符查询
bool bitAt(const QString& s, int idx);

// 二进制字符串按指定进制格式化
QString format(const QString& binStr, const QString& base, int bw);

// 文本按指定进制解析为位列表；出错时 *err 非空
QVariantList parse(const QString& text, const QString& base, int bw, QString* err);

} // namespace ValueCodec

#endif // VALUECODEC_H