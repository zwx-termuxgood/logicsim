#ifndef WIRENETWORKSOLVER_H
#define WIRENETWORKSOLVER_H

#include <QVariantList>

namespace WireNetworkSolver {
struct Analysis {
    QVariantList virtualWires;
    QVariantList dots;
};
// 同一几何快照需要两种结果时，只构建一次拓扑。
Analysis analyze(const QVariantList& components, const QVariantList& wires);
// 无状态接口共用同一个整数拓扑构建器，不修改真实线路或拆分用户折线。
QVariantList generateVirtualWires(const QVariantList& components,
                                  const QVariantList& wires);
// 把同网的每对输出/输入端口生成 wireType="virtual" 的电气线：
//   - 端点、折点、端口在线段上时接入；共线重叠按同一导体处理；
//   - 两段内部的纯十字穿越不连接，交点有明确顶点/端口时才连接；
//   - 保留贝塞尔线及 ortho 的显式端口绑定，忽略传入的虚拟线。
// 虚拟线只用于两套引擎建图，不写入模型、不参与画布渲染。

QVariantList connectionDots(const QVariantList& components,
                            const QVariantList& wires);
// 从上述同一拓扑输出整数 {x,y} 圆点：有效方向支路数 + 独立端口数 >= 3。
// 重叠段、重复顶点与重复路径按方向去重，不增加虚假支路；普通直通与折角
// 不画点，T/明确四路节点画实心点，纯十字穿越不画点。
}

#endif // WIRENETWORKSOLVER_H
