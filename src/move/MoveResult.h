#ifndef MOVERESULT_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define MOVERESULT_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：additions/removals/replacements 中的连线都以 map 表示
#include <QList>
// 引入 QList：四个结果集合都是 QList
#include <QPair>
// 引入 QPair：replacements 元素是"旧线, 新线"的二元组
#include "ConnectionData.h"
// 引入 ConnectionData 完整定义：unsatisfied 以值语义存储它，需要完整布局

class MoveResult {
    // 一次连线重连计算的结果：描述"相比移动前，线网需要做哪些增删改"，
    // 由 Connector 生成、ConnectorThread 传递、Circuit/UI 消费；
    // 全部为 public 字段，因为它就是纯数据载体，不需要封装
public:
    // 五类结果

    QList<QVariantMap> additions;
    // 需要新增的连线：每项是完整 wire map（wireType="ortho"，含 path）；
    // A* 找到路径后由 processPath 逐段生成

    QList<QVariantMap> removals;
    // 需要整体删除的原连线：当某根原线完全被新路径替代时放入此项；
    // 删除时按 id 从上下文中移除

    QList<QPair<QVariantMap, QVariantMap>> replacements;
    // 需要替换的连线：first 为原线（按 id 定位），second 为缩短后的新线；
    // 用于"从原线中间引出新线"的场景——原线只保留远离新接入点的一段

    QList<ConnectionData> unsatisfied;
    // 未能成功重连的连接：A* 失败或超时导致；
    // UI 可据此提示用户，或回退本次移动

    int totalDistance = 0;
    // 所有成功连接路径的总距离：用于在多轮尝试中比较优劣——
    // 在 unsatisfied 数量相同时，优先选择总距离更短的方案（线更短更美观）
};

#endif
// 结束头文件保护