#include "../Circuit.h"
// 引入 Circuit 类定义：本文件实现 Circuit 的四个静态成员函数，
// 必须看到类声明才能定义成员
#include "../logic/ComponentLogic.h"
// 引入 ComponentLogic 命名空间：四个转发函数都调用它，
// 需要完整声明以解析函数重载与返回类型

int Circuit::componentInputCount(const QVariantMap& comp) {
    // 静态转发：把"某元件有多少输入端口"的查询委托给 ComponentLogic
    // 之所以不直接在 Circuit 内实现，是因为引擎、错误检查等也需要同一逻辑，
    // 集中到 ComponentLogic 保证唯一事实来源
    return ComponentLogic::inputCount(comp);
    // 直接返回 ComponentLogic 的结果：类型、钳位、按类型分支都在那边处理
}

int Circuit::componentOutputCount(const QVariantMap& comp) {
    // 静态转发：查询"某元件有多少输出端口"
    // Circuit 保留这层薄封装是为了让 cpp 内部（如错误检查、端口面板）
    // 能以 Circuit 的私有静态接口调用，不把 ComponentLogic 暴露给所有调用点
    return ComponentLogic::outputCount(comp);
    // 转调 ComponentLogic::outputCount，逻辑不在此重复
}

int Circuit::outputPortBitWidth(const QVariantMap& comp, int portIdx) {
    // 静态转发：查询"某元件某输出端口的位宽"
    // 端口位宽因类型而异（splitter 按分段、sub 按子电路定义），
    // 这些知识全在 ComponentLogic，Circuit 只负责转发
    return ComponentLogic::outputBitWidth(comp, portIdx);
    // 传入端口索引，由 ComponentLogic 做越界回退与类型分支
}

int Circuit::inputPortBitWidth(const QVariantMap& comp, int portIdx) {
    // 静态转发：查询"某元件某输入端口的位宽"
    // 与 outputPortBitWidth 对称；错误检查中比较源输出宽度与目标输入宽度时使用
    return ComponentLogic::inputBitWidth(comp, portIdx);
    // 转调 ComponentLogic::inputBitWidth
}