#ifndef CONNECTOR_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define CONNECTOR_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include "MoveRequest.h"
// 引入 MoveRequest 完整定义：computeWires 以 const 引用接收它，
// 且实现中需要访问 gesture()/dx()/dy() 成员，必须知道类布局
#include "MoveResult.h"
// 引入 MoveResult 完整定义：函数返回 MoveResult*，
// 调用方与实现都需要完整类型来构造/读取 additions/removals/replacements

class Connector {
    // 连线重连计算器：给定一次移动请求（哪些元件移动了多少），
    // 重新规划被移动元件与外部网络之间的 ortho 连线；
    // 纯静态入口，无可观察状态——一次请求的计算过程完全由输入决定

public:
    // 唯一公有接口

    static MoveResult* computeWires(const MoveRequest& req);
    // 计算连线重连方案：
    //   1) 从 MoveGesture 取连接描述（端口位置、方向、已有 wire 链）；
    //   2) 用固定避让地图剔除不可能达成的连接；
    //   3) 为每个连接生成 A* 起点（端口位置 + 沿 wire 链的候选点）；
    //   4) 多次尝试不同连接顺序（排序/反序/随机），每次在克隆地图上跑 A*，
    //      保留 unsatisfied 最少、总距离最小的方案；
    //   5) 返回的 MoveResult 描述：新增哪些线、删除/替换哪些线、哪些连接未满足；
    // 返回裸指针，由调用方负责 delete；请求被覆盖或超时时返回空 MoveResult（非 nullptr）
};

#endif
// 结束头文件保护