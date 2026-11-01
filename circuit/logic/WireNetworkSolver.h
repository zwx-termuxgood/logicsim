#ifndef WIRENETWORKSOLVER_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define WIRENETWORKSOLVER_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantList>
// 引入 QVariantList：两个函数的入参（元件/连线列表）与返回类型（虚拟线/圆点列表）都用它

namespace WireNetworkSolver {
// 用命名空间而非类：全部是无状态纯函数，输入决定输出，便于两套引擎复用与独立测试

QVariantList generateVirtualWires(const QVariantList& components,
                                  const QVariantList& wires);
// 生成虚拟线（电气互联拓扑）：
//   - 用并查集把"元件端口位置"与"wire 路径顶点"合并成电气网络；
//   - 合并规则：元件端口↔位置节点、wire 内部所有顶点、端点重合、T 型接入
//     （端点落在另一根线中间段）；十字交叉（中间对中间）不合并；
//   - 最后为每个网络中的每对 (输出端口, 输入端口) 生成一条 wireType="virtual" 的线；
// 返回的虚拟线只用于引擎建图，不写入上下文、不参与渲染；
// 两套引擎都先调用它，再与真实 wire 合并后建图，从而统一处理 T 型/多驱等拓扑

QVariantList connectionDots(const QVariantList& components,
                            const QVariantList& wires);
// 计算连接圆点位置：
//   - 统计每个坐标处的"连接数"：元件端口 +1，wire 顶点 +1，
//     T 型接入点额外 +2（模拟该 wire 被分割成两段）；
//   - 连接数 >= 3 的位置返回 {x, y}，供 QML 绘制实心圆点；
// 之所以用计数模拟而非真正分割 wire，是为了在只读展示时保持数据结构不变，
// 同时让视觉结果与 Logisim 的圆点规则一致
}

#endif
// 结束头文件保护