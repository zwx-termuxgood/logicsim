// 引入 Circuit 类声明：本文件要定义它的成员函数，必须看到完整类定义（成员、私有函数、Context 别名等）
#include "model/Circuit.h"
// 引入 ID 分配器：addComponent 里给新元件分配唯一 id、addOrthoWire 等给连线分配 id 都要用它
#include "core/IdAllocator.h"
// 统一使用整数坐标与 10px 移动网格
#include "core/Grid.h"
// 引入元件逻辑库：查询元件端口几何（outputPorts/inputPorts）以及端口数量都委托给它，保证端口规则唯一来源
#include "logic/ComponentLogic.h"
// 引入事件引擎：仿真运行时需要 rebuildEventEngine、triggerInputString/Bit、run 等接口
#include "engine/event/EventEngine.h"
#include "engine/iter/IterEngine.h"
// 引入移动手势类型：本文件包含移动相关接口，可能间接使用该头里的类型/常量
#include "move/MoveGesture.h"
// 引入移动结果结构：移动操作可能返回/使用该结构描述位移结果
#include "move/MoveResult.h"
// 引入 QVariantMap：每个元件/连线都以 map 表示，用字符串 key 存取字段
#include <QVariantMap>
// 引入 QSet：用于去重、记录本次移动的元件和连线 ID
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <QList>
#include <algorithm>
#include <utility>

// 定义"添加元件"接口：类型、位置、位宽、输入数、子电路 id（仅 sub 类型用）
QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    // 浏览子电路（只读）模式下禁止编辑，直接返回空 id 表示失败
    if (m_viewOnly || !Grid::valid(x) || !Grid::valid(y)) return QString();
    // 位宽下限钳到 1：0 位或负位没有意义，会破坏位运算/格式化逻辑
    if (bitWidth < 1) bitWidth = 1;
    // 位宽上限钳到 64：内部用 quint64/二进制串表示，超过 64 位无法安全承载
    if (bitWidth > 64) bitWidth = 64;
    // 输入数下限钳到 2：多输入门至少两输入才有意义（not/led 等单输入元件在下方分支里单独处理）
    if (inputCount < 2) inputCount = 2;
    // 输入数上限钳到 64：防止 UI 传入离谱值导致端口列表爆炸
    if (inputCount > 64) inputCount = 64;
    // 放置子电路实例前检查是否会形成自包含/循环引用，不允许则直接失败
    if (type == "sub" && !canAddSubInstance(subId)) return QString();
    //
    // 先压撤销快照：保证这次"添加元件"可被撤销；必须在真正改数据之前调用
    pushUndo();
    // 准备新元件的 map，后续逐字段填充
    QVariantMap c;
    // 取当前上下文引用（根或子电路），新元件插入这里
    Context& ctx = currentCtx();
    // 分配唯一元件 id：把当前上下文传进去，让分配器基于已有元件生成不冲突的编号
    c["id"] = IdAllocator::nextComponentId(ctx);
    // 写入类型字段
    c["type"] = type;
    // 写入世界坐标位置
    c["x"] = Grid::snap(x); c["y"] = Grid::snap(y);
    // 写入（已钳位的）位宽
    c["bitWidth"] = bitWidth;
    // 写入（已钳位的）输入端口数
    c["inputCount"] = inputCount;
    // 默认显示进制为二进制，QML 输入框/显示据此格式化
    c["displayBase"] = "bin";
    // 默认无自定义名称，由 QML 侧按类型显示默认标签
    c["name"] = "";
    // 准备 inputBits 列表：input/const 元件用它保存每一位的布尔值
    QVariantList bits;
    // 初始化所有位为 false（0），长度等于位宽，保证后续读取时长度一致
    for (int i = 0; i < bitWidth; ++i) bits.append(false);
    // 写入 inputBits 字段
    c["inputBits"] = bits;
    //
    // 准备输出端口值列表和输入端口值列表：元素是二进制串，表示该端口当前值
    QVariantList outPorts, inPorts;
    // 文本元件：写入默认内容"文本"
    if (type == "text") c["content"] = "文本";
    // 时钟元件：默认名称 CLK，便于识别
    else if (type == "clock") c["name"] = "CLK";
    // 常量元件：默认名称 CONST
    else if (type == "const") c["name"] = "CONST";
    // 子电路实例分支
    else if (type == "sub") {
        // 记录引用的子电路 id，引擎/刷新时据此找到对应上下文
        c["subId"] = subId;
        // 占位：子电路输入端口名，后续 syncSubPins 会填充
        c["subInputNames"] = QVariantList();
        // 占位：子电路输出端口名
        c["subOutputNames"] = QVariantList();
        // 占位：子电路输入端口位宽
        c["subInputWidths"] = QVariantList();
        // 占位：子电路输出端口位宽
        c["subOutputWidths"] = QVariantList();
    }
    // 分线器分支：1 个输入，按 outputSplits 拆成多段输出
    else if (type == "splitter") {
        // 准备分段列表
        QVariantList splits;
        // 默认每段 1 位，共 bitWidth 段，总和恰好等于位宽
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        // 写入分段字段
        c["outputSplits"] = splits;
        // 输入端口初始值：bitWidth 个 '0' 的二进制串
        inPorts.append(QString(bitWidth, '0'));
        // 为每个分段生成一个输出端口初始值
        for (int i = 0; i < splits.size(); ++i) {
            // 取出该段位宽，防御性钳到至少 1
            int w = splits[i].toInt(); if (w < 1) w = 1;
            // 该段输出端口初始值为 w 个 '0'
            outPorts.append(QString(w, '0'));
        }
    }
    // 汇线器分支：多段输入按 outputSplits 合成 1 个输出
    else if (type == "hub") {
        // 准备分段列表
        QVariantList splits;
        // 默认每段 1 位，共 bitWidth 段
        for (int i = 0; i < bitWidth; ++i) splits.append(1);
        // 写入分段字段（hub 复用同一字段名表示各输入段宽度）
        c["outputSplits"] = splits;
        // 为每个分段生成一个输入端口初始值
        for (int i = 0; i < splits.size(); ++i) {
            // 取出该段位宽并钳位
            int w = splits[i].toInt(); if (w < 1) w = 1;
            // 该段输入端口初始值为 w 个 '0'
            inPorts.append(QString(w, '0'));
        }
        // 输出端口初始值：总位宽个 '0'
        outPorts.append(QString(bitWidth, '0'));
    }
    // 传输门分支：3 输入（数据/控制各方向），1 输出
    else if (type == "tgate") {
        // 强制输入数为 3，覆盖外部传入值，保证端口数与语义一致
        c["inputCount"] = 3;
        // 1 个输出端口，初始全 0
        outPorts.append(QString(bitWidth, '0'));
        // 第 1 个输入端口初始全 0
        inPorts.append(QString(bitWidth, '0'));
        // 第 2 个输入端口初始全 0
        inPorts.append(QString(bitWidth, '0'));
        // 第 3 个输入端口初始全 0
        inPorts.append(QString(bitWidth, '0'));
    }
    // N/P 型晶体管分支：2 输入，1 输出
    else if (type == "ntran" || type == "ptran") {
        // 强制输入数为 2
        c["inputCount"] = 2;
        // 1 个输出端口初始全 0
        outPorts.append(QString(bitWidth, '0'));
        // 第 1 个输入端口初始全 0
        inPorts.append(QString(bitWidth, '0'));
        // 第 2 个输入端口初始全 0
        inPorts.append(QString(bitWidth, '0'));
    }
    // 其余常规元件分支（input/const/clock/not/and/or/... /led/output）
    else {
        // n 表示输出端口数量
        int n = 0;
        // input/const/clock/not 各有 1 个输出；与/或/与非、或非/异或/同或也各有 1 个输出
        if (type == "input" || type == "const" || type == "clock" || type == "not" ||
            type == "and" || type == "or" || type == "nand" ||
            type == "nor" || type == "xor" || type == "xnor") n = 1;
        // 按 n 生成输出端口初始值（全 0）
        for (int i = 0; i < n; ++i) outPorts.append(QString(bitWidth, '0'));
        // ic 表示输入端口数量
        int ic = 0;
        // 非门/指示灯/输出端口只有 1 个输入
        if (type == "not" || type == "led" || type == "output") ic = 1;
        // 多输入逻辑门：输入数用调用者传入（已钳位）的 inputCount
        else if (type == "and" || type == "or" || type == "nand" ||
                 type == "nor" || type == "xor" || type == "xnor") ic = inputCount;
        // 按 ic 生成输入端口初始值（全 0）
        for (int i = 0; i < ic; ++i) inPorts.append(QString(bitWidth, '0'));
    }
    // 把输出端口值列表写入元件 map
    c["outputPorts"] = outPorts;
    // 把输入端口值列表写入元件 map
    c["inputPortValues"] = inPorts;
    //
    // 把新元件追加到当前上下文元件列表
    ctx.components.append(c);
    // 元件集合变了，"上下文→子电路实例 id 列表"缓存失效，必须清空
    m_ctxSubInstCache.clear();
    // 新时钟元件的初始输出状态登记为 false（低电平）
    if (type == "clock") m_clockStates[c["id"].toString()] = false;
    //
    // 立即全量求值：让新元件（尤其带连线后）输出收敛，界面马上能显示正确值
    evaluateAll();
    // 通知 QML 数据变了，需要重绘
    emit changed();
    // 通知结构变了（元件集合变化），QML 需重建连线网络/端口索引等
    emit structureChanged();
    // 返回新元件 id，成功标志；调用方用它继续操作（如 addText 里紧接着设置文本）
    return c["id"].toString();
}

// 定义"删除单个元件"接口
void Circuit::removeComponent(const QString& id) {
    // 只读浏览模式禁止删除
    if (m_viewOnly) return;
    // 在当前上下文查元件索引
    int idx = indexOfComponent(id);
    // 找不到则什么都不做
    if (idx < 0) return;
    // 压撤销快照，保证删除可撤销
    pushUndo();
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出待删元件 map，后面要用它的类型判断是否需要清时钟状态
    QVariantMap c = ctx.components[idx].toMap();
    //
    // 如果删的是时钟元件，同步清掉它的输出状态记录，避免残留
    if (c.value("type").toString() == "clock") m_clockStates.remove(id);
    // 从元件列表移除该元件
    ctx.components.removeAt(idx);
    // 子电路实例列表可能变化（删的可能是 sub 实例），缓存失效
    m_ctxSubInstCache.clear();
    //
    // 正交线不随元件删除而删除，只清掉它指向被删元件的绑定；
    // 只有贝塞尔线才随端点匹配删除。
    // 处理策略：
    //   · ortho：fromComp/toComp 若指向被删元件，就把该端绑定字段
    //     （fromComp/fromPort 或 toComp/toPort）移除，path 顶点原位保留，
    //     视觉上变成一条"悬空但仍在"的正交线，用户可继续拖动/重连；
    //   · bezier：fromComp/toComp 若指向被删元件，整条线删除。
    // 对 ortho 的 T 型接入不删除：T 型接入的路径点本来就位于被删元件
    // 的端口坐标，随元件消失自然悬空，符合"正交线保留"的需求。
    //
    // 倒序遍历连线：倒序删除不会打乱未遍历索引
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        // 取出连线 map
        QVariantMap w = ctx.wires[i].toMap();
        // 取线型，缺省回退 bezier（与全项目一致）
        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";
        // 起点是否指向被删元件
        bool fromHits = (w.value("fromComp").toString() == id);
        // 终点是否指向被删元件
        bool toHits   = (w.value("toComp").toString() == id);
        // 两端都不指向被删元件：无关，直接跳过
        if (!fromHits && !toHits) continue;

        // 【分支】正交线：不删除，只清除对已删元件的绑定
        if (wt == "ortho") {
            // 起点绑定被删元件：清除起点绑定字段
            if (fromHits) {
                w.remove("fromComp");
                w.remove("fromPort");
            }
            // 终点绑定被删元件：清除终点绑定字段
            if (toHits) {
                w.remove("toComp");
                w.remove("toPort");
            }
            // 写回修改后的连线（path 顶点保留原位，保持"线还在"的视觉）
            ctx.wires[i] = w;
            // 继续检查下一条
            continue;
        }

        // 【分支】贝塞尔线（或 wireType 缺失）：端点指向被删元件则删除
        ctx.wires.removeAt(i);
    }
    //
    // 结构变了，重新全量求值让剩余电路收敛
    evaluateAll();
    // 通知 QML 重绘
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 定义"批量删除元件"接口
void Circuit::removeComponents(const QStringList& ids) {
    // 只读模式禁止删除
    if (m_viewOnly) return;
    // 空列表直接返回，省去后续开销
    if (ids.isEmpty()) return;
    // 把 id 列表转成集合，后续 O(1) 判断"是否在删除集合里"
    QSet<QString> idSet;
    // 逐个插入集合
    for (const auto& id : ids) idSet.insert(id);
    // 标记是否至少命中一个真实存在的元件
    bool any = false;
    // 遍历当前上下文元件
    for (const auto& cv : currentCtx().components) {
        // 命中任一即置 true 并提前退出
        if (idSet.contains(cv.toMap().value("id").toString())) { any = true; break; }
    }
    // 选择集合也包含线路；仅选线时同样允许批量删除。
    if (!any) {
        for (const auto& wv : currentCtx().wires) {
            if (idSet.contains(wv.toMap().value("id").toString())) { any = true; break; }
        }
    }
    // 一个都不存在就没必要压撤销/改数据
    if (!any) return;
    endSelectionMove();
    // 压撤销快照（整批一次快照，撤销时整批恢复）
    pushUndo();
    // 取当前上下文
    Context& ctx = currentCtx();
    //
    // 倒序遍历元件，便于原地删除
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        // 取出元件 map
        QVariantMap c = ctx.components[i].toMap();
        // 不在删除集合里则跳过
        if (!idSet.contains(c.value("id").toString())) continue;
        // 若删的是时钟元件
        if (c.value("type").toString() == "clock")
            // 同步清掉其输出状态
            m_clockStates.remove(c.value("id").toString());
        // 从元件列表移除
        ctx.components.removeAt(i);
    }
    //
    // 元件集合变化，子电路实例缓存失效
    m_ctxSubInstCache.clear();
    //
    // 显式选中的线路整条删除；其余线路沿用元件删除后的绑定清理规则。
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        QVariantMap w = ctx.wires[i].toMap();
        if (idSet.contains(w.value("id").toString())) {
            ctx.wires.removeAt(i);
            continue;
        }
        // 取线型，缺省回退 bezier
        QString wt = w.value("wireType").toString();
        if (wt.isEmpty()) wt = "bezier";
        // 起点是否指向被删集合中的某个元件
        bool fromHits = idSet.contains(w.value("fromComp").toString());
        // 终点是否指向被删集合中的某个元件
        bool toHits   = idSet.contains(w.value("toComp").toString());
        // 两端都不指向被删集合：无关，直接跳过
        if (!fromHits && !toHits) continue;

        // 【分支】正交线：不删除，只清除对已删元件的绑定
        if (wt == "ortho") {
            // 起点绑定被删元件：清除起点绑定字段
            if (fromHits) {
                w.remove("fromComp");
                w.remove("fromPort");
            }
            // 终点绑定被删元件：清除终点绑定字段
            if (toHits) {
                w.remove("toComp");
                w.remove("toPort");
            }
            // 写回修改后的连线（path 顶点保留原位）
            ctx.wires[i] = w;
            // 继续检查下一条
            continue;
        }

        // 【分支】贝塞尔线：端点指向被删元件则删除
        ctx.wires.removeAt(i);
    }
    //
    // 重新全量求值
    evaluateAll();
    // 通知 QML 重绘
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 移动前的真实端口决定随动范围，不沿整个电气网络传播位移。
void Circuit::updateWiresForMovedComponent(Context& ctx, const QSet<QString>& movedIds,
                                           double dx, double dy) {
    const int sx = Grid::snap(dx), sy = Grid::snap(dy);
    QList<QVariantMap> oldPorts;
    for (const auto& cv : ctx.components) {
        const auto c = cv.toMap();
        if (!movedIds.contains(c.value("id").toString())) continue;
        auto ports = ComponentLogic::outputPorts(c);
        ports.append(ComponentLogic::inputPorts(c));
        for (const auto& pv : ports) {
            const auto p = pv.toMap();
            const auto pos = Grid::point(Grid::xi(c) + Grid::xi(p) - sx,
                                         Grid::yi(c) + Grid::yi(p) - sy);
            if (!oldPorts.contains(pos)) oldPorts.append(pos);
        }
    }
    auto same = [](const QVariantMap& a, const QVariantMap& b) {
        return Grid::xi(a) == Grid::xi(b) && Grid::yi(a) == Grid::yi(b);
    };
    auto movedPort = [&](const QVariantMap& p) {
        for (const auto& port : oldPorts) if (same(p, port)) return true;
        return false;
    };
    auto translated = [&](const QVariantMap& p) {
        return Grid::point(Grid::xi(p) + sx, Grid::yi(p) + sy);
    };
    // 单端移动时先按原首段方向接到原相邻点，远端及后续折点不动。
    auto moveEnd = [&](QVariantList path, bool front) {
        if (!front) std::reverse(path.begin(), path.end());
        const auto a = path[0].toMap(), b = path[1].toMap();
        const auto n = translated(a);
        const bool vertical = Grid::xi(a) == Grid::xi(b);
        const auto corner = vertical ? Grid::point(Grid::xi(n), Grid::yi(b))
                                     : Grid::point(Grid::xi(b), Grid::yi(n));
        path[0] = n;
        if (!same(corner, n) && !same(corner, b)) path.insert(1, corner);
        // 折回原顶点时去零长段，不调用会补斜段的通用规范化掩盖错误。
        for (int j = path.size() - 1; j > 0; --j)
            if (same(path[j].toMap(), path[j - 1].toMap())) path.removeAt(j);
        if (!front) std::reverse(path.begin(), path.end());
        return path;
    };
    const QVariantList originalWires = ctx.wires;
    for (int i = 0; i < ctx.wires.size(); ++i) {
        auto w = ctx.wires[i].toMap();
        if (w.value("wireType").toString() != "ortho") continue;
        auto path = w.value("path").toList();
        if (path.size() < 2) continue;
        const bool selected = movedIds.contains(w.value("id").toString());
        const bool front = movedPort(path.first().toMap());
        const bool back = movedPort(path.last().toMap());
        if (selected || (front && back)) {
            // 显式选线与双端随动共用一次平移，混合选择绝不重复变换。
            for (auto& pv : path) pv = translated(pv.toMap());
        } else if (front || back) {
            path = moveEnd(path, front);
        } else {
            continue;
        }
        w["path"] = path;
        // from/to 表示输出/输入而非 path 首尾，必须按实际端口几何验绑定。
        for (const QString prefix : {QStringLiteral("from"), QStringLiteral("to")}) {
            const QString key = prefix + "Comp";
            if (!w.contains(key)) continue;
            const int ci = ctx.indexOfComponent(w.value(key).toString());
            bool aligned = false;
            if (ci >= 0) {
                const auto c = ctx.components[ci].toMap();
                const auto ports = prefix == "from" ? ComponentLogic::outputPorts(c)
                                                     : ComponentLogic::inputPorts(c);
                const int pi = w.value(prefix + "Port").toInt();
                if (pi >= 0 && pi < ports.size()) {
                    const auto p = ports[pi].toMap();
                    const auto pos = Grid::point(Grid::xi(c) + Grid::xi(p), Grid::yi(c) + Grid::yi(p));
                    aligned = same(pos, path.first().toMap()) || same(pos, path.last().toMap());
                }
            }
            if (!aligned) { w.remove(key); w.remove(prefix + "Port"); }
        }
        ctx.wires[i] = w;
    }
    // 端口原本接在线段内部时保留固定主干，只补一条端口接续支线。
    // 不复制/移动整网，也不把未选中的主干折点拖走。
    for (const auto& p : oldPorts) {
        bool attached = false;
        for (const auto& wv : originalWires) {
            const auto w = wv.toMap();
            if (w.value("wireType").toString() != "ortho" || movedIds.contains(w.value("id").toString())) continue;
            const auto path = w.value("path").toList();
            if (path.size() < 2 || same(p, path.first().toMap()) || same(p, path.last().toMap())) continue;
            for (int j = 0; j + 1 < path.size(); ++j) {
                const auto a = path[j].toMap(), b = path[j + 1].toMap();
                if (Grid::onSegment(Grid::xi(p), Grid::yi(p), Grid::xi(a), Grid::yi(a), Grid::xi(b), Grid::yi(b))) {
                    attached = true;
                    break;
                }
            }
            if (attached) break;
        }
        const auto n = translated(p);
        for (int i = 0; attached && i < originalWires.size(); ++i) {
            const auto old = originalWires[i].toMap();
            const auto current = ctx.wires[i].toMap();
            if (old != current || current.value("wireType").toString() != "ortho") continue;
            // 沿主干移动时，新端口仍在线上，不生成重叠支线。
            if (Grid::onPath(Grid::xi(n), Grid::yi(n), current.value("path").toList())) attached = false;
        }
        if (!attached) continue;
        QVariantList path{p};
        if (sx != 0 && sy != 0) path.append(Grid::point(Grid::xi(n), Grid::yi(p)));
        path.append(n);
        ctx.wires.append(QVariantMap{{"id", IdAllocator::nextWireId(ctx)},
                                     {"wireType", "ortho"}, {"path", path}});
    }
}

// 绝对位置只取整；位移吸附 10px，保留旧元件所在的精确整数行列。
void Circuit::moveComponent(const QString& id, double x, double y) {
    if (m_viewOnly || !Grid::valid(x) || !Grid::valid(y)) return;
    const int idx = indexOfComponent(id);
    if (idx < 0) return;
    const auto c = currentCtx().components[idx].toMap();
    moveSelection({id}, x - Grid::xi(c), y - Grid::yi(c));
}

QSet<QString> Circuit::movableSelectionIds(const Context& ctx, const QStringList& ids) const {
    QSet<QString> moved;
    for (const auto& id : ids) {
        const int wi = ctx.indexOfWire(id);
        if (ctx.indexOfComponent(id) >= 0 ||
            (wi >= 0 && ctx.wires[wi].toMap().value("wireType").toString() == "ortho"))
            moved.insert(id);
    }
    return moved;
}

// 共享纯候选计算；调用者只在所有几何通过边界检查后提交。
bool Circuit::buildSelectionMove(Context& next, const QSet<QString>& moved, int sx, int sy) {
    if (sx == 0 && sy == 0) return true;
    const auto originalWires = next.wires;
    for (int i = 0; i < next.components.size(); ++i) {
        auto c = next.components[i].toMap();
        if (!moved.contains(c.value("id").toString())) continue;
        const int x = Grid::xi(c) + sx, y = Grid::yi(c) + sy;
        if (!Grid::valid(x) || !Grid::valid(y)) return false;
        c["x"] = x;
        c["y"] = y;
        next.components[i] = c;
    }
    updateWiresForMovedComponent(next, moved, sx, sy);
    for (int i = 0; i < next.wires.size(); ++i) {
        if (i < originalWires.size() && next.wires[i] == originalWires[i]) continue;
        for (const auto& pv : next.wires[i].toMap().value("path").toList()) {
            const auto p = pv.toMap();
            if (!Grid::valid(p.value("x").toDouble()) || !Grid::valid(p.value("y").toDouble())) return false;
        }
    }
    return true;
}

bool Circuit::beginSelectionMove(const QStringList& ids) {
    endSelectionMove();
    if (m_viewOnly) return false;
    const auto moved = movableSelectionIds(currentCtx(), ids);
    if (moved.isEmpty()) return false;
    m_selectionMove.active = true;
    m_selectionMove.ctxId = m_currentCtxId;
    m_selectionMove.start = currentCtx();
    m_selectionMove.ids = moved;
    return true;
}

bool Circuit::updateSelectionMove(double totalDx, double totalDy) {
    if (!m_selectionMove.active) return false;
    if (m_viewOnly || m_selectionMove.ctxId != m_currentCtxId) {
        endSelectionMove();
        return false;
    }
    if (!Grid::valid(totalDx) || !Grid::valid(totalDy)) return false;
    const int sx = Grid::snapStep(totalDx), sy = Grid::snapStep(totalDy);
    if (sx == m_selectionMove.dx && sy == m_selectionMove.dy) return true;
    Context next = m_selectionMove.start;
    if (!buildSelectionMove(next, m_selectionMove.ids, sx, sy)) return false;
    if (!m_selectionMove.undoPushed) {
        pushUndo();
        m_selectionMove.undoPushed = true;
    }
    m_selectionMove.dx = sx;
    m_selectionMove.dy = sy;
    currentCtx() = std::move(next);
    evaluateAll();
    emit geometryChanged();
    emit changed();
    emit structureChanged();
    return true;
}

void Circuit::endSelectionMove() {
    m_selectionMove = SelectionMoveState{};
}

// 非手势 API 保留相对移动和原有的撤销合并行为。
void Circuit::moveSelection(const QStringList& ids, double dx, double dy) {
    endSelectionMove();
    if (m_viewOnly || ids.isEmpty() || !Grid::valid(dx) || !Grid::valid(dy)) return;
    const int sx = Grid::snapStep(dx), sy = Grid::snapStep(dy);
    if (sx == 0 && sy == 0) return;
    Context& ctx = currentCtx();
    const auto moved = movableSelectionIds(ctx, ids);
    if (moved.isEmpty()) return;
    Context next = ctx;
    if (!buildSelectionMove(next, moved, sx, sy)) return;
    QStringList mergeIds(moved.begin(), moved.end());
    mergeIds.sort();
    pushUndo("moveSel:" + mergeIds.join(','));
    ctx = std::move(next);
    evaluateAll();
    emit geometryChanged();
    emit changed();
    emit structureChanged();
}

/// 语义化接口：带线移动单个元件
void Circuit::moveComponentWithWires(const QString& id, double x, double y) {
    // 当前 ortho 线随动已内置在 moveComponent 里，直接转调即可
    moveComponent(id, x, y);
}
// 语义化接口：带线批量相对移动选中元件
void Circuit::moveSelectionWithWires(const QStringList& ids, double dx, double dy) {
    // 当前 ortho 线随动已内置在 moveSelection 里，直接转调即可
    moveSelection(ids, dx, dy);
}

// 定义"切换输入元件值"接口：input 元件被点击时把所有位取反
void Circuit::toggleInput(const QString& id) {
    // 只读浏览模式禁止修改输入
    if (m_viewOnly) return;
    // 在当前上下文查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在则什么都不做
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 只有 input 类型元件支持点击切换，其它类型直接返回
    if (c.value("type").toString() != "input") return;
    // 取出输入位列表
    QVariantList bits = c.value("inputBits").toList();
    // 取出位宽，下限钳到 1
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 补齐位列表长度到位宽（防御历史数据不一致）
    while (bits.size() < bw) bits.append(false);
    // 超出位宽的多余位裁掉
    while (bits.size() > bw) bits.removeLast();
    // 取反第一位得到新值
    bool v = !bits[0].toBool();
    // 所有位统一设为新值（整字一起翻转）
    for (int i = 0; i < bits.size(); ++i) bits[i] = v;
    // 写回位列表
    c["inputBits"] = bits;

    // 同步 outputPorts 与 inputPortValues：即使未开始仿真，
    // input 元件自身的显示值（端口颜色、位方块）也立即响应；
    // 但下游元件保持不变，符合"未开始仿真不传播"的语义。
    // 同时提前算出 binStr，供后面 event 已运行分支复用，避免重复计算。
    QString binStr;
    binStr.reserve(bw);
    for (int i = 0; i < bw; ++i) binStr += v ? '1' : '0';
    // 输出端口值同步
    QVariantList outPorts;
    outPorts.append(binStr);
    c["outputPorts"] = outPorts;
    // 输入端口值同步（input 元件在引擎里也有 1 个输入端口，与输出同值）
    QVariantList inPortVals;
    inPortVals.append(binStr);
    c["inputPortValues"] = inPortVals;

    // 写回上下文
    ctx.components[idx] = c;

    // 事件引擎运行中：走增量触发路径
    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        // 若引擎脏（结构已变）需先重建
        if (m_engineDirty) {
            // 重建事件引擎
            rebuildEventEngine();
            // 取重建后的线延迟
            quint64 wd0 = m_eventEngine->wireDelay();
            // 取重建后的门延迟
            quint64 gd0 = m_eventEngine->globalGateDelay();
            // 跑完初始 Recalc 事件让状态收敛
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            // 把引擎输出回写到上下文
            applyEngineOutputsToContexts();
        }
        // 取该元件在引擎里对应的所有 qid（可能因层级展开有多个）
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        // 对每个 qid 触发输入串
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        // 取线延迟
        quint64 wd = m_eventEngine->wireDelay();
        // 取门延迟
        quint64 gd = m_eventEngine->globalGateDelay();
        // 跑够事件直到稳定
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        // 把引擎输出回写到上下文
        applyEngineOutputsToContexts();
        // 通知 QML 仿真推进
        emit simulationTick();
        // 事件引擎模式下但未运行：只标记脏，不传播
    } else if (m_engineType == "event") {
        m_engineDirty = true;
        // 关键修复：这里不再调用 IterEngine::evaluateAll。
        // 用户已明确要求："event 引擎未开始仿真不能传播"。input 自身的
        // outputPorts 已在上面同步为最新值（画布上 input 元件立即响应），
        // 但下游 output/LED/门 保持原值，直到 startSimulation 后事件队列
        // 真正推进。原实现调用 IterEngine::evaluateAll 会造成"未运行即
        // 传播"——即使只有一层 input→wire→output 也会立即把新值送到 output。
        // 非事件引擎：全量求值
    } else {
        evaluateAll();
    }
    // 通知 QML 数据变化
    emit changed();
    // 通知结构变化（端口值/网络刷新）
    emit structureChanged();
}

// 定义"设置输入/常量元件某一位的值"接口
void Circuit::setBitValue(const QString& id, int bit, bool value) {
    // 只读模式禁止修改
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 取类型
    QString t = c.value("type").toString();
    // 只有 input/const 有可编辑的位值，其它类型返回
    if (t != "input" && t != "const") return;
    // 取出位列表
    QVariantList bits = c.value("inputBits").toList();
    // 位宽下限钳到 1
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 补齐位列表长度
    while (bits.size() < bw) bits.append(false);
    // 裁掉多余位
    while (bits.size() > bw) bits.removeLast();
    // 位索引越界返回
    if (bit < 0 || bit >= bits.size()) return;
    // 值没变则直接返回，避免无意义操作
    if (bits[bit].toBool() == value) return;
    // 写入新位值
    bits[bit] = value;
    // 写回位列表
    c["inputBits"] = bits;

    // 同步 outputPorts 与 inputPortValues：无论是否在仿真中，
    // input/const 元件自身显示立即响应；未开始仿真时下游不传播。
    // 提前算出 binStr，供后面 event 已运行分支复用。
    QString binStr;
    binStr.reserve(bw);
    for (int i = 0; i < bw; ++i) binStr += bits[i].toBool() ? '1' : '0';
    // 输出端口值同步
    QVariantList outPorts;
    outPorts.append(binStr);
    c["outputPorts"] = outPorts;
    // 输入端口值同步
    QVariantList inPortVals;
    inPortVals.append(binStr);
    c["inputPortValues"] = inPortVals;

    // 写回上下文
    ctx.components[idx] = c;

    // 事件引擎运行中：走增量触发路径
    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        // 引擎脏则先重建并跑初始收敛
        if (m_engineDirty) {
            // 重建事件引擎
            rebuildEventEngine();
            // 取重建后的线延迟
            quint64 wd0 = m_eventEngine->wireDelay();
            // 取重建后的门延迟
            quint64 gd0 = m_eventEngine->globalGateDelay();
            // 跑完初始事件收敛
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            // 回写输出到上下文
            applyEngineOutputsToContexts();
        }
        // 取该元件对应的所有引擎 qid
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        // 对每个 qid 触发单个位的值变化
        for (const auto& qid : qids)
            m_eventEngine->triggerInputBit(qid, bit, value);
        // 取线延迟
        quint64 wd = m_eventEngine->wireDelay();
        // 取门延迟
        quint64 gd = m_eventEngine->globalGateDelay();
        // 跑够事件直到稳定
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        // 回写输出到上下文
        applyEngineOutputsToContexts();
        // 通知 QML 仿真推进
        emit simulationTick();
        // 事件引擎模式但未运行：只标记脏，不传播
    } else if (m_engineType == "event") {
        m_engineDirty = true;
        // 关键修复：删除 IterEngine::evaluateAll 调用。
        // 与 toggleInput 同理：event 未运行时不允许传播，input/const
        // 自身的 outputPorts 已更新，下游等到 startSimulation 后再更新。
        // 非事件引擎：全量求值
    } else {
        evaluateAll();
    }
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 定义"设置元件任意属性"接口：统一处理位宽/输入数/旋转等结构属性与普通属性
void Circuit::setComponentProp(const QString& id, const QString& key, const QVariant& value) {
    // 只读模式禁止修改
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 结构属性按 int 比较：值相同则无需改动
    if (key == "bitWidth" || key == "inputCount" || key == "rotation") {
        if (c.value(key).toInt() == value.toInt()) return;
        // 普通属性按 QVariant 直接比较
    } else {
        if (c.value(key) == value) return;
    }
    // 压撤销快照，合并键 "prop:id:key"，同一属性连续修改 500ms 内合并
    pushUndo("prop:" + id + ":" + key);
    // 标记本次修改是否属于结构变化（需发 structureChanged）
    bool structural = false;
    // 位宽修改分支
    if (key == "bitWidth") {
        // 取下限 1、上限 64 钳位后的位宽
        int bw = value.toInt();
        if (bw < 1) bw = 1;
        if (bw > 64) bw = 64;
        // 写回位宽
        c["bitWidth"] = bw;
        // 取出位列表
        QVariantList bits = c.value("inputBits").toList();
        // 位宽变大：在最高位侧补 0
        while (bits.size() < bw) bits.prepend(false);
        // 位宽变小：从最高位侧裁掉
        while (bits.size() > bw) bits.removeFirst();
        // 写回位列表
        c["inputBits"] = bits;
        // 取类型
        QString t = c.value("type").toString();
        // 分线器/汇线器还需检查分段总和是否仍等于位宽
        if (t == "splitter" || t == "hub") {
            // 取出分段列表
            QVariantList splits = c.value("outputSplits").toList();
            // 求分段总和
            int sum = 0;
            for (auto& sv : splits) sum += sv.toInt();
            // 总和不等于新位宽：重置为每段 1 位的默认分段
            if (sum != bw) {
                // 准备新分段列表
                QVariantList ns;
                // 预分配容量
                ns.reserve(bw);
                // 每段 1 位，共 bw 段
                for (int i = 0; i < bw; ++i) ns.append(1);
                // 写回分段
                c["outputSplits"] = ns;
            }
        }
        // 位宽变化属于结构变化
        structural = true;
        // 输入端口数修改分支
    } else if (key == "inputCount") {
        // 取钳位后的输入数（下限 2、上限 64）
        int v = value.toInt();
        if (v < 2) v = 2;
        if (v > 64) v = 64;
        // 写回输入数
        c[key] = v;
        // 端口数变化属于结构变化
        structural = true;
        // 旋转修改分支
    } else if (key == "rotation") {
        // 取旋转值并归一化到 0..3
        int v = value.toInt();
        v = ((v % 4) + 4) % 4;
        // 写回旋转值
        c["rotation"] = v;
        // 旋转影响端口几何，属于结构变化
        structural = true;
        // 普通属性：直接写入
    } else {
        c[key] = value;
    }
    // 写回上下文
    ctx.components[idx] = c;
    // 全量求值让逻辑/几何保持一致
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 结构属性变化时额外通知结构变化
    if (structural) emit structureChanged();
}

// 定义"旋转元件"接口：按 delta 步（每步 90°）旋转
void Circuit::rotateComponent(const QString& id, int delta) {
    // 只读模式禁止旋转
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 取当前旋转值
    int cur = c.value("rotation").toInt();
    // 计算归一化后的新旋转值
    int nw = ((cur + delta) % 4 + 4) % 4;
    // 旋转值没变则直接返回
    if (nw == cur) return;
    // 压撤销快照，合并键 "rot:id"
    pushUndo("rot:" + id);
    // 写回新旋转值
    c["rotation"] = nw;
    // 写回上下文
    ctx.components[idx] = c;
    // 通知数据变化
    emit changed();
    // 旋转改变端口几何，通知结构变化
    emit structureChanged();
}

// 定义"重命名元件"接口
void Circuit::renameComponent(const QString& id, const QString& name) {
    // 只读模式禁止重命名
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 名称未变则直接返回
    if (c.value("name").toString() == name) return;
    // 压撤销快照，合并键 "rename:id"
    pushUndo("rename:" + id);
    // 写入新名称
    c["name"] = name;
    // 写回上下文
    ctx.components[idx] = c;
    // 通知数据变化
    emit changed();
}

// 定义"设置分线器/汇线器分段方案"接口：输入形如 "1,2,3" 的分段字符串
void Circuit::setSplitterSplits(const QString& id, const QString& splitsStr) {
    // 只读模式禁止修改
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 取类型
    QString t = c.value("type").toString();
    // 只有分线器/汇线器支持分段设置
    if (t != "splitter" && t != "hub") return;
    // 取位宽，下限钳到 1
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 复制输入字符串，便于统一分隔符
    QString s = splitsStr;
    // 逗号统一替换成空格
    s.replace(',', ' ');
    // 按空白切分成各段
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    // 准备解析后的分段列表
    QVariantList splits;
    // 记录分段总和
    int sum = 0;
    // 逐个解析分段
    for (auto& p : parts) {
        // 转换结果标记
        bool ok = false;
        // 转成整数
        int v = p.toInt(&ok);
        // 非法或小于 1 的段直接跳过
        if (!ok || v < 1) continue;
        // 加入分段列表
        splits.append(v); sum += v;
    }
    // 总和必须等于位宽且至少一段，否则视为非法输入不生效
    if (sum != bw || splits.isEmpty()) return;
    // 分段方案未变则直接返回
    if (c.value("outputSplits").toList() == splits) return;
    // 压撤销快照，合并键 "splits:id"
    pushUndo("splits:" + id);
    // 写回分段列表
    c["outputSplits"] = splits;
    // 写回上下文
    ctx.components[idx] = c;
    // 全量求值让端口值按新分段重算
    evaluateAll();
    // 通知数据变化
    emit changed();
    // 分段方案改变端口结构，通知结构变化
    emit structureChanged();
}

// 定义"读取分线器/汇线器分段方案字符串"接口
QString Circuit::getSplitterSplitsStr(const QString& id) const {
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在返回空串
    if (idx < 0) return {};
    // 取出元件 map
    QVariantMap c = currentCtx().components[idx].toMap();
    // 取类型
    QString t = c.value("type").toString();
    // 非分线器/汇线器返回空串
    if (t != "splitter" && t != "hub") return {};
    // 取出分段列表
    QVariantList splits = c.value("outputSplits").toList();
    // 准备字符串片段列表
    QStringList parts;
    // 逐段转成数字字符串
    for (auto& v : splits) parts.append(QString::number(v.toInt()));
    // 用逗号拼接返回
    return parts.join(",");
}

// 定义"添加文本元件"接口：先建 text 元件再设置内容
QString Circuit::addText(double x, double y, const QString& text) {
    // 通过通用添加接口创建 text 元件
    QString id = addComponent("text", x, y, 1, 2, "");
    // 创建成功则立刻写入文本内容
    if (!id.isEmpty()) setTextContent(id, text);
    // 返回新元件 id（失败为空串）
    return id;
}

// 定义"设置文本元件内容"接口
void Circuit::setTextContent(const QString& id, const QString& content) {
    // 只读模式禁止修改
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 只有 text 类型支持设置内容
    if (c.value("type").toString() != "text") return;
    // 内容未变则直接返回
    if (c.value("content").toString() == content) return;
    // 压撤销快照，合并键 "text:id"
    pushUndo("text:" + id);
    // 写入新内容
    c["content"] = content;
    // 写回上下文
    ctx.components[idx] = c;
    // 通知数据变化
    emit changed();
}