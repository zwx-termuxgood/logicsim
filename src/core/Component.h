#ifndef COMPONENT_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define COMPONENT_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：所有存取器的入参都是元件 map（字段名 → QVariant）
#include <QString>
// 引入 QString：返回 ID/类型/名称、拼接 qualifiedId 都需要它

namespace Component {
// 用命名空间而非类：这里全是无状态的纯函数，不需要对象实例，
// 命名空间可避免污染全局，也便于按 Component::id(c) 的形式调用

// ---- 字段读取 ----
// 以下一组函数约定元件 QVariantMap 的字段名与取值方式，
// 让其它模块不必到处写 c.value("xxx").toYyy()，字段名只在此处出现一次

inline QString id(const QVariantMap& c)     { return c.value("id").toString(); }
// 读取元件唯一 ID（如 C1）；inline 避免频繁调用的函数开销

inline QString type(const QVariantMap& c)   { return c.value("type").toString(); }
// 读取元件类型字符串（"and"/"input"/"sub" 等），后续所有类型判定都基于它

inline QString name(const QVariantMap& c)   { return c.value("name").toString(); }
// 读取用户可见名称；缺省为空串，由界面决定回退显示 ID 还是类型名

inline QString subId(const QVariantMap& c)  { return c.value("subId").toString(); }
// 读取 sub 元件引用的子电路 ID；非 sub 类型返回空串，调用者需先判断类型

inline QString displayBase(const QVariantMap& c) {
    // 读取显示进制；字段缺失或为空时回退 "bin"，保证 UI 永远拿到合法值
    QString b = c.value("displayBase").toString();
    return b.isEmpty() ? QStringLiteral("bin") : b;
}

inline int bitWidth(const QVariantMap& c) {
    // 读取位宽并钳位到 [1, 64]：
    // 下限 1 防止零/负位宽导致空字符串；
    // 上限 64 对应 quint64 与 ValueCodec 的编解码上限
    int b = c.value("bitWidth").toInt();
    return b < 1 ? 1 : (b > 64 ? 64 : b);
}

inline int inputCount(const QVariantMap& c) { return c.value("inputCount").toInt(); }
// 读取输入端口数量（仅 and/or/nand/nor/xor/xnor 等多输入门使用）；
// 此处不钳位，钳位逻辑在 ComponentLogic::inputCount 中按类型分别处理

inline int rotation(const QVariantMap& c)   { return c.value("rotation").toInt(); }
// 读取旋转角（0..3，代表 0/90/180/270 度）；取模归一化在写入端完成

inline double x(const QVariantMap& c)       { return c.value("x").toDouble(); }
// 读取元件中心 X 坐标；用 double 以支持无级缩放与半像素对齐

inline double y(const QVariantMap& c)       { return c.value("y").toDouble(); }
// 读取元件中心 Y 坐标

// ---- 类型判定 ----
// 每个判定函数只做字符串比较，集中在此便于新增类型时统一修改，
// 也避免各处硬编码类型名导致拼写错误

inline bool isInput(const QVariantMap& c)  { QString t = type(c); return t == "input" || t == "const"; }
// 判断是否为"输入源"类元件：input（可交互翻转）与 const（常量）语义相近，
// 都被视为数据源，故合并判定

inline bool isEditable(const QVariantMap& c) {
    // 判断是否可被用户编辑值：目前只有 input/const 可写 inputBits；
    // 单独命名以便将来扩展（例如可编辑的 RAM）时只改这里
    QString t = type(c);
    return t == "input" || t == "const";
}

inline bool isClock(const QVariantMap& c)  { return type(c) == "clock"; }
// 判断是否为时钟源：时钟有独立的 m_clockStates 与定时器驱动

inline bool isOutput(const QVariantMap& c) { return type(c) == "output"; }
// 判断是否为输出探针：output 是子电路的对外出口，也是顶层显示用

inline bool isLed(const QVariantMap& c)    { return type(c) == "led"; }
// 判断是否为 LED：仅显示用，无输出端口

inline bool isSub(const QVariantMap& c)    { return type(c) == "sub"; }
// 判断是否为子电路实例：其端口由子电路内 input/output 元件动态决定

inline bool isText(const QVariantMap& c)   { return type(c) == "text"; }
// 判断是否为纯文本标注：无端口、不参与求值，多处逻辑需跳过

inline bool isSplitter(const QVariantMap& c) { return type(c) == "splitter"; }
// 判断是否为分线器：1 输入 → N 输出，输出位宽由 outputSplits 决定

inline bool isHub(const QVariantMap& c)    { return type(c) == "hub"; }
// 判断是否为集线器：N 输入 → 1 输出，是 splitter 的逆操作

inline bool isGate(const QVariantMap& c) {
    // 判断是否为基本逻辑门：and/or/nand/nor/xor/xnor/not；
    // 用于统计、批量操作与"是否需要门延迟"判断
    QString t = type(c);
    return t == "and" || t == "or" || t == "nand" ||
           t == "nor" || t == "xor" || t == "xnor" || t == "not";
}

inline bool isTransistor(const QVariantMap& c) {
    // 判断是否为晶体管类元件：tgate（传输门）/ntran（NMOS）/ptran（PMOS）；
    // 它们有特殊的三态输出语义（Z），与普通门不同
    QString t = type(c);
    return t == "tgate" || t == "ntran" || t == "ptran";
}

// ---- inline：不再需要 Component.cpp ----
// 注释说明设计决策：这些函数足够简单，全部 inline 放在头文件，
// 避免为几个 getter 单独建 Component.cpp 与链接开销

inline QString qualifiedId(const QString& prefix, const QString& localId) {
    // 拼接"限定 ID"：子电路实例内的元件在引擎中以 "实例路径/本地ID" 表示；
    // prefix 为空表示根上下文，直接返回本地 ID，不加多余分隔符
    return prefix.isEmpty() ? localId : (prefix + "/" + localId);
}

} // namespace Component
// 结束 Component 命名空间

#endif // COMPONENT_H
// 结束头文件保护