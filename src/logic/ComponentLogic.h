#ifndef COMPONENTLOGIC_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define COMPONENTLOGIC_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：所有查询函数的入参都是元件 map
#include <QVariantList>
// 引入 QVariantList：outputPorts/inputPorts 返回端口几何列表，outputSplits 参数也是列表
#include <QString>
// 引入 QString：类型名、位串、返回二进制值
#include <QVector>
// 引入 QVector：computeCombinationalOutputs 的输入输出都是 QVector<QString>，
// 比 QVariantList 更轻量，适合引擎内部高频调用

/**
 * @brief 元件类型的基础属性查询 + 端口几何计算 + 组合逻辑输出计算。
 *
 * 这是 logic 层唯一的"元件知识入口"。
 * 引擎、QML 端口面板、错误检查都通过这里获取元件信息。
 */
// 上方注释点明架构意图：
//   - 集中：所有"某类型有多少端口、端口在哪、逻辑怎么算"的知识只在这里；
//   - 复用：EventEngine 与 IterEngine 都调用 computeCombinationalOutputs，
//     保证两引擎对同一电路输出一致，不会出现切换引擎后行为不同；
//   - 无状态：全部是纯函数，便于单元测试，也避免引擎持有重复逻辑

namespace ComponentLogic {
// 用命名空间而非类：所有函数无状态，调用形式 ComponentLogic::xxx() 更直接

int inputCount(const QVariantMap& comp);
// 返回元件输入端口数量：
// 依类型不同而变——not/led/output/splitter 固定 1；
// and/or 等取 inputCount 字段（钳位 2..64）；hub 取分段数；
// sub 取 subInputNames 长度；tgate/ntran/ptran 固定 2；
// 数据源与 text 返回 0

int outputCount(const QVariantMap& comp);
// 返回元件输出端口数量：
// 数据源/门/sub? 视类型而定——input/clock/const/门 固定 1；
// splitter 取分段数；hub 固定 1；led/output/text 为 0；
// sub 取 subOutputNames 长度；晶体管固定 1

int outputBitWidth(const QVariantMap& comp, int portIdx);
// 返回某输出端口的位宽：
// 大多等于元件 bitWidth；
// splitter 取 outputSplits[portIdx]；
// sub 取 subOutputWidths[portIdx]；
// 越界回退 1

int inputBitWidth(const QVariantMap& comp, int portIdx);
// 返回某输入端口的位宽：
// 大多等于元件 bitWidth；
// hub 取 outputSplits[portIdx]（集线器各输入宽度对应分段）；
// sub 取 subInputWidths[portIdx]；
// 越界回退 1

QVariantList outputPorts(const QVariantMap& comp);
// 计算所有输出端口的几何位置：
// 返回列表每项含 index/x/y/bitWidth，坐标是相对元件中心的偏移；
// 不同元件有不同端口布局（门在右侧、splitter 在右侧多行、晶体管按类型偏移），
// 最后统一按 rotation 旋转偏移

QVariantList inputPorts(const QVariantMap& comp);
// 计算所有输入端口的几何位置：
// 规则与 outputPorts 对称（门在左侧、hub 在左侧多行、晶体管栅/漏极分列）；
// 同样按 rotation 旋转；
// 连线吸附、命中测试、错误标记都依赖此几何

// ============================================================
// 组合逻辑输出计算（EventEngine / IterEngine 共用）
// ============================================================
// 对"纯组合逻辑"类型统一计算输出：
//   not / and / or / nand / nor / xor / xnor
//   tgate / ntran / ptran
//   splitter / hub
// 其它类型（input/const/clock/output/led/sub）返回空 QVector，
// 由调用者按各自数据源处理。
//
// 参数：
//   type         — 元件类型
//   inputs       — 各输入端的二进制字符串（位宽由调用者保证与元件配置一致）
//   bitWidth     — 元件位宽
//   outputSplits — splitter / hub 的 outputSplits（其它类型传空即可）
//
// 返回：
//   与端口顺序匹配的输出字符串向量；未识别类型返回空。
QVector<QString> computeCombinationalOutputs(const QString& type,
                                             const QVector<QString>& inputs,
                                             int bitWidth,
                                             const QVariantList& outputSplits);
// 纯函数实现所有组合逻辑与三态语义：
//   - not：逐位取反，Z/E 映射为 E；
//   - and/or/nand/nor/xor/xnor：逐位规约，任一输入含 Z/E 则输出 E，nand/nor/xnor 最后取反；
//   - tgate/ntran/ptran：按使能位在 data 与 Z 间选择，Z/E 使能输出 E；
//   - splitter：把主输入按 outputSplits 从高位到低位切分成多段；
//   - hub：把多个输入按 outputSplits 拼接到主输出（splitter 逆操作）；
// 输入位宽不符时调用者已负责补零/截断，此函数按 bitWidth 安全读位

} // namespace ComponentLogic
// 结束 ComponentLogic 命名空间

#endif // COMPONENTLOGIC_H
// 结束头文件保护