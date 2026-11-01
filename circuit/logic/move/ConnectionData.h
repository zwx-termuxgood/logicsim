#ifndef CONNECTIONDATA_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define CONNECTIONDATA_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：位置点 m_loc、路径起点 m_wirePathStart 都以 {x,y} map 表示
#include <QList>
// 引入 QList：m_wirePath 存沿路径经过的连线列表
#include <QString>
// 引入 QString：方向字段 m_dir（"east"/"west"/"north"/"south"）及 hash 计算
#include <QHash>
// 引入 QHash：为提供 qHash 重载支持，使 ConnectionData 可作为 QHash key

class ConnectionData {
    // 一条"连接"的描述：记录被移动元件某端口的位置、朝向、
    // 以及沿该端口向外经过的 wire 链与链末端起点；
    // Connector 用它作为 A* 的起点集合与路径裁剪依据
public:
    // 构造函数 + 访问器 + 相等/哈希（供容器去重用）

    ConnectionData(const QVariantMap& loc, const QString& dir,
                   const QList<QVariantMap>& wirePath,
                   const QVariantMap& wirePathStart)
        : m_loc(loc), m_dir(dir), m_wirePath(wirePath), m_wirePathStart(wirePathStart) {}
    // 构造时一次性拷贝四要素：
    //   loc          — 被移动元件端口的当前坐标；
    //   dir          — 该端口所在 wire 的自然延伸方向（决定 A* 首选方向）；
    //   wirePath     — 沿端口向外遍历到的非选中 ortho wire 列表（可能为空）；
    //   wirePathStart— 沿 wirePath 走到尽头的坐标（A* 终点若不在 wire 上则须经此）
    // 采用值语义拷贝，保证 MoveGesture 计算后即使上下文变化，连接描述仍稳定

    QVariantMap location() const { return m_loc; }
    // 返回端口当前位置 {x, y}；返回副本，调用方修改不会影响内部
    QString direction() const { return m_dir; }
    // 返回首选方向串；为空时 Connector 按位移主分量自行推断
    const QList<QVariantMap>& wirePath() const { return m_wirePath; }
    // 返回 wire 链的 const 引用：遍历频繁，避免拷贝
    QVariantMap wirePathStart() const { return m_wirePathStart; }
    // 返回 wire 链末端起点坐标

    bool equals(const ConnectionData& o) const {
        // 判等只比较位置与方向：
        // wirePath 是位置的外延结果，位置方向相同则连接语义相同；
        // 用于调试/去重场景，不作为 A* 访问判据（那里用 hash）
        return m_loc == o.m_loc && m_dir == o.m_dir;
    }

    int hash() const {
        // 计算哈希：把 x、y 的 double 哈希与方向哈希按 31 组合；
        // 每次乘 31 让各分量错开位，降低不同连接哈希碰撞概率；
        // Connector 用 conn.hash() 作为 pathLocs/initNodes 的 key，
        // 同一次移动请求内需保证稳定且碰撞少
        return qHash(m_loc.value("x").toDouble()) * 31
               + qHash(m_loc.value("y").toDouble()) * 31
               + qHash(m_dir);
    }

private:
    // 私有数据：构造后只读

    QVariantMap m_loc;
    // 端口当前坐标 {x, y}；用 QVariantMap 与上下文/连线统一数据形态
    QString m_dir;
    // 首选延伸方向；空串表示由位移方向推断
    QList<QVariantMap> m_wirePath;
    // 沿端口向外经过的 wire 列表；Connector::processConnection 会遍历它们
    // 生成起始候选点，processPath 用它判断是否需要删除/缩短原线
    QVariantMap m_wirePathStart;
    // wire 链走完后的位置；A* 路径到达该点即视为已接入原网络
};

#endif
// 结束头文件保护