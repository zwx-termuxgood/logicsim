#include "MoveGesture.h"
// 引入本文件要实现的 MoveGesture 类完整声明：定义其成员函数前必须看到类定义
#include "AvoidanceMap.h"
// 引入避让地图完整定义：getFixedAvoidanceMap 需要构造它、调用 create
#include "ConnectorThread.h"
// 引入连接线程：enqueueRequest/forceRequest 需要向工作线程提交请求
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：查询端口几何，用于收集端口位置与判断连接
#include "core/CircuitContext.h"
#include "core/Grid.h"
// 引入电路上下文完整定义：遍历 m_circuit 的元件与连线
#include <QSet>
// 引入 QSet：记录已访问 wire id、选中元件 id，避免重复遍历
#include <QPointF>
// 引入 QPointF：端口位置、沿 wire 走到的当前坐标都用浮点二维点
#include <cmath>
// 引入 C 数学库：std::abs/std::round 用于坐标比较与取整
#include <QMutex>
// 引入 QMutex：全局手势互斥量，保护结果缓存
#include <QMutexLocker>
// 引入 QMutexLocker：RAII 方式加解锁互斥量

static QMutex g_gestureMutex;
// 全局互斥量：保护所有 MoveGesture 的 m_cachedResults；
// 用全局锁而非每对象一把锁，是因为结果缓存可能被工作线程与 UI 线程并发访问，
// 且手势数量少、临界区极短，全局锁实现简单且无死锁风险

// 构造函数：保存上下文指针与选中元件副本
MoveGesture::MoveGesture(const CircuitContext* circuit,
                         const QList<QVariantMap>& selectedComponents)
    : m_circuit(circuit), m_selected(selectedComponents) {}
// 初始化列表直接拷贝两字段：m_circuit 为只读指针，m_selected 为值拷贝，
// 保证拖动期间即使上下文被修改，计算输入仍稳定

// 析构函数：释放缓存的结果与固定避让地图
MoveGesture::~MoveGesture() {
    // 逐项删除缓存中的 MoveResult
    for (auto* r : m_cachedResults) delete r;
    // 删除固定避让地图（可能为空，delete nullptr 安全）
    delete m_initAvoid;
}

// 定义"判断某元件 id 是否在选中列表中"静态辅助函数
static bool isSelected(const QList<QVariantMap>& selected, const QString& id) {
    // 线性遍历选中列表
    for (const auto& s : selected)
        // 命中即返回 true
        if (s.value("id").toString() == id) return true;
    // 全部未命中返回 false
    return false;
}

// 定义"获取连接描述列表"接口：懒计算并缓存
const QList<ConnectionData>& MoveGesture::getConnections() const {
    // 尚未计算过则先计算
    if (!m_connectionsValid) {
        // const 函数内用 const_cast 调用非 const 的 computeConnections
        const_cast<MoveGesture*>(this)->computeConnections();
        // 标记缓存有效
        m_connectionsValid = true;
    }
    // 返回缓存引用，避免拷贝
    return m_connections;
}

// 定义"实际扫描连接"接口
void MoveGesture::computeConnections() {
    m_connections.clear();
    if (!m_circuit) return;
    QSet<QString> seen;
    for (const auto& c : m_selected) {
        auto ports = ComponentLogic::outputPorts(c);
        ports.append(ComponentLogic::inputPorts(c));
        for (const auto& pv : ports) {
            const auto p = pv.toMap();
            const int x = Grid::xi(c) + Grid::xi(p), y = Grid::yi(c) + Grid::yi(p);
            const QString key = QString::number(x) + ":" + QString::number(y);
            if (seen.contains(key)) continue;
            seen.insert(key);
            const auto loc = Grid::point(x, y);
            QList<QVariantMap> wires;
            QString direction;
            // 只收集端口实际接触的线，不能把整个电气网络当作可删除的链。
            for (const auto& wv : m_circuit->wires) {
                const auto w = wv.toMap();
                if (w.value("wireType").toString() != "ortho") continue;
                const auto path = w.value("path").toList();
                for (int j = 0; j + 1 < path.size(); ++j) {
                    const auto a = path[j].toMap(), b = path[j + 1].toMap();
                    if (!Grid::onSegment(x, y, Grid::xi(a), Grid::yi(a), Grid::xi(b), Grid::yi(b))) continue;
                    wires.append(w);
                    if (direction.isEmpty()) {
                        const auto other = (Grid::xi(a) == x && Grid::yi(a) == y) ? b : a;
                        if (Grid::xi(other) != x) direction = Grid::xi(other) > x ? "east" : "west";
                        else direction = Grid::yi(other) > y ? "south" : "north";
                    }
                    break;
                }
            }
            bool fixedPort = false;
            for (const auto& cv : m_circuit->components) {
                const auto other = cv.toMap();
                if (isSelected(m_selected, other.value("id").toString())) continue;
                auto fixed = ComponentLogic::outputPorts(other);
                fixed.append(ComponentLogic::inputPorts(other));
                for (const auto& fpv : fixed) {
                    const auto fp = fpv.toMap();
                    if (Grid::xi(other) + Grid::xi(fp) == x && Grid::yi(other) + Grid::yi(fp) == y)
                        fixedPort = true;
                }
            }
            if (!wires.isEmpty() || fixedPort)
                m_connections.append(ConnectionData(loc, direction, wires, loc));
        }
    }
}

// 定义"获取固定避让地图"接口：懒创建并缓存
AvoidanceMap* MoveGesture::getFixedAvoidanceMap() const {
    // 已创建则直接返回
    if (m_initAvoid) return m_initAvoid;

    // 准备参与标记的元素列表
    QList<QVariantMap> elements;
    // 选中元件 id 集合
    QSet<QString> selIds;
    // 收集选中 id
    for (const auto& s : m_selected) selIds.insert(s.value("id").toString());

    // 所有非选中组件
    for (const auto& cv : m_circuit->components) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 文本元件不占网格，跳过
        if (c.value("type").toString() == "text") continue;
        // 选中元件不参与固定地图（它们的新位置另行叠加），跳过
        if (selIds.contains(c.value("id").toString())) continue;
        // 加入元素列表
        elements.append(c);
    }
    // 所有非选中 wire
    for (const auto& wv : m_circuit->wires) {
        // 连线转 map
        QVariantMap w = wv.toMap();
        // 两端元件 id
        QString from = w.value("fromComp").toString();
        QString to = w.value("toComp").toString();
        // 任一端是选中元件则不参与固定地图，跳过
        if (selIds.contains(from) || selIds.contains(to)) continue;
        // 只处理 ortho 线
        if (w.value("wireType").toString() != "ortho") continue;
        // 加入元素列表
        elements.append(w);
    }

    // 创建固定地图（无整体平移）
    m_initAvoid = AvoidanceMap::create(elements, 0, 0);
    // 返回缓存
    return m_initAvoid;
}

// 定义"工作线程结果回调"接口：写入缓存
void MoveGesture::notifyResult(const MoveRequest& req, MoveResult* result) {
    // 加全局锁保护缓存
    QMutexLocker lk(&g_gestureMutex);
    // 以请求哈希为 key 存入结果
    m_cachedResults[req.hash()] = result;
}

// 定义"查询某位移结果"接口：命中返回缓存指针，未命中返回空
MoveResult* MoveGesture::findResult(int dx, int dy) {
    // 加全局锁保护缓存
    QMutexLocker lk(&g_gestureMutex);
    // 构造对应请求以计算哈希（const_cast 因为构造函数需要非 const 指针）
    MoveRequest req(const_cast<MoveGesture*>(this), dx, dy);
    // 命中返回值，未命中返回 nullptr
    return m_cachedResults.value(req.hash(), nullptr);
}

// 定义"提交异步计算请求"接口
bool MoveGesture::enqueueRequest(int dx, int dy) {
    // 构造请求
    MoveRequest req(this, dx, dy);
    // 用块限界控制锁范围
    {
        // 加全局锁
        QMutexLocker lk(&g_gestureMutex);
        // 已有缓存则无需重复计算，返回 false
        if (m_cachedResults.contains(req.hash())) return false;
    }
    // 交给工作线程非优先入队
    ConnectorThread::instance()->enqueueRequest(req, false);
    // 返回 true 表示已提交
    return true;
}

// 定义"同步强制计算"接口：优先提交并自旋等待结果
MoveResult* MoveGesture::forceRequest(int dx, int dy) {
    // 构造请求
    MoveRequest req(this, dx, dy);
    // 以优先方式入队，确保尽快被计算
    ConnectorThread::instance()->enqueueRequest(req, true);
    // 加全局锁检查/等待缓存
    QMutexLocker lk(&g_gestureMutex);
    // 缓存中尚无结果则循环等待
    while (!m_cachedResults.contains(req.hash())) {
        // 等待期间释放锁，允许工作线程写入
        lk.unlock();
        // 短暂休眠，避免忙等占满 CPU
        QThread::msleep(5);
        // 重新加锁继续检查
        lk.relock();
    }
    // 返回缓存结果（此时必定存在）
    return m_cachedResults.value(req.hash(), nullptr);
}