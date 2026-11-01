#ifndef AVOIDANCEMAP_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define AVOIDANCEMAP_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：元件与连线都以 QVariantMap 传入标记函数
#include <QList>
// 引入 QList：markAll 与 create 接收元件/连线列表
#include <QHash>
// 引入 QHash：内部用 "x,y" → 允许方向 的哈希表表示网格占用
#include <QString>
// 引入 QString：key 类型与三个常量标记都是 QString

class AvoidanceMap {
    // A* 寻路用的网格占用图：把不可穿越/单向可穿越的区域编码进哈希表；
    // 只记录被占用的格子，空闲格子不存条目（默认视为可自由通过），节省内存
public:
    // 公有接口：常量标记、工厂、克隆、查询、标记、取消标记

    static const QString ALLOW_NEITHER;
    // 标记：该格完全禁止穿越（落在元件包围盒或线与线交叉冲突处）
    static const QString ALLOW_VERTICAL;
    // 标记：只允许垂直方向穿越（格点被水平线段占用，垂直穿过视为合法交叉）
    static const QString ALLOW_HORIZONTAL;
    // 标记：只允许水平方向穿越（格点被垂直线段占用）
    // 这三个常量的语义在 SearchNode 扩展邻居时被解释为"可用方向数"

    static AvoidanceMap* create(const QList<QVariantMap>& elements, int dx, int dy);
    // 工厂函数：新建地图并一次性标记全部元素；
    // 返回裸指针由调用方 delete（MoveGesture 缓存固定地图，Connector 克隆后临时使用）；
    // dx/dy 用于预演"整体平移后的占用"，选中元件移动时支持提前避让
    AvoidanceMap* cloneMap() const;
    // 克隆：复制 m_map 生成独立地图；
    // A* 过程中会临时标记/取消标记，必须克隆固定地图以免污染缓存

    QString get(double x, double y) const;
    // 查询某坐标的允许方向：
    // 未标记返回空串（完全自由）；否则返回三个常量之一

    void markAll(const QList<QVariantMap>& elements, int dx, int dy);
    // 批量标记：对每个元素按 wireType 字段分派到 markWire 或 markComponent；
    // 设计上把"元件/连线"统一成 elements 列表，调用方无需自己分类
    void markComponent(const QVariantMap& comp, int dx, int dy);
    // 标记元件占用：按类型估算包围盒，对包围盒内网格点写 ALLOW_NEITHER；
    // 不同元件尺寸不同（输入/输出/子电路/分线器等），故需分类型估算宽高
    void markWire(const QVariantMap& wire, int dx, int dy);
    // 标记连线占用：
    //   - ortho：沿每段采样，水平段标记 ALLOW_VERTICAL（只许垂直穿越），
    //     垂直段标记 ALLOW_HORIZONTAL；若格子已被相反方向占用则升级为 ALLOW_NEITHER；
    //   - bezier：不占网格（曲线难以精确离散，且寻路只对 ortho 生效）
    void unmarkLocation(double x, double y);
    // 取消某坐标标记：A* 找到路径后回退临时标记时使用
    void unmarkWire(const QVariantMap& wire, const QVariantMap& deletedEnd);
    // 取消某连线的标记：
    // 逐段移除本线写入的方向标记，并尽量恢复被本线覆盖前的另一方向标记；
    // deletedEnd 参数用于识别被缩短/删除的一端，避免误清相邻段

private:
    // 私有区：构造、存储、key 编码

    AvoidanceMap() {}
    // 私有构造函数：强制通过 create 工厂创建，保证标记流程统一

    QHash<QString, QString> m_map;
    // 占用表：key 为 "x,y"（取整到 10px 网格），value 为三个允许方向常量之一；
    // 稀疏存储——只有被元件/连线覆盖的格子才有条目

    static QString keyOf(double x, double y) {
        // 坐标编码：四舍五入到整数再拼成 "x,y"；
        // 用整数网格而非原始 double 是因为寻路步长固定 10px，
        // 取整后可容忍浮点误差，且字符串 key 便于调试打印
        return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
    }
};

#endif
// 结束头文件保护