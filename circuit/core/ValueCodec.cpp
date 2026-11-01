#include "ValueCodec.h"
// 引入 ValueCodec 命名空间声明：实现其编解码函数前必须看到函数原型
#include <QStringList>
// 引入 QStringList：本文件按需使用字符串列表工具
#include <cstring>
// 引入 C 字符串/内存库：std::memcpy 用于 float/double 与其位模式的互相转换

namespace ValueCodec {
// 打开 ValueCodec 命名空间，与头文件声明保持一致

// 定义"位数组转二进制串"接口
QString bitsToString(const QVariantList& bits, int bw) {
    // 准备结果串
    QString s;
    // 预分配位宽容量，避免循环中反复扩容
    s.reserve(bw);
    // 逐位生成，长度固定为 bw
    for (int i = 0; i < bw; ++i)
        // 索引在范围内且该位为 true 则为 '1'，否则补 '0'
        s += (i < bits.size() && bits[i].toBool()) ? '1' : '0';
    // 返回定长二进制串
    return s;
}

// 定义"安全读取二进制串某一位"接口
bool bitAt(const QString& s, int idx) {
    // 索引越界返回 false，避免崩溃
    if (idx < 0 || idx >= s.length()) return false;
    // 该字符为 '1' 则返回 true
    return s[idx] == '1';
}

// 定义"二进制串按指定进制格式化"接口
QString format(const QString& binStr, const QString& base, int bw) {
    // 位宽下限钳到 1
    if (bw < 1) bw = 1;
    // 位宽上限钳到 64
    if (bw > 64) bw = 64;
    // 复制输入串，便于补齐/截断
    QString s = binStr;
    // 长度不足位宽：左侧补 '0'
    if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
    // 长度超出位宽：只保留低位
    if (s.length() > bw) s = s.right(bw);
    // 取进制，为空回退 "bin"
    QString b = base;
    if (b.isEmpty()) b = "bin";
    // 二进制直接返回补齐后的串
    if (b == "bin") return s;
    // 十六进制：每 4 位一组转成一个 hex 字符
    if (b == "hex") {
        // 需要的 hex 位数（向上取整）
        int digits = (bw + 3) / 4;
        // 准备结果串
        QString out;
        // 从最高位数字开始逐位输出
        for (int d = digits - 1; d >= 0; --d) {
            // 该位十六进制数字的值
            int v = 0;
            // 组装 4 个二进制位
            for (int bit = 0; bit < 4; ++bit) {
                // 该 bit 在串中的位置（从低位起算）
                int pos = d * 4 + bit;
                // 映射到字符串中的索引
                int idx = bw - 1 - pos;
                // 在范围内且为 '1' 则置对应位
                if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1 << bit);
            }
            // 数值转字符：0-9 用 '0'-'9'，10-15 用 'A'-'F'
            out += (v < 10) ? QChar('0' + v) : QChar('A' + v - 10);
        }
        // 返回十六进制串
        return out;
    }
    // 无符号/有符号十进制
    if (b == "udec" || b == "sdec") {
        // 先按无符号解析二进制值为 quint64
        quint64 v = 0;
        // 逐位组装
        for (int i = 0; i < bw && i < 64; ++i) {
            // 该 bit 对应的字符串索引
            int idx = bw - 1 - i;
            // 为 '1' 则置对应位
            if (idx >= 0 && idx < bw && s[idx] == '1') v |= (1ULL << i);
        }
        // 无符号直接输出
        if (b == "udec") return QString::number(v);
        // 有符号：位宽小于 64 时按最高位判断符号并做符号扩展
        if (bw < 64) {
            // 转成有符号初值
            qint64 sv = (qint64)v;
            // 符号位掩码
            qint64 sign = 1LL << (bw - 1);
            // 符号位为 1 则减去 2^bw 得到负数真值
            if (sv & sign) sv -= (1LL << bw);
            // 输出有符号十进制
            return QString::number(sv);
        }
        // 位宽 64：直接按有符号 64 位输出
        return QString::number((qint64)v);
    }
    // 32 位浮点
    if (b == "f32") {
        // 组装 32 位无符号位模式
        quint32 uv = 0;
        // 逐位组装（最多 32 位）
        for (int i = 0; i < 32; ++i) {
            // 该 bit 对应的字符串索引
            int idx = bw - 1 - i;
            // 为 '1' 则置对应位
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1u << i);
        }
        // 用 memcpy 把位模式解释为 float（避免类型双关 UB）
        float f; std::memcpy(&f, &uv, 4);
        // 以 9 位有效数字输出，兼顾精度与可读性
        return QString::number((double)f, 'g', 9);
    }
    // 64 位浮点
    if (b == "f64") {
        // 组装 64 位无符号位模式
        quint64 uv = 0;
        // 逐位组装
        for (int i = 0; i < 64; ++i) {
            // 该 bit 对应的字符串索引
            int idx = bw - 1 - i;
            // 为 '1' 则置对应位
            if (idx >= 0 && idx < bw && s[idx] == '1') uv |= (1ULL << i);
        }
        // 用 memcpy 把位模式解释为 double
        double d; std::memcpy(&d, &uv, 8);
        // 以 17 位有效数字输出，保证 double 往返无损
        return QString::number(d, 'g', 17);
    }
    // 未知进制：原样返回二进制串
    return s;
}

// 定义"文本按指定进制解析为位数组"接口，出错时写入 err
QVariantList parse(const QString& text, const QString& base, int bw, QString* err) {
    // 准备默认全 0 的位数组
    QVariantList bits;
    for (int i = 0; i < bw; ++i) bits.append(false);
    // 去空白后的输入文本
    QString s = text.trimmed();
    // 空输入视为全 0
    if (s.isEmpty()) return bits;
    // 二进制解析
    if (base == "bin") {
        // 逐字符校验只能是 0/1
        for (auto ch : s) if (ch != '0' && ch != '1') {
                // 非法字符：写错误消息并返回全 0
                if (err) *err = "二进制只能是 0/1"; return bits;
            }
        // 超出位宽报错
        if (s.length() > bw) {
            if (err) *err = QString("最多 %1 位").arg(bw); return bits;
        }
        // 低位对齐：右侧输入对应低位
        int offset = bw - s.length();
        // 逐位写入
        for (int i = 0; i < s.length(); ++i)
            bits[offset + i] = (s[i] == '1');
        return bits;
    }
    // 十六进制解析
    if (base == "hex") {
        // 统一转大写便于判断
        QString up = s.toUpper();
        // 逐字符校验只能是 0-9/A-F
        for (auto ch : up) if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) {
                // 非法字符：写错误消息并返回全 0
                if (err) *err = "非法十六进制"; return bits;
            }
        // 位宽允许的最大十六进制位数
        int maxDigits = (bw + 3) / 4;
        // 超出位数报错
        if (up.length() > maxDigits) {
            if (err) *err = QString("最多 %1 位十六进制").arg(maxDigits); return bits;
        }
        // 从最低位数字开始逐位写入
        for (int i = up.length() - 1; i >= 0; --i) {
            // 该数字从右往左的序号
            int digitFromRight = (up.length() - 1 - i);
            // 当前字符
            QChar ch = up[i];
            // 字符转数值：0-9 与 A-F
            int v = (ch >= '0' && ch <= '9') ? (ch.unicode() - '0') : (ch.unicode() - 'A' + 10);
            // 每个 hex 数字展开成 4 个二进制位
            for (int bit = 0; bit < 4; ++bit) {
                // 该 bit 从低位起的位置
                int pos = digitFromRight * 4 + bit;
                // 映射到字符串中的索引
                int idx = bw - 1 - pos;
                // 在范围内则写入对应位
                if (idx >= 0 && idx < bw)
                    bits[idx] = (v & (1 << bit)) != 0;
            }
        }
        return bits;
    }
    // 无符号十进制解析
    if (base == "udec") {
        // 转换结果标记
        bool ok = false;
        // 解析成 64 位整数
        qint64 v = s.toLongLong(&ok);
        // 解析失败：写错误消息
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        // 无符号不允许负数
        if (v < 0) { if (err) *err = "无符号不能为负"; return bits; }
        // 位宽小于 63 时校验上限
        if (bw < 63) {
            // 该位宽能表示的最大无符号值
            qint64 maxV = (1LL << bw) - 1;
            // 超出范围报错
            if (v > maxV) {
                if (err) *err = QString("超出范围 [0, %1]").arg(maxV); return bits;
            }
        }
        // 逐位写入（低位在前对应右侧）
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((v >> i) & 1) != 0;
        }
        return bits;
    }
    // 有符号十进制解析
    if (base == "sdec") {
        // 转换结果标记
        bool ok = false;
        // 解析成 64 位整数
        qint64 v = s.toLongLong(&ok);
        // 解析失败：写错误消息
        if (!ok) { if (err) *err = "无效整数"; return bits; }
        // 位宽小于 63 时校验有符号范围
        if (bw < 63) {
            // 该位宽能表示的最小值
            qint64 minV = -(1LL << (bw - 1));
            // 该位宽能表示的最大值
            qint64 maxV = (1LL << (bw - 1)) - 1;
            // 超出范围报错
            if (v < minV || v > maxV) {
                if (err) *err = QString("超出范围 [%1, %2]").arg(minV).arg(maxV);
                return bits;
            }
        }
        // 转成无符号位模式
        quint64 uv = (quint64)v;
        // 逐位写入
        for (int i = 0; i < bw; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    // 32 位浮点解析
    if (base == "f32") {
        // 位宽不足 32 无法容纳 float
        if (bw < 32) { if (err) *err = "需要至少 32 位"; return bits; }
        // 转换结果标记
        bool ok = false; float f = s.toFloat(&ok);
        // 解析失败：写错误消息
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        // float 转位模式（memcpy 避免 UB）
        quint32 uv; std::memcpy(&uv, &f, 4);
        // 逐位写入（从低位起）
        for (int i = 0; i < 32; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    // 64 位浮点解析
    if (base == "f64") {
        // 位宽不足 64 无法容纳 double
        if (bw < 64) { if (err) *err = "需要至少 64 位"; return bits; }
        // 转换结果标记
        bool ok = false; double d = s.toDouble(&ok);
        // 解析失败：写错误消息
        if (!ok) { if (err) *err = "无效浮点"; return bits; }
        // double 转位模式
        quint64 uv; std::memcpy(&uv, &d, 8);
        // 逐位写入
        for (int i = 0; i < 64; ++i) {
            int idx = bw - 1 - i;
            bits[idx] = ((uv >> i) & 1) != 0;
        }
        return bits;
    }
    // 未知进制：写错误消息
    if (err) *err = "未知进制";
    // 返回全 0 位数组
    return bits;
}

} // namespace ValueCodec
// 关闭 ValueCodec 命名空间