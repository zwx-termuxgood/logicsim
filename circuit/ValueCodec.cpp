#include "ValueCodec.h"
#include <QStringList>
#include <cstring>

namespace ValueCodec {

QString bitsToString(const QVariantList& bits, int bw) {
    QString s;
    s.reserve(bw);
    for (int i = 0; i < bw; ++i)
        s += (i < bits.size() && bits[i].toBool()) ? '1' : '0';
    return s;
}

bool bitAt(const QString& s, int idx) {
    if (idx < 0 || idx >= s.length()) return false;
    return s[idx] == '1';
}

QString format(const QString& binStr, const QString& base, int bw) {
    if (bw < 1) bw = 1;
    if (bw > 64) bw = 64;
    QString s = binStr;
    if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
    if (s.length() > bw) s = s.right(bw);
    QString b = base;
    if (b.isEmpty()) b = "bin";
    if (b == "bin") return s;
    if (b == "hex") {
        int digits = (bw + 3) / 4;
        QString out;
        for (int d = digits - 1; d >= 0; --d) {
            int v = 0;
            for (int bit = 0; bit < 4; ++bit) {
                int pos = d * 4 + bit;
                int idx = bw - 1 - pos;
                if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1 << bit);
            }
            out += (v < 10) ? QChar('0' + v) : QChar('A' + v - 10);
        }
        return out;
    }
    if (b == "udec" || b == "sdec") {
        quint64 v = 0;
        for (int i = 0; i < bw && i < 64; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1ULL << i);
        }
        if (b == "udec") return QString::number(v);
        if (bw < 64) {
            qint64 sv = (qint64)v;
            qint64 sign = 1LL << (bw - 1);
            if (sv & sign) sv -= (1LL << bw);
            return QString::number(sv);
        }
        return QString::number((qint64)v);
    }
    if (b == "f32") {
        quint32 uv = 0;
        for (int i = 0; i < 32; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1u << i);
        }
        float f; std::memcpy(&f, &uv, 4);
        return QString::number((double)f, 'g', 9);
    }
    if (b == "f64") {
        quint64 uv = 0;
        for (int i = 0; i < 64; ++i) {
            int idx = bw - 1 - i;
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1ULL << i);
        }
        double d; std::memcpy(&d, &uv, 8);
        return QString::number(d, 'g', 17);
    }
    return s;
}

QVariantList parse(const QString& text, const QString& base, int bw, QString* err) {
    QVariantList bits;
    for (int i = 0; i < bw; ++i) bits.append(false);
    QString s = text.trimmed();
    if (s.isEmpty()) return bits;
    if (base == "bin") {
        for (auto ch : s) if (ch != '0' && ch != '1') {
                if (err) *err = "二进制只能是 0/1"; return bits;
            }
        if (s.length() > bw) {
            if (err) *err = QString("最多 %1 位").arg(bw); return bits;
        }
        int offset = bw - s.length();
        for (int i = 0; i < s.length(); ++i)
            bits[offset + i] = (s[i] == '1');
        return bits;
    }
    if (base == "hex") {
        QString up = s.toUpper();
        for (auto ch : up) if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) {
                if (err) *err = "非法十六进制"; return bits;
            }
        int maxDigits = (bw + 3) / 4;
        if (up.length() > maxDigits) {
            if (err) *err = QString("最多 %1 位十六进制").arg(maxDigits); return bits;
        }
        for (int i = up.length() - 1; i >= 0; --i) {
            int digitFromRight = (up.length() - 1 - i);
            QChar ch = up[i];
            int v = (ch >= '0' && ch <= '9') ? (ch.unicode() - '0') : (ch.unicode() - 'A' + 10);
            for (int bit = 0; bit < 4; ++bit) {
                int pos = digitFromRight * 4 + bit;
                int idx = bw - 1 - pos;
                if (idx >= 0 && idx < bw)
                    bits[idx] = (v & (1 << bit)) != 0;
            }
        }
        return bits;
    }
    if (base == "udec") {
        bool ok = false;
        qint64 v = s.toLongLong(&ok);
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        if (v < 0) { if (err) *err = "无符号不能为负"; return bits; }
        if (bw < 63) {
            qint64 maxV = (1LL << bw) - 1;
            if (v > maxV) {
                if (err) *err = QString("超出范围 [0, %1]").arg(maxV); return bits;
            }
        }
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((v >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "sdec") {
        bool ok = false;
        qint64 v = s.toLongLong(&ok);
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        if (bw < 63) {
            qint64 minV = -(1LL << (bw - 1));
            qint64 maxV = (1LL << (bw - 1)) - 1;
            if (v < minV || v > maxV) {
                if (err) *err = QString("超出范围 [%1, %2]").arg(minV).arg(maxV);
                return bits;
            }
        }
        quint64 uv = (quint64)v;
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "f32") {
        if (bw < 32) { if (err) *err = "需要至少 32 位"; return bits; }
        bool ok = false; float f = s.toFloat(&ok);
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        quint32 uv; std::memcpy(&uv, &f, 4);
        for (int i = 0; i < 32; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (base == "f64") {
        if (bw < 64) { if (err) *err = "需要至少 64 位"; return bits; }
        bool ok = false; double d = s.toDouble(&ok);
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        quint64 uv; std::memcpy(&uv, &d, 8);
        for (int i = 0; i < 64; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    if (err) *err = "未知进制";
    return bits;
}

} // namespace ValueCodec