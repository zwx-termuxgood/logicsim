#ifndef SEARCHNODE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define SEARCHNODE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：节点位置 m_loc 与目标位置 m_dest 都是 {x,y} map
#include <QString>
// 引入 QString：方向字段 m_dir（"east"/"west"/"north"/"south"）及 next 的 moveDir 参数

class ConnectionData;
// 前置声明：SearchNode 只持有指针 m_conn，无需完整定义；
// 避免与 ConnectionData.h 形成循环包含（ConnectionData 不需要 SearchNode）

class SearchNode {
    // A* 寻路的搜索节点：表示"在某位置、朝某方向、正在延伸某条连接"的状态；
    // 同一个 (位置, 方向) 可能因 extendsWire 不同而有不同代价，
    // 但 hash/equals 只按 (loc, dir, dest) 判同，保证搜索空间有限
public:
    // 惩罚常量 + 两种构造 + 访问器 + 扩展

    static const int CROSSING_PENALTY = 20;
    // 穿越已有线/元件的额外代价：让路径尽量避开而非硬穿，
    // 但数值不大，保证实在无路可走时仍能穿过去
    static const int TURN_PENALTY = 50;
    // 每转一次弯的额外代价：远大于交叉惩罚，因为直角拐点影响美观；
    // 这让 A* 优先选少拐弯的长路径而非多拐弯的短路径

    SearchNode(ConnectionData* conn, const QVariantMap& src,
               const QString& srcDir, const QVariantMap& dst);
    // 起点构造：prev=nullptr、dist=0；
    // extendsWire 由 srcDir 是否为空决定——空方向表示凭空起步，不算延伸已有线；
    // 构造时立即计算启发值 m_heur

    SearchNode(const QVariantMap& loc, const QString& dir, ConnectionData* conn,
               const QVariantMap& dest, int dist, bool extendsWire, SearchNode* prev);
    // 扩展构造：由 next() 调用，携带新位置、方向、累计距离、是否仍在延伸原线、父指针；
    // prev 链用于最终回溯出完整路径

    int heuristicValue() const { return m_heur; }
    // 返回启发值（f = g + h）：优先队列按它排序
    QVariantMap location() const { return m_loc; }
    // 返回当前位置 {x,y}
    QVariantMap destination() const { return m_dest; }
    // 返回目标位置
    QString direction() const { return m_dir; }
    // 返回当前朝向
    int distance() const { return m_dist; }
    // 返回从起点累计距离（g）：多轮尝试比较方案总距离时使用
    SearchNode* previous() const { return m_prev; }
    // 返回父节点：convertToPath 据此回溯并提取拐点
    bool isDestination() const;
    // 判断当前位置是否与目标重合：容差 0.01，吸收浮点误差
    bool isStart() const { return m_prev == nullptr; }
    // 是否起点：Connector 用它决定是否放宽起点处的避让限制
    bool isExtendingWire() const { return m_extendsWire; }
    // 当前是否仍沿原 wire 方向延伸：
    // 影响启发函数——延伸时按轴向距离估计，否则按曼哈顿距离并加转弯惩罚
    ConnectionData* connection() const { return m_conn; }
    // 返回所属连接：用于读取方向偏好、wirePath 等

    bool equals(const SearchNode& o) const;
    // 状态判等：比较 loc、dir、dest 三者；
    // 不含 dist/prev，因为 A* 中同一状态只应保留最优到达方式
    int hash() const;
    // 状态哈希：把 loc 的 x、y 与 dir、dest 组合成整数，
    // 供 visited 集合去重；必须与 equals 语义一致

    SearchNode* next(const QString& moveDir, bool crossing);
    // 扩展一个邻居：
    //   - 按 moveDir 前进一格（10px）；
    //   - 累计距离加 10（若仍沿原线延伸则加 9，略优惠，鼓励顺着原线走）；
    //   - crossing 为真时加 CROSSING_PENALTY；
    //   - moveDir 与当前 m_dir 不同时加 TURN_PENALTY；
    //   - 越界（负坐标）返回 nullptr；
    // 返回新节点，所有权归调用方（Connector 统一收集后 delete）

private:
    // 私有数据与启发函数

    int getHeuristic();
    // 计算启发值 h：
    //   - 若在延伸原线且目标在原方向前方，用轴向距离 * 9 + 横向偏差（乐观估计）；
    //   - 否则用曼哈顿距离；
    //   - 若当前方向与目标方向相反，额外加两倍转弯惩罚（惩罚回头路）；
    //   - 若还需转弯，加一次转弯惩罚；
    // 构造时调用一次，存入 m_heur

    QVariantMap m_loc;
    // 当前位置
    QString m_dir;
    // 当前朝向；空串表示起点尚未确定方向
    ConnectionData* m_conn;
    // 所属连接（不拥有）
    QVariantMap m_dest;
    // 目标位置
    int m_dist;
    // 从起点累计距离 g
    int m_heur;
    // 缓存的启发值 f = g + h
    bool m_extendsWire;
    // 是否仍在延伸原线：影响代价与启发计算
    SearchNode* m_prev;
    // 父节点指针：构成搜索树，回溯路径用
};

#endif
// 结束头文件保护