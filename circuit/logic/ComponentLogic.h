#ifndef COMPONENTLOGIC_H
#define COMPONENTLOGIC_H

#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <QVector>

/**
 * @brief 元件类型的基础属性查询 + 端口几何计算 + 组合逻辑输出计算。
 *
 * 这是 logic 层唯一的"元件知识入口"。
 * 引擎、QML 端口面板、错误检查都通过这里获取元件信息。
 */
namespace ComponentLogic {

int inputCount(const QVariantMap& comp);
int outputCount(const QVariantMap& comp);
int outputBitWidth(const QVariantMap& comp, int portIdx);
int inputBitWidth(const QVariantMap& comp, int portIdx);

QVariantList outputPorts(const QVariantMap& comp);
QVariantList inputPorts(const QVariantMap& comp);

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

} // namespace ComponentLogic

#endif // COMPONENTLOGIC_H