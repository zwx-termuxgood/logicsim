#include "MoveGesture.h"
// 引入本文件要实现的 MoveGesture 类完整声明：定义其成员函数前必须看到类定义
#include "AvoidanceMap.h"
// 引入避让地图完整定义：getFixedAvoidanceMap 需要构造它、调用 create
#include "ConnectorThread.h"
// 引入连接线程：enqueueRequest/forceRequest 需要向工作线程提交请求
#include "../ComponentLogic.h"
// 引入元件逻辑库：查询端口几何，用于收集端口位置与判断连接
#include "../../core/CircuitContext.h"
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
    // 清空旧连接
    m_connections.clear();
    // 没有选中元件则无连接
    if (m_selected.isEmpty()) return;

    // 收集所有选中元件的端口位置
    QList<QPointF> locs;
    // 遍历选中元件
    for (const auto& c : m_selected) {
        // 元件中心坐标
        double cx = c.value("x").toDouble();
        double cy = c.value("y").toDouble();
        // 查询输出端口几何
        QVariantList outs = ComponentLogic::outputPorts(c);
        // 逐个输出端口换算成世界坐标加入列表
        for (const auto& pv : outs) {
            // 端口转 map
            QVariantMap p = pv.toMap();
            // 世界坐标 = 元件坐标 + 端口偏移
            locs.append(QPointF(cx + p.value("x").toDouble(),
                                cy + p.value("y").toDouble()));
        }
        // 查询输入端口几何
        QVariantList ins = ComponentLogic::inputPorts(c);
        // 逐个输入端口换算成世界坐标加入列表
        for (const auto& pv : ins) {
            // 端口转 map
            QVariantMap p = pv.toMap();
            // 世界坐标 = 元件坐标 + 端口偏移
            locs.append(QPointF(cx + p.value("x").toDouble(),
                                cy + p.value("y").toDouble()));
        }
    }

    // 对每个位置，判断是否连接到非选中组件
    for (const auto& loc : locs) {
        // 该位置是否与非选中元件端口重合
        bool found = false;
        // 遍历所有"非选中组件"，检查其端口是否在 loc 上
        for (const auto& cv : m_circuit->components) {
            // 元件转 map
            QVariantMap c = cv.toMap();
            // 选中的元件跳过
            if (isSelected(m_selected, c.value("id").toString())) continue;
            // 元件中心坐标
            double cx = c.value("x").toDouble();
            double cy = c.value("y").toDouble();
            // 检查输出端口
            QVariantList outs = ComponentLogic::outputPorts(c);
            for (const auto& pv : outs) {
                // 端口转 map
                QVariantMap p = pv.toMap();
                // 与 loc 坐标在容差内重合即命中
                if (qAbs(cx + p.value("x").toDouble() - loc.x()) < 0.5 &&
                    qAbs(cy + p.value("y").toDouble() - loc.y()) < 0.5) {
                    found = true; break;
                }
            }
            // 已命中则跳出外层元件循环
            if (found) break;
            // 检查输入端口
            QVariantList ins = ComponentLogic::inputPorts(c);
            for (const auto& pv : ins) {
                // 端口转 map
                QVariantMap p = pv.toMap();
                // 与 loc 坐标在容差内重合即命中
                if (qAbs(cx + p.value("x").toDouble() - loc.x()) < 0.5 &&
                    qAbs(cy + p.value("y").toDouble() - loc.y()) < 0.5) {
                    found = true; break;
                }
            }
            // 已命中则跳出
            if (found) break;
        }
        // 该位置不与非选中元件相连，无需重连
        if (!found) continue;

        // 找 wirePath：从这个位置出发，沿非选中 wire 走
        QList<QVariantMap> wirePath;
        // 路径终点坐标，默认就是起点
        QPointF pathStart = loc;
        // 路径上最后一根 wire
        QVariantMap lastOnPath;
        // 已访问 wire id 集合
        QSet<QString> visited;
        // 当前沿链走到的坐标
        QPointF cur = loc;

        // 找第一条经过 cur 的、非选中的正交 wire
        auto findWire = [&](const QPointF& pt, const QString& ignoreId) -> QVariantMap {
            // 遍历所有连线
            for (const auto& wv : m_circuit->wires) {
                // 连线转 map
                QVariantMap w = wv.toMap();
                // 连线 id
                QString id = w.value("id").toString();
                // 忽略指定 id
                if (id == ignoreId) continue;
                // 已访问过则跳过
                if (visited.contains(id)) continue;
                // 只处理 ortho 线
                QString wt = w.value("wireType").toString();
                if (wt != "ortho") continue;
                // 检查 wire 是否有 path 顶点在 pt 上
                QVariantList path = w.value("path").toList();
                for (const auto& pv : path) {
                    // 顶点转 map
                    QVariantMap p = pv.toMap();
                    // 与 pt 在容差内重合即命中，返回该线
                    if (qAbs(p.value("x").toDouble() - pt.x()) < 0.5 &&
                        qAbs(p.value("y").toDouble() - pt.y()) < 0.5) {
                        return w;
                    }
                }
            }
            // 没找到返回空 map
            return QVariantMap();
        };

        // 从起点找第一根 wire
        QVariantMap first = findWire(cur, QString());
        // 找到则开始沿链走
        if (!first.isEmpty()) {
            // 标记已访问
            visited.insert(first.value("id").toString());
            // 加入路径
            wirePath.append(first);
            // 记录最后走到的线
            lastOnPath = first;

            // 沿 path 向外走
            QVariantList path = first.value("path").toList();
            // 找 cur 在 path 中的位置
            int curIdx = -1;
            for (int i = 0; i < path.size(); ++i) {
                // 顶点转 map
                QVariantMap p = path[i].toMap();
                // 与 cur 重合则记录索引
                if (qAbs(p.value("x").toDouble() - cur.x()) < 0.5 &&
                    qAbs(p.value("y").toDouble() - cur.y()) < 0.5) {
                    curIdx = i; break;
                }
            }
            // 定位成功则把 cur 推进到线的另一端
            if (curIdx >= 0) {
                // 起点是首顶点：向末顶点走
                if (curIdx == 0) {
                    // 从 path.first 开始，向 path.last 走
                    QVariantMap lastP = path.last().toMap();
                    cur = QPointF(lastP.value("x").toDouble(), lastP.value("y").toDouble());
                    // 起点是末顶点：向首顶点走
                } else if (curIdx == path.size() - 1) {
                    QVariantMap firstP = path.first().toMap();
                    cur = QPointF(firstP.value("x").toDouble(), firstP.value("y").toDouble());
                }
            }

            // 继续向外找
            for (int iter = 0; iter < 100; ++iter) {
                // 从当前坐标找下一根 wire
                QVariantMap next = findWire(cur, QString());
                // 找不到则链结束
                if (next.isEmpty()) break;
                // 标记已访问
                visited.insert(next.value("id").toString());
                // 加入路径
                wirePath.append(next);
                // 取下一根线的 path
                QVariantList p2 = next.value("path").toList();
                // 找 cur 在该 path 中的位置
                int idx2 = -1;
                for (int i = 0; i < p2.size(); ++i) {
                    // 顶点转 map
                    QVariantMap p = p2[i].toMap();
                    // 与 cur 重合则记录索引
                    if (qAbs(p.value("x").toDouble() - cur.x()) < 0.5 &&
                        qAbs(p.value("y").toDouble() - cur.y()) < 0.5) {
                        idx2 = i; break;
                    }
                }
                // 定位失败则结束
                if (idx2 < 0) break;
                // 推进到线的另一端
                if (idx2 == 0) {
                    QVariantMap lp = p2.last().toMap();
                    cur = QPointF(lp.value("x").toDouble(), lp.value("y").toDouble());
                } else if (idx2 == p2.size() - 1) {
                    QVariantMap fp = p2.first().toMap();
                    cur = QPointF(fp.value("x").toDouble(), fp.value("y").toDouble());
                    // 起点在线的中间（T 型接入），无法继续向单方向走，结束
                } else break;
            }
            // 链末端坐标作为路径终点
            pathStart = cur;
        }

        // 计算方向：从最后一段 wire 的走向推断首选延伸方向
        QString dir;
        // 有走过的线才推断
        if (!lastOnPath.isEmpty()) {
            // 取该线 path
            QVariantList lp = lastOnPath.value("path").toList();
            // 至少两个顶点才能算方向
            if (lp.size() >= 2) {
                // 取 loc 附近的顶点与相邻点
                double dx = 0, dy = 0;
                // 首顶点
                QVariantMap a = lp.first().toMap();
                // 末顶点
                QVariantMap b = lp.last().toMap();
                // 计算首末顶点差
                dx = a.value("x").toDouble() - b.value("x").toDouble();
                dy = a.value("y").toDouble() - b.value("y").toDouble();
                // 横向差为主：东/西
                if (std::abs(dx) > std::abs(dy))
                    dir = dx > 0 ? "east" : "west";
                // 纵向差为主：南/北
                else
                    dir = dy > 0 ? "south" : "north";
            }
        }

        // 起点坐标 map
        QVariantMap locMap;
        locMap["x"] = loc.x();
        locMap["y"] = loc.y();
        // 路径终点坐标 map
        QVariantMap startMap;
        startMap["x"] = pathStart.x();
        startMap["y"] = pathStart.y();

        // 组装连接描述并加入结果
        m_connections.append(ConnectionData(locMap, dir, wirePath, startMap));
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