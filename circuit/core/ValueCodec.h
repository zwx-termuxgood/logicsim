#ifndef VALUECODEC_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define VALUECODEC_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QString>
// 引入 QString：几乎所有函数的输入/输出都是 QString（二进制串、进制名、格式化结果）
#include <QVariantList>
// 引入 QVariantList：bits 参数与 parse 的返回类型都是 QVariantList（每个元素是 bool 的位）

/**
 * @brief 二进制串与各种进制之间的编解码。
 */
// 上方注释点明职责：本命名空间只做"值表示层"的转换，
// 不认识元件、不碰电路拓扑，因此被 Query 层与 UI 层共同复用

namespace ValueCodec {
// 用命名空间而非类：全部是无状态纯函数，便于按 ValueCodec::format(...) 调用

QString bitsToString(const QVariantList& bits, int bw);
// 把 QVariantList 形式的位数组转成 "0101..." 二进制串：
// 输入 bits 每个元素是 bool；长度可能小于 bw（前面补 '0'）或大于 bw（截取）；
// 统一成定长字符串，方便后续按位运算与显示

bool bitAt(const QString& s, int idx);
// 安全读取二进制串第 idx 位：
// 越界返回 false 而不是崩溃，让调用者无需自己写边界判断；
// 返回 bool 而非 QChar，因为调用方通常直接用于逻辑运算

QString format(const QString& binStr, const QString& base, int bw);
// 把二进制串按指定进制格式化为可读文本：
// base 支持 "bin"/"hex"/"udec"/"sdec"/"f32"/"f64"；
// bw 用于定长补零与符号位判断（sdec 需要知道位宽才能识别负数）；
// 返回字符串供 QML 输入框/标签直接显示

QVariantList parse(const QString& text, const QString& base, int bw, QString* err);
// 把可读文本解析回二进制位数组：
// 与 format 互逆；bw 决定补位/截断与取值范围校验；
// err 为输出参数——解析成功时保持空，失败时写入中文错误消息；
// 用 QString* 而不是返回值传错误，是为了让返回值只承载数据、便于链式使用

} // namespace ValueCodec
// 结束 ValueCodec 命名空间

#endif // VALUECODEC_H
// 结束头文件保护