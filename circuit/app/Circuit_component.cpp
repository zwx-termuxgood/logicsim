
// 引入 Circuit 类声明：本文件要定义它的成员函数，必须看到完整类定义（成员、私有函数、Context 别名等）
#include "../Circuit.h"
// 引入 ID 分配器：addComponent 里给新元件分配唯一 id、addOrthoWire 等给连线分配 id 都要用它
#include "../core/IdAllocator.h"
// 引入元件逻辑库：查询元件端口几何（outputPorts/inputPorts）以及端口数量都委托给它，保证端口规则唯一来源
#include "../logic/ComponentLogic.h"
// 引入事件引擎：仿真运行时需要 rebuildEventEngine、triggerInputString/Bit、run 等接口
#include "../logic/event/EventEngine.h"
// 引入移动手势类型：本文件包含移动相关接口，可能间接使用该头里的类型/常量
#include "../logic/move/MoveGesture.h"
// 引入移动结果结构：移动操作可能返回/使用该结构描述位移结果
#include "../logic/move/MoveResult.h"
// 引入 QVariantMap：每个元件/连线都以 map 表示，用字符串 key 存取字段
#include <QVariantMap>
// 引入 QSet：用于去重集合，如删除元件时收集端口位置、批量删除时收集 id
#include <QSet>
// 引入 QString：元件 id、类型、名称、二进制串等都用它
#include <QString>
// 引入 Qt 全局定义：qAbs、qMax、qBound 等工具函数在此声明
#include <QtGlobal>
// 引入 QDebug：调试输出用（本文件保留 include 以便后续加日志）
#include <QDebug>
// 引入 QPointF：计算端口新坐标、返回浮点二维点时使用
#include <QPointF>
// 引入 QList：QList<QPointF> 收集端口坐标点用
#include <QList>
// 引入 C 数学库：std::round 用于端口位置取整做键、比较坐标是否相等
#include <cmath>

// 匿名命名空间：内部辅助函数只在本编译单元可见，避免符号污染和链接冲突
namespace {

// 定义"把浮点坐标取整后拼成字符串键"的辅助函数，供"同一端口位置"比较使用
QString posKeyR(double x, double y) {
    // 先对 x、y 四舍五入取整再拼成 "x,y"；取整是为了容忍浮点误差，让同一端口位置得到同一个键
    return QString("%1,%2").arg(int(std::round(x))).arg(int(std::round(y)));
}

// 定义"收集某元件所有端口的世界坐标到集合"的辅助函数；世界坐标 = 元件坐标 + 端口偏移
void collectPortWorldPositions(const QVariantMap& c, QSet<QString>& out) {
    // 取出元件自身 x 坐标，作为世界坐标的基准
    double cx = c.value("x").toDouble();
    // 取出元件自身 y 坐标，作为世界坐标的基准
    double cy = c.value("y").toDouble();
    // 向 ComponentLogic 查询该元件的所有输出端口（含相对偏移 x/y），保证端口规则唯一来源
    QVariantList outs = ComponentLogic::outputPorts(c);
    // 遍历每个输出端口
    for (const auto& pv : outs) {
        // 端口以 QVariantMap 表示，转成 map 便于按 key 取字段
        QVariantMap p = pv.toMap();
        // 端口世界 x = 元件 x + 端口相对 x，取整拼键后插入集合；世界 y 同理，与 x 一起构成唯一位置键
        out.insert(posKeyR(cx + p.value("x").toDouble(),
                           cy + p.value("y").toDouble()));
    }
    // 向 ComponentLogic 查询该元件的所有输入端口
    QVariantList ins = ComponentLogic::inputPorts(c);
    // 遍历每个输入端口
    for (const auto& pv : ins) {
        // 端口转 map
        QVariantMap p = pv.toMap();
        // 输入端口世界 x 同样由元件坐标加偏移得到；世界 y 同理
        out.insert(posKeyR(cx + p.value("x").toDouble(),
                           cy + p.value("y").toDouble()));
    }
}

// 定义"判断一条 ortho 线的 path 顶点是否与给定端口位置集合重合（容差 15px）"的辅助函数
bool wireTouchesPositions(const QVariantMap& w, const QSet<QString>& positions) {
    // 取出线型字段
    QString wt = w.value("wireType").toString();
    // 只有 ortho 线才有显式 path 顶点可比较；bezier 线几何由两端实时算出，这里不处理，直接返回不命中
    if (wt != "ortho") return false;
    // 取出 ortho 线的顶点列表
    QVariantList path = w.value("path").toList();
    // 没有 path 就不可能碰到端口，直接返回 false
    if (path.isEmpty()) return false;
    //
    // 准备一个浮点二维点列表，把 positions 里的字符串键还原成坐标点
    QList<QPointF> portPts;
    // 预分配容量，避免循环中反复扩容
    portPts.reserve(positions.size());
    // 遍历所有端口位置键
    for (const auto& key : positions) {
        // 键格式是 "x,y"，用逗号切成两段
        QStringList parts = key.split(',');
        // 格式不对（理论上不会）则跳过，防御性编程
        if (parts.size() != 2) continue;
        // 把两段转成 double 组成 QPointF 加入列表
        portPts.append(QPointF(parts[0].toDouble(), parts[1].toDouble()));
    }
    // 没有有效端口点则不可能命中
    if (portPts.isEmpty()) return false;
    //
    // 遍历 ortho 线的每个顶点
    for (const auto& pv : path) {
        // 顶点转 map
        QVariantMap p = pv.toMap();
        // 取出顶点 x
        double px = p.value("x").toDouble();
        // 取出顶点 y
        double py = p.value("y").toDouble();
        // 对每个端口点做比较
        for (const auto& pp : portPts) {
            // 横纵坐标都在 15px 容差内就认为"线顶到了端口"（T 型接入/端点重合）
            if (qAbs(px - pp.x()) < 15.0 && qAbs(py - pp.y()) < 15.0) {
                // 命中任意一个端口即返回 true
                return true;
            }
        }
    }
    // 所有顶点都没碰到端口，返回 false
    return false;
}

// 匿名命名空间结束：以上辅助函数仅本文件可用
}

// 定义"添加元件"接口：类型、位置、位宽、输入数、子电路 id（仅 sub 类型用）；参数续行：位宽、输入端口数、子电路 id
QString Circuit::addComponent(const QString& type, double x, double y,
                              int bitWidth, int inputCount, const QString& subId) {
    // 浏览子电路（只读）模式下禁止编辑，直接返回空 id 表示失败
    if (m_viewOnly) return QString();
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
    c["x"] = x; c["y"] = y;
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
    // 取出待删元件 map，后面要用它的端口位置判断相关连线
    QVariantMap c = ctx.components[idx].toMap();
    //
    // 准备集合：收集该元件所有端口的世界坐标键
    QSet<QString> portPositions;
    // 填充集合，用于判断哪些 ortho 线的顶点顶在这些端口上（T 型接入）
    collectPortWorldPositions(c, portPositions);
    //
    // 如果删的是时钟元件，同步清掉它的输出状态记录，避免残留
    if (c.value("type").toString() == "clock") m_clockStates.remove(id);
    // 从元件列表移除该元件
    ctx.components.removeAt(idx);
    // 子电路实例列表可能变化（删的可能是 sub 实例），缓存失效
    m_ctxSubInstCache.clear();
    //
    // 倒序遍历连线：倒序删除不会打乱未遍历索引
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        // 取出连线 map
        auto w = ctx.wires[i].toMap();
        // 标记这条线是否要删
        bool shouldRemove = false;
        // 连线起点就是被删元件
        if (w.value("fromComp").toString() == id ||
            // 或者连线终点就是被删元件
            w.value("toComp").toString() == id) {
            // 两端任一是被删元件，这条线必须删
            shouldRemove = true;
            // 否则检查 ortho 线顶点是否顶在被删元件的端口上（T 型接入）
        } else if (wireTouchesPositions(w, portPositions)) {
            // 是则也删，避免留下悬空 T 型线头
            shouldRemove = true;
        }
        // 满足条件就从连线列表移除
        if (shouldRemove) ctx.wires.removeAt(i);
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
    // 一个都不存在就没必要压撤销/改数据
    if (!any) return;
    // 压撤销快照（整批一次快照，撤销时整批恢复）
    pushUndo();
    // 取当前上下文
    Context& ctx = currentCtx();
    //
    // 收集所有被删元件的端口世界坐标
    QSet<QString> portPositions;
    // 倒序遍历元件，便于原地删除
    for (int i = ctx.components.size() - 1; i >= 0; --i) {
        // 取出元件 map
        QVariantMap c = ctx.components[i].toMap();
        // 不在删除集合里则跳过
        if (!idSet.contains(c.value("id").toString())) continue;
        // 在删除前先收集它的端口位置，供后面清理 T 型线
        collectPortWorldPositions(c, portPositions);
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
    // 倒序遍历连线
    for (int i = ctx.wires.size() - 1; i >= 0; --i) {
        // 取出连线 map
        auto w = ctx.wires[i].toMap();
        // 是否删除标记
        bool shouldRemove = false;
        // 起点在被删集合里
        if (idSet.contains(w.value("fromComp").toString()) ||
            // 或终点在被删集合里
            idSet.contains(w.value("toComp").toString())) {
            // 端点被删则删线
            shouldRemove = true;
            // 否则检查是否顶在被删元件的端口上
        } else if (wireTouchesPositions(w, portPositions)) {
            // 是则删，清理 T 型接入
            shouldRemove = true;
        }
        // 满足则移除
        if (shouldRemove) ctx.wires.removeAt(i);
    }
    //
    // 重新全量求值
    evaluateAll();
    // 通知 QML 重绘
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 定义"元件移动后修正其相连 ortho 线端点/相邻拐点"的辅助函数
void Circuit::updateWiresForMovedComponent(Context& ctx, const QString& movedId) {
    // 遍历当前上下文所有连线
    for (int i = 0; i < ctx.wires.size(); ++i) {
        // 取出连线 map
        QVariantMap w = ctx.wires[i].toMap();
        // 只有 ortho 线有显式 path 需要随动；bezier 两端实时算，无需修正
        if (w.value("wireType").toString() != "ortho") continue;
        // 连线起点元件 id
        QString fromComp = w.value("fromComp").toString();
        // 连线终点元件 id
        QString toComp = w.value("toComp").toString();
        // 起点是否就是被移动的元件
        bool fromIsMoved = (fromComp == movedId);
        // 终点是否就是被移动的元件
        bool toIsMoved = (toComp == movedId);
        // 两端都不是被移动元件，这条线不用管
        if (!fromIsMoved && !toIsMoved) continue;
        //
        // 取出 ortho 线顶点列表
        QVariantList path = w.value("path").toList();
        // 少于两个顶点不是有效线，跳过
        if (path.size() < 2) continue;
        //
        // 内联 lambda：求某元件某端口移动后的新世界坐标
        auto getNewPortPos = [&](const QString& compId, int port, bool isOut) -> QPointF {
            // 按 id 查元件索引
            int ci = ctx.indexOfComponent(compId);
            // 元件不存在（可能刚被删）返回空点
            if (ci < 0) return QPointF();
            // 取出元件 map
            QVariantMap c = ctx.components[ci].toMap();
            // 按输出/输入取端口几何列表
            QVariantList ports = isOut ? ComponentLogic::outputPorts(c) : ComponentLogic::inputPorts(c);
            // 端口索引越界返回空点
            if (port < 0 || port >= ports.size()) return QPointF();
            // 取出该端口相对偏移
            QVariantMap p = ports[port].toMap();
            // 新世界 x = 元件新 x + 端口偏移 x；新世界 y = 元件新 y + 端口偏移 y
            return QPointF(c.value("x").toDouble() + p.value("x").toDouble(),
                           c.value("y").toDouble() + p.value("y").toDouble());
        };
        //
        // 如果移动的是起点元件
        if (fromIsMoved) {
            // 取出起点端口索引
            int fp = w.value("fromPort").toInt();
            // 求该输出端口移动后的新坐标
            QPointF np = getNewPortPos(movedId, fp, true);
            // 取出旧的首顶点
            QVariantMap oldP0 = path[0].toMap();
            // 记录旧首顶点 x，用于判断与第二点之间原本是横还是竖
            double oldX = oldP0.value("x").toDouble();
            // 准备新的首顶点
            QVariantMap newP0;
            // 首顶点更新到新端口坐标
            newP0["x"] = np.x(); newP0["y"] = np.y();
            // 写回 path[0]
            path[0] = newP0;
            //
            // 若还有第二点
            if (path.size() >= 2) {
                // 取出第二点
                QVariantMap p1 = path[1].toMap();
                // 取第二点 x
                double p1x = p1.value("x").toDouble();
                // 首段原本是否竖直（第二点 x 与旧首点 x 几乎相等即竖直）
                bool wasVertical = (qAbs(p1x - oldX) < 0.5);
                // 原本竖直：保持竖直，第二点 x 跟着新首点 x 走
                if (wasVertical) p1["x"] = np.x();
                // 原本水平：保持水平，第二点 y 跟着新首点 y 走
                else p1["y"] = np.y();
                // 写回 path[1]
                path[1] = p1;
            }
        }
        //
        // 如果移动的是终点元件
        if (toIsMoved) {
            // 取出终点端口索引
            int tp = w.value("toPort").toInt();
            // 求该输入端口移动后的新坐标
            QPointF np = getNewPortPos(movedId, tp, false);
            // 顶点总数
            int n = path.size();
            // 取出旧的末顶点
            QVariantMap oldPn = path[n-1].toMap();
            // 记录旧末顶点 x，用于判断与倒数第二点之间原本是横还是竖
            double oldX = oldPn.value("x").toDouble();
            // 准备新的末顶点
            QVariantMap newPn;
            // 末顶点更新到新端口坐标
            newPn["x"] = np.x(); newPn["y"] = np.y();
            // 写回 path[n-1]
            path[n-1] = newPn;
            //
            // 若还有倒数第二点
            if (n >= 2) {
                // 取出倒数第二点
                QVariantMap pn1 = path[n-2].toMap();
                // 取倒数第二点 x
                double pn1x = pn1.value("x").toDouble();
                // 末段原本是否竖直
                bool wasVertical = (qAbs(pn1x - oldX) < 0.5);
                // 原本竖直：倒数第二点 x 跟着新末点 x
                if (wasVertical) pn1["x"] = np.x();
                // 原本水平：倒数第二点 y 跟着新末点 y
                else pn1["y"] = np.y();
                // 写回 path[n-2]
                path[n-2] = pn1;
            }
        }
        //
        // 把修正后的 path 写回连线 map
        w["path"] = path;
        // 把连线 map 写回上下文
        ctx.wires[i] = w;
    }
}

// 定义"把元件移动到绝对坐标"接口
void Circuit::moveComponent(const QString& id, double x, double y) {
    // 只读模式禁止移动
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 记录旧坐标
    double ox = c.value("x").toDouble(), oy = c.value("y").toDouble();
    // 新旧坐标几乎相同就当作没移动，避免无意义的撤销入栈和求值
    if (qAbs(ox - x) < 1e-9 && qAbs(oy - y) < 1e-9) return;
    // 压撤销快照，合并键 "move:元件id"：对同一元件连续拖动在 500ms 内会合并成一次撤销
    pushUndo("move:" + id);
    // 更新元件坐标
    c["x"] = x; c["y"] = y;
    // 写回上下文
    ctx.components[idx] = c;
    // 修正与该元件相连的 ortho 线 path，保持连线跟随
    updateWiresForMovedComponent(ctx, id);
    // 位置可能影响几何但不影响逻辑值；这里仍调求值保持一致（也同步子电路 pins）
    evaluateAll();
    // 通知只关心几何的 QML 组件（可只重算布局）
    emit geometryChanged();
    // 通知数据变化，需要重绘
    emit changed();
    // 连线的端口索引/几何结构也可能变了，发结构信号让 QML 重建网络
    emit structureChanged();
}

// 定义"批量相对移动"接口
void Circuit::moveSelection(const QStringList& ids, double dx, double dy) {
    // 只读模式禁止移动
    if (m_viewOnly) return;
    // 空选择直接返回
    if (ids.isEmpty()) return;
    // 位移几乎为 0 直接返回，避免无意义操作
    if (qAbs(dx) < 1e-9 && qAbs(dy) < 1e-9) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 选择集合，便于 O(1) 判断
    QSet<QString> idSet;
    // 填充集合
    for (const auto& id : ids) idSet.insert(id);
    // 标记是否至少命中一个真实元件
    bool any = false;
    // 遍历元件
    for (int i = 0; i < ctx.components.size(); ++i) {
        // 命中即提前退出
        if (idSet.contains(ctx.components[i].toMap().value("id").toString())) { any = true; break; }
    }
    // 一个都不存在则不做任何事
    if (!any) return;
    // 压撤销快照，合并键 "moveSel"：连续拖动整组在 500ms 内合并为一次撤销
    pushUndo("moveSel");
    // 遍历元件
    for (int i = 0; i < ctx.components.size(); ++i) {
        // 取出元件 map
        QVariantMap c = ctx.components[i].toMap();
        // 只处理被选中的元件
        if (idSet.contains(c.value("id").toString())) {
            // x 加相对位移
            c["x"] = c.value("x").toDouble() + dx;
            // y 加相对位移
            c["y"] = c.value("y").toDouble() + dy;
            // 写回上下文
            ctx.components[i] = c;
        }
    }
    // 遍历被选 id（只为修正连线）
    for (const auto& id : ids) {
        // 元件仍存在才需要修正
        if (ctx.indexOfComponent(id) >= 0) {
            // 逐个修正与该元件相连的 ortho 线
            updateWiresForMovedComponent(ctx, id);
        }
    }
    // 重新全量求值保持一致
    evaluateAll();
    // 通知几何变化
    emit geometryChanged();
    // 通知数据变化
    emit changed();
    // 通知结构变化
    emit structureChanged();
}

// 语义化接口：带线移动单个元件
void Circuit::moveComponentWithWires(const QString& id, double x, double y) {
    // 当前 ortho 线随动已内置在 moveComponent 里，直接转调即可；保留此接口是为了 QML 语义清晰
    moveComponent(id, x, y);
}
void Circuit::moveSelectionWithWires(const QStringList& ids, double dx, double dy) {
    moveSelection(ids, dx, dy);
}

// ★ 修复：rebuild 之后必须先跑完初始 Recalc 事件，再 apply
void Circuit::toggleInput(const QString& id) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "input") return;
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    bool v = !bits[0].toBool();
    for (int i = 0; i < bits.size(); ++i) bits[i] = v;
    c["inputBits"] = bits;
    ctx.components[idx] = c;

    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        if (m_engineDirty) {
            rebuildEventEngine();
            quint64 wd0 = m_eventEngine->wireDelay();
            quint64 gd0 = m_eventEngine->globalGateDelay();
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            applyEngineOutputsToContexts();
        }
        QString binStr;
        binStr.reserve(bw);
        for (int i = 0; i < bw; ++i) binStr += v ? '1' : '0';
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else if (m_engineType == "event") {
        m_engineDirty = true;
    } else {
        evaluateAll();
    }
    emit changed();
    emit structureChanged();
}

void Circuit::setBitValue(const QString& id, int bit, bool value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "input" && t != "const") return;
    QVariantList bits = c.value("inputBits").toList();
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    while (bits.size() < bw) bits.append(false);
    while (bits.size() > bw) bits.removeLast();
    if (bit < 0 || bit >= bits.size()) return;
    if (bits[bit].toBool() == value) return;
    bits[bit] = value;
    c["inputBits"] = bits;
    ctx.components[idx] = c;

    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        if (m_engineDirty) {
            rebuildEventEngine();
            quint64 wd0 = m_eventEngine->wireDelay();
            quint64 gd0 = m_eventEngine->globalGateDelay();
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            applyEngineOutputsToContexts();
        }
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        for (const auto& qid : qids)
            m_eventEngine->triggerInputBit(qid, bit, value);
        quint64 wd = m_eventEngine->wireDelay();
        quint64 gd = m_eventEngine->globalGateDelay();
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        applyEngineOutputsToContexts();
        emit simulationTick();
    } else if (m_engineType == "event") {
        m_engineDirty = true;
    } else {
        evaluateAll();
    }
    emit changed();
    emit structureChanged();
}

void Circuit::setComponentProp(const QString& id, const QString& key, const QVariant& value) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (key == "bitWidth" || key == "inputCount" || key == "rotation") {
        if (c.value(key).toInt() == value.toInt()) return;
    } else {
        if (c.value(key) == value) return;
    }
    pushUndo("prop:" + id + ":" + key);
    bool structural = false;
    if (key == "bitWidth") {
        int bw = value.toInt();
        if (bw < 1) bw = 1;
        if (bw > 64) bw = 64;
        c["bitWidth"] = bw;
        QVariantList bits = c.value("inputBits").toList();
        while (bits.size() < bw) bits.prepend(false);
        while (bits.size() > bw) bits.removeFirst();
        c["inputBits"] = bits;
        QString t = c.value("type").toString();
        if (t == "splitter" || t == "hub") {
            QVariantList splits = c.value("outputSplits").toList();
            int sum = 0;
            for (auto& sv : splits) sum += sv.toInt();
            if (sum != bw) {
                QVariantList ns;
                ns.reserve(bw);
                for (int i = 0; i < bw; ++i) ns.append(1);
                c["outputSplits"] = ns;
            }
        }
        structural = true;
    } else if (key == "inputCount") {
        int v = value.toInt();
        if (v < 2) v = 2;
        if (v > 64) v = 64;
        c[key] = v;
        structural = true;
    } else if (key == "rotation") {
        int v = value.toInt();
        v = ((v % 4) + 4) % 4;
        c["rotation"] = v;
        structural = true;
    } else {
        c[key] = value;
    }
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
    if (structural) emit structureChanged();
}

void Circuit::rotateComponent(const QString& id, int delta) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    int cur = c.value("rotation").toInt();
    int nw = ((cur + delta) % 4 + 4) % 4;
    if (nw == cur) return;
    pushUndo("rot:" + id);
    c["rotation"] = nw;
    ctx.components[idx] = c;
    emit changed();
    emit structureChanged();
}

void Circuit::renameComponent(const QString& id, const QString& name) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("name").toString() == name) return;
    pushUndo("rename:" + id);
    c["name"] = name;
    ctx.components[idx] = c;
    emit changed();
}

void Circuit::setSplitterSplits(const QString& id, const QString& splitsStr) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return;
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    QString s = splitsStr;
    s.replace(',', ' ');
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    QVariantList splits;
    int sum = 0;
    for (auto& p : parts) {
        bool ok = false;
        int v = p.toInt(&ok);
        if (!ok || v < 1) continue;
        splits.append(v); sum += v;
    }
    if (sum != bw || splits.isEmpty()) return;
    if (c.value("outputSplits").toList() == splits) return;
    pushUndo("splits:" + id);
    c["outputSplits"] = splits;
    ctx.components[idx] = c;
    evaluateAll();
    emit changed();
    emit structureChanged();
}

QString Circuit::getSplitterSplitsStr(const QString& id) const {
    int idx = indexOfComponent(id);
    if (idx < 0) return {};
    QVariantMap c = currentCtx().components[idx].toMap();
    QString t = c.value("type").toString();
    if (t != "splitter" && t != "hub") return {};
    QVariantList splits = c.value("outputSplits").toList();
    QStringList parts;
    for (auto& v : splits) parts.append(QString::number(v.toInt()));
    return parts.join(",");
}

QString Circuit::addText(double x, double y, const QString& text) {
    QString id = addComponent("text", x, y, 1, 2, "");
    if (!id.isEmpty()) setTextContent(id, text);
    return id;
}

void Circuit::setTextContent(const QString& id, const QString& content) {
    if (m_viewOnly) return;
    int idx = indexOfComponent(id);
    if (idx < 0) return;
    Context& ctx = currentCtx();
    QVariantMap c = ctx.components[idx].toMap();
    if (c.value("type").toString() != "text") return;
    if (c.value("content").toString() == content) return;
    pushUndo("text:" + id);
    c["content"] = content;
    ctx.components[idx] = c;
    emit changed();
}