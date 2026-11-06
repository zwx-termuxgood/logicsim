#include "model/Circuit.h"
// 包含 Circuit 自身的类声明（src/Circuit.h），本文件所有函数签名、成员变量都必须先有完整声明才能定义
// .cpp 的第一件事永远是包含自己所属类的头文件：成员访问、Qt moc 生成代码都依赖它

#include "core/Component.h"
// 引入元件基础层 Component 的定义，提供元件类型判断等工具
// 本文件处理"元器件 map"的通用逻辑（updateOneComponent 要按 type 区分 input/output/led/sub）时依赖该层

#include "core/ValueCodec.h"
// 引入数值编解码器：二进制串与各进制字符串互转
// 虽然本文件没有直接调用 ValueCodec 的函数，但电路值的表示（QVariantList 位列表 / QString 二进制串）由它定义，
// JsonCodec 等下游层依赖它，纳入本 TU 可保证类型与接口一致


#include "logic/ComponentLogic.h"
// 引入元件逻辑层：outputBitWidth / inputBitWidth / outputPorts / inputPorts 等按类型计算端口几何与位宽的纯函数
// refreshErrors 要判断"源端口位宽 vs 目标端口位宽"，位宽不是存在 map 里的固定字段，而是由元件类型与配置推算出来的，
// 必须走这一层

#include "logic/WireNetworkSolver.h"
// 引入线网求解器：generateVirtualWires 从既有导线推导出"虚拟连接"，connectionDots 计算三通接头圆点
// 事件引擎要的是完整的连接关系，而 ortho 线只记录两端点坐标、bezier 线只记录端口索引，
// 靠 generateVirtualWires 把两者统一补全后再交给引擎


#include "engine/iter/IterEngine.h"
// 引入迭代引擎：evaluateAll / evaluateStandalone / syncSubPins 三个静态入口
// "iter" 引擎模式（以及子电路浏览时的局部求值）全靠它；syncSubPins 还负责把子电路定义的端口方向同步给所有实例


#include "engine/event/EventEngine.h"
// 引入事件驱动引擎 EventEngine 的完整定义（build / run / step / isIdle / getOutputs / triggerInputString / takeDirtyQids 等）
// Circuit.h 里只有 class EventEngine 的前置声明，一旦在本文件里调用它的成员函数就必须看到完整定义；
// 事件引擎是本包默认引擎，本文件大量与之交互


#include "io/JsonCodec.h"
// 引入序列化编解码器：encode 把上下文树编码为 QVariantMap，decode 反向还原为 DecodedData
// 本文件负责 toJson/fromJson/saveToFile/loadFromFile/importFromFile，Circuit 自身不关心 JSON 字段格式，
// 全部委托这一层，保证"文件格式"只有一处定义


#include "io/RecentStore.h"
// 引入最近文件持久化：load()/save() 读写 QSettings
// "最近打开文件"属于 UI 偏好而非电路数据，独立存储，Circuit 只经 RecentStore 访问


#include <QJsonDocument>
// Qt 提供的 JSON 文档类型：fromJson（解析）与 toJson（带缩进输出）
// 保存时要输出人类可读、可 diff 的 Indented JSON；加载时要先解析成 QJsonDocument 再转 QVariantMap 交给 JsonCodec


#include <QJsonObject>
// Qt 的 JSON 对象类型，可容纳键值对并从 QVariantMap 构造
// QJsonDocument 只能包裹数组或对象；toJson 返回的是 QVariantMap，需要 QJsonObject::fromVariantMap 转换后才能写入文档


#include <QFile>
// QFile：跨平台文件读写（含 Android 的 content:// URI 兼容路径）
// saveToFile/loadFromFile/importFromFile 都用它


#include <QSaveFile>
// QSaveFile：原子写入（先写临时文件，commit 成功才替换原文件）
// 本文件实际没有使用 QSaveFile——saveToFile 采用"直接 WriteOnly|Truncate"，这是下一轮可以改进的健壮性点，见 saveToFile 处注释


#include <QFileInfo>
// QFileInfo：提取路径中的文件名 / 去扩展名基名 / 是否存在
// 导入电路时要用"文件名（不含扩展名）"给导入后的子电路命名；最近文件列表里也要显示纯文件名而不是完整路径


#include <QUrl>
// QUrl：URI 解析，toLocalFile() 把 file:///... 转成本地路径
// QML 文件对话框返回的路径在部分平台（尤其 Android）是 file:// URI，直接交给 QFile 会打不开，必须先转换


#include <QDir>
// QDir：目录相关工具类
// 本文件未直接使用 QDir（保留包含，属历史遗留或间接依赖），后续可清理，但不改原代码以保持逐字节对照


#include <QDateTime>
// QDateTime：当前时间、时间差计算、格式化输出
// 撤销合并要判断"距上一次入栈是否不到 500ms"，最近文件要记录"最后打开时间"


#include <QDebug>
// qDebug() 调试输出流
// 本文件已无 qDebug 调用（调试输出已移除），保留包含以便后续排障时直接打日志


// ============================================================================
// ① 列表查询
// ============================================================================

QVariantList Circuit::subcircuits() const {
    // 定义 Circuit::subcircuits() 的 const 实现：返回全部子电路的摘要列表，供 QML 侧边栏展示
    // QML 侧不应该知道 m_subContexts（QHash<QString,Context>）的内部结构，这里把它"翻译"成 QVariantList 每个元素是 QVariantMap；
    //  const 是因为纯查询、不修改任何成员，也据此保证内部缓存（m_ctxSubInstCache 是 mutable）以外不产生副作用


    QVariantList out;
    // 创建输出列表 out，最终作为返回值返回
    // 先把所有子电路的摘要攒起来再一次性 return，避免边遍历边 return；QVariantList 每个元素是 QVariantMap


    for (auto it = m_subNames.begin(); it != m_subNames.end(); ++it) {
        // 遍历 m_subNames（QHash<QString,QString>：子电路 ID -> 显示名），it.key() 是 ID，it.value() 是名字
        // 子电路的"存在性"由 m_subNames 定义（m_subContexts 才是内容），按名字表遍历能保证列表里每个条目都有显示名；
        //  QHash 遍历顺序不确定，这是可接受的：UI 侧不需要稳定顺序，需要稳定顺序的地方见 editContexts 里先排序

        QVariantMap m;
        // 为当前子电路创建一条摘要记录 m
        // 用 QVariantMap 而不是自定义结构体：QML 可以直接读 map 的 key，不需要注册元类型


        m["id"] = it.key();
        // 写入子电路 ID（如 "S3"），QML 点选后用它去调用 enterSubcircuit/deleteSubcircuit
        // ID 必须随条目一起返回，否则 UI 无法回指具体子电路


        m["name"] = it.value();
        // 写入显示名
        // 侧边栏直接显示，省去 QML 再调用 getSubcircuitName 的往返

        const Context* ctx = ctxById(it.key());
        // 按 ID 取子电路上下文指针（const 版）：ctxById 对空串返回 &m_root，对不存在的 ID 返回 nullptr
        // 用指针做存在性判断，再解引用统计元件；用 const 指针因为这里只读不改

        int inCount = 0, outCount = 0;
        // 声明两个计数器：输入元件数与输出元件数，初值 0
        // "子电路有几个输入/输出端口"直接决定了实例上要画几个接线柱，UI 需要这个数字；
        //  初值 0 保证即便上下文缺失（ctx 为空）也能返回合法的 0/0


        if (ctx) {
            // 【分支】只有上下文存在才统计
            // 防御空指针：ID 与名称表不一致（例如正在删除、或旧存档缺上下文）时不能崩溃，返回 0/0 即可


            for (auto& cv : ctx->components) {
                // 遍历该子电路里的所有元件（每个元素是 QVariant 包装的 QVariantMap）
                // 子电路的"端口"不是单独存的，而是靠里面的 type=="input"/"output" 元件体现，所以要扫全部元件


                QString t = cv.toMap().value("type").toString();
                // 把 QVariant 转成 QVariantMap，取出 "type" 字段并转为 QString
                // toMap() 会产生一次拷贝，但这只是查询函数，可读性优先

                if (t == "input") inCount++;
                // 【分支】类型为 input 则输入计数 +1
                // 输入元件（input）是子电路的输入端口来源，统计它就等于统计端口数

                else if (t == "output") outCount++;
                // 【分支】类型为 output 则输出计数 +1
                // output 元件是子电路的输出端口来源；用 else if 而不是两个独立 if，避免未来出现同时是两者的类型时重复计数


            }
            // 元件遍历体结束

        }
        // if (ctx) 判断块结束
        // 统计完成后两个计数器要么是 0/0，要么是真实数量


        m["inCount"] = inCount;
        // 把输入端口数写入摘要


        m["outCount"] = outCount;
        // 把输出端口数写入摘要
        out.append(m);
        // 把这条摘要追加到结果列表
        // QList 的 append 是摊还 O(1)，不必提前 reserve

    }
    // 子电路遍历结束
    // 此时 out 里每条都是 {id, name, inCount, outCount}

    return out;
    // 返回组装好的列表
    // 按值返回，QVariantList 是隐式共享（写时复制）的，这里直接返回不会产生深拷贝

}
// subcircuits() 函数体结束

QVariantList Circuit::editContexts() const {
    // 定义 editContexts()：返回"可编辑上下文"列表 = 根上下文 + 所有子电路
    // 界面上有一个"当前正在编辑哪个电路"的下拉框：根电路（主电路）和每个子电路都必须是可切换的目标；
    // 与 subcircuits() 的区别：subcircuits() 是"有哪些子电路"（带端口统计），editContexts() 是"能在哪些上下文里编辑"（带根条目、且顺序稳定）


    QVariantList out;
    // 结果列表


    QVariantMap rootMap;
    // 为"根上下文"创建一条记录


    rootMap["id"] = "";
    // 根上下文的 ID 约定为空串
    // 整个工程统一约定"空串 = 根"（见 Circuit.h 的 isRootContext()，以及本文件大量 m_currentCtxId.isEmpty() 判断），
    //  这里必须用同一个约定，保证 switchEditContext("") 能正确切回根

    rootMap["name"] = "主电路";
    // 根上下文的显示名固定为中文"主电路"


    out.append(rootMap);
    // 把根条目放在列表最前面
    // 根电路是最常用、默认的编辑目标，放第一位符合下拉框的操作习惯

    QStringList ids = m_subNames.keys();
    // 取出所有子电路 ID 组成 QStringList


    std::sort(ids.begin(), ids.end());
    // 对 ID 列表做升序排序（字典序）
    // QHash 遍历顺序不确定，如果不同步排序，下拉框的条目会在每次重建时乱跳；排序后 "S1,S2,S10,S11" 虽不是自然数序，但至少稳定可预期

    for (auto& id : ids) {
        // 按排序后的顺序遍历每个子电路 ID


        QVariantMap m;
        // 为当前子电路创建一条记录


        m["id"] = id;
        // 写入子电路 ID


        m["name"] = m_subNames.value(id);
        // 从名称表取值写入显示名


        out.append(m);
        // 追加该子电路条目


    }
    // 子电路遍历结束
    // 结果顺序：["主电路", S1, S2, ...] 升序


    return out;
    // 返回完整列表

}
// editContexts() 结束
// ——


// ============================================================================
// ② 求值生命周期：引擎切换、全量求值
// ============================================================================

void Circuit::setEngineType(const QString& t) {
    // 定义 setEngineType：切换底层求值引擎（"iter" 迭代引擎 或 "event" 事件驱动引擎），切换后立即把新引擎跑出结果并刷新界面
    // 是 Q_PROPERTY 的 WRITE 函数（见 Circuit.h 第 73 行），QML 里直接给 engineType 赋值就会走这里


    if (m_engineType == t) return;
    // 【提前返回】如果目标类型与当前一致，直接返回，不做任何事


    if (t != "iter" && t != "event") return;
    // 【校验】只接受 "iter" 与 "event" 两个字面值


    if (m_simulationRunning) pauseSimulation();
    // 若仿真正在运行，先暂停


    if (m_clockRunning) stopClock();
    // 若时钟定时器在跑，先停止


    m_engineType = t;
    // 真正写入新的引擎类型


    saveSettings();
    // 把新引擎类型持久化到 QSettings


    if (m_engineType == "event") {
        // 【分支 A】切换到事件驱动引擎：重建引擎并把初始事件跑完


        rebuildEventEngine();
        // 用当前所有上下文（根 + 子电路）的元件与线重新构建事件引擎拓扑


        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        // 读取事件引擎当前的"线延迟"（每个信号穿过一根线所需的仿真时间单位），引擎为空时退回 1


        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        // 读取"全局门延迟"（每个逻辑门被求值后的输出要延迟多久才有效），引擎为空时退回 1


        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 把事件引擎一直推进到空闲：advance = (线延迟 + 门延迟) × 64 + 200，budget = 500000 个事件
        // ★ runEventEngineFully 内部有总事件预算 3 万，振荡电路不会在此卡死。


        applyEngineOutputsToContexts();
        // 把引擎算出来的每个节点输出写回元件 map


    } else {
        // 【分支 B】切换到迭代引擎："iter" 没有需要重建的常驻对象，直接一次性全量求值即可


        applyEngineOutputsToContexts();
        // 先把事件引擎（若存在）最后一次的结果写回上下文


        evaluateAll();
        // 用迭代引擎做一次全量收敛求值


    }
    // 分支结束


    emit engineChanged();
    // 发出 engineChanged 信号


    emit engineSettingsChanged();
    // 发出 engineSettingsChanged 信号


    emit changed();
    // 发出通用数据变化信号


    emit structureChanged();
    // 发出结构变化信号


}
// setEngineType 结束
// ——

void Circuit::evaluateAll() {
    // 定义 evaluateAll：整个电路模型最核心的"求值"入口
    // 所有会改变逻辑的写操作（连线、改属性、粘贴、切换引擎、撤销……）最终都会落到这里


    IterEngine::syncSubPins(m_root, m_subContexts);
    // 同步根上下文中所有 sub 实例的端口定义


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 对每个子电路上下文再次调用 syncSubPins，把"嵌套子电路的端口"同步到子电路内部的 sub 实例上


        IterEngine::syncSubPins(it.value(), m_subContexts);
    // 对当前子电路上下文执行端口同步


    if (m_viewOnly && !m_currentCtxId.isEmpty())
        // 【条件】当处于"浏览子电路实例"的只读模式时，把父实例上的输入值同步进子电路上下文


        syncSubStateFromParent(m_currentCtxId);
    // 执行上述同步


    if (m_engineType == "event") {
        // 【分支】事件引擎模式


        if (m_simulationRunning && m_eventEngine) {
            // 【子分支】仿真正在运行且引擎实例存在


            rebuildEventEngine();
            // 按当前结构重建引擎拓扑


            {
                // 开一个局部作用域，把"读延迟、跑引擎"两步包起来


                quint64 wd = m_eventEngine->wireDelay();
                // 读取线延迟


                quint64 gd = m_eventEngine->globalGateDelay();
                // 读取全局门延迟


                runEventEngineFully((wd + gd) * 64 + 200, 500000);
                // 把重建后引擎里堆积的初始事件全部跑完
                // ★ runEventEngineFully 内部有总事件预算 3 万，振荡电路不会在此卡死。


            }
            // 局部作用域结束


            applyEngineOutputsToContexts();
            // 把引擎输出写回上下文


        } else {
            // 【子分支】事件引擎模式，但仿真没在运行 / 引擎实例不存在
            //
            // 用户已明确要求 event 引擎"未开始仿真不能传播"。
            // 只标记 dirty，不做任何求值。input/const 元件自身的
            // outputPorts / inputPortValues 在各自的输入操作里已经手动同步为最新值，
            // 画布上 input 元件立即响应；下游元件保持原值，直到用户真正
            // startSimulation 后由事件引擎统一推进。

            m_engineDirty = true;
            // 标记引擎拓扑已失效


        }
        // 事件模式子分支结束
        // ——

    } else {
        // 【分支】迭代引擎模式（m_engineType != "event"）
        // 迭代引擎没有"运行/暂停"概念，每次都是全量重算，所以直接调用即可

        IterEngine::evaluateAll(m_root, m_subContexts);
        // 对根上下文与所有子电路做全量不动点迭代


    }
    // 引擎分支结束
    // ——


    bool eventSimOnCurrent = !m_viewOnly && m_engineType == "event" && m_simulationRunning;
    // 计算一个"当前上下文已经被事件引擎的 apply 覆盖过"的标志


    if (!m_currentCtxId.isEmpty() && !eventSimOnCurrent) {
        // 【条件】当正在查看某个子电路上下文、且它没有被子事件引擎覆盖时，额外对它做一次"独立求值"
        //
        // event 引擎未运行时，不允许通过任何路径传播。
        // 因此这里再加一层条件：只有"viewOnly 浏览模式"或"非 event 引擎"时
        // 才走 evaluateStandalone。否则（event + 编辑子电路 + 未运行），
        // 求值会被跳过——子电路内部保持原值，等待用户开始仿真。

        if (m_viewOnly || m_engineType != "event") {
            // 真正的求值前置条件：浏览模式或非 event 引擎

            auto it = m_subContexts.find(m_currentCtxId);
            // 在子电路表中定位当前上下文


            if (it != m_subContexts.end())
                // 【条件】找到了才求值


                IterEngine::evaluateStandalone(it.value(), m_subContexts);
            // 只对该子电路上下文求值一次

        }
        // viewOnly / 非 event 的判断结束

    }
    // 当前子电路独立求值分支结束
    // ——


    emit subcircuitsChanged();
    // 发出子电路列表变化信号


}
// evaluateAll 结束
// ——


void Circuit::evaluate() {
    // 定义 evaluate：Q_INVOKABLE 的公开求值入口


    evaluateAll();
    // 直接委托给 evaluateAll


}
// evaluate 结束
// ——


// ============================================================================
// ③ 事件引擎生命周期
// ============================================================================

void Circuit::rebuildEventEngine() {
    // 定义 rebuildEventEngine：把 Circuit 的上下文树"拍平"成事件引擎能理解的节点-边图，并重建引擎


    if (!m_eventEngine) return;
    // 【提前返回】引擎实例不存在时什么都不做


    IterEngine::syncSubPins(m_root, m_subContexts);
    // 构建前先同步根上下文中 sub 实例的端口定义


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 对每个子电路上下文同步其内部嵌套实例的端口


        IterEngine::syncSubPins(it.value(), m_subContexts);
    // 执行该层同步


    QHash<QString, QVariantList> subComps, subWires;
    // 准备两份"快照"：每个子电路 ID 对应的元件列表与连线列表


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        // 遍历所有子电路，填充上述两份快照


        subComps[it.key()] = it.value().components;
        // 把该子电路的元件列表存入快照，key 为 subId

        subWires[it.key()] = it.value().wires;
        // 把该子电路的连线列表存入快照


    }
    // 快照填充结束
    // ——


    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    // 判断当前是否处于"把子电路当作根来编辑/仿真"的模式


    if (editingSubAsRoot) {
        // 【分支】编辑子电路作为根


        auto it = m_subContexts.find(m_currentCtxId);
        // 定位当前编辑的子电路


        if (it != m_subContexts.end()) {
            // 【分支】找到了当前子电路


            QVariantList allWires = it.value().wires;
            // 以该子电路的原始连线为起点


            QVariantList vw = WireNetworkSolver::generateVirtualWires(
                // 根据该子电路的元件与连线，推导出"虚拟连接"列表，存入 vw


                it.value().components, it.value().wires);
            // 传入该子电路的元件与原始连线


            for (const auto& w : vw) allWires.append(w);
            // 把虚拟线逐条追加到 allWires


            m_eventEngine->build(it.value().components, allWires, subComps, subWires);
            // 用"该子电路元件 + 合并后的线 + 全部子电路定义"构建引擎


            m_engineDirty = false;
            // 标记拓扑已是最新


            m_needFullApply = true;
            // 标记下一次 apply 必须全量刷新


            m_ctxSubInstCache.clear();
            // 清空"上下文 → 子电路实例 ID 列表"缓存


            return;
            // 【提前返回】子电路作根的分支处理完毕，直接结束函数


        }
        // 编辑子电路分支结束
        // ——


    }
    // editingSubAsRoot 外层分支结束


    QVariantList allWires = m_root.wires;
    // 以根上下文的原始连线为起点


    QVariantList vw = WireNetworkSolver::generateVirtualWires(
        // 为根上下文生成虚拟连接


        m_root.components, m_root.wires);
    // 传入根的元件与原始连线


    for (const auto& w : vw) allWires.append(w);
    // 合并虚拟线


    m_eventEngine->build(m_root.components, allWires, subComps, subWires);
    // 以根上下文为顶层构建引擎


    m_engineDirty = false;
    // 标记拓扑最新


    m_needFullApply = true;
    // 下次 apply 全量


    m_ctxSubInstCache.clear();
    // 清空实例缓存


}
// rebuildEventEngine 结束
// ——

static void updateOneComponent(QVariantMap& c,
                               EventEngine* eng,
                               const QHash<QString, CircuitContext>& subContexts)
// 定义文件级静态函数 updateOneComponent：把事件引擎中某一个元件的输出写回它的数据 map


{
    // 函数体开始


    QString localId = c.value("id").toString();
    // 取出该元件在本地上下文中的 ID


    QString type = c.value("type").toString();
    // 取出元件类型字符串


    if (type == "sub") {
        // 【分支】子电路实例：需要特殊处理

        QString subId = c.value("subId").toString();
        // 取出该实例引用的子电路定义 ID


        auto sit = subContexts.find(subId);
        // 在定义表中查找该子电路


        if (sit == subContexts.end()) return;
        // 【提前返回】定义不存在就不更新这个实例


        QVariantList outVals;
        // 准备收集该实例的输出端口值

        for (const auto& scv : sit.value().components) {
            // 遍历子电路定义中的所有元件


            QVariantMap sc = scv.toMap();
            // 把 QVariant 转成 QVariantMap


            if (sc.value("type").toString() != "output") continue;
            // 【跳过条件】不是 output 元件的直接跳过


            QString innerQid = localId + "/" + sc.value("id").toString();
            // 拼出内部 output 元件在引擎中的限定 ID


            QStringList outs = eng->getOutputs(innerQid);
            // 向引擎查询该节点的输出端口值


            QString v = outs.isEmpty() ? QString() : outs[0];
            // 取第一个输出端口的值


            if (v.isEmpty()) {
                // 【条件】引擎里没有这个节点的值时，构造一个"全 0"的合法值

                int w = sc.value("bitWidth").toInt();
                // 读取该 output 元件的位宽


                if (w < 1) w = 1;
                // 【钳位】位宽至少为 1

                v = QString(w, '0');
                // 构造长度为 w 的全 '0' 字符串

            }
            // 回退分支结束


            outVals.append(v);
            // 把该输出端口的值追加到结果列表

        }
        // 输出端口遍历结束


        if (!outVals.isEmpty()) c["outputPorts"] = outVals;
        // 若有输出端口，写回实例 map 的 "outputPorts" 字段

        QVariantList inVals;
        // 准备收集该实例的输入端口值

        for (const auto& scv : sit.value().components) {
            // 再次遍历子电路定义中的元件，这次找 input

            QVariantMap sc = scv.toMap();
            // 转成 map

            if (sc.value("type").toString() != "input") continue;
            // 【跳过条件】只要 input 元件

            QString innerQid = localId + "/" + sc.value("id").toString();
            // 拼出内部 input 元件的限定 ID

            QStringList outs = eng->getOutputs(innerQid);
            // 查询该 input 节点的输出值

            QString v = outs.isEmpty() ? QString() : outs[0];
            // 取端口值，空则留空串

            if (v.isEmpty()) {
                // 【条件】值为空时回退全 0

                int w = sc.value("bitWidth").toInt();
                // 读取位宽

                if (w < 1) w = 1;
                // 【钳位】位宽至少 1

                v = QString(w, '0');
                // 构造全 0 串

            }
            // 输入回退分支结束

            inVals.append(v);
            // 追加到输入端口值列表

        }
        // 输入端口遍历结束

        if (!inVals.isEmpty()) c["inputPortValues"] = inVals;
        // 写回实例的 "inputPortValues" 字段

        return;
        // 【提前返回】sub 分支处理完毕，结束函数

    }
    // sub 分支结束

    QStringList outs = eng->getOutputs(localId);
    // 对普通元件，直接用自己的本地 ID 向引擎查询输出

    if (!outs.isEmpty()) {
        // 【条件】引擎确实有这个节点、且有输出端口时才写回

        QVariantList ops;
        // 准备输出端口值列表

        ops.reserve(outs.size());
        // 预分配容量

        for (const auto& s : outs) ops.append(s);
        // 逐个把 QString 追加进 QVariantList

        c["outputPorts"] = ops;
        // 把输出端口值写回元件 map

        if (type == "input" || type == "const") {
            // 【条件】input（输入开关）与 const（常量）元件还要额外同步"位数组"

            int bw = c.value("bitWidth").toInt();
            // 读取位宽

            if (bw < 1) bw = 1;
            // 【钳位】位宽至少 1

            QString s = outs[0];
            // 取该元件的第一个输出串

            if (s.length() != bw) {
                // 【条件】字符串长度与位宽不一致时做规整

                if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
                // 【分支】长度不足时左侧补 '0' 到 bw 位

                else                 s = s.right(bw);
                // 【分支】长度超出时只保留最右侧 bw 位

            }
            // 规整分支结束

            QVariantList bits;
            // 准备位数组

            bits.reserve(bw);
            // 预分配 bw 个元素容量

            for (int i = 0; i < bw; ++i) bits.append(s[i] == '1');
            // 逐位比较：字符为 '1' 记 true

            c["inputBits"] = bits;
            // 写回位数组

        }
        // input/const 附加分支结束

    }
    // 普通元件输出写回分支结束

    if (type == "output" || type == "led") {
        // 【条件】output（输出探针）与 led（指示灯）还要同步"输入端口值"

        if (!outs.isEmpty()) {
            // 【条件】引擎有值才写

            QVariantList ips;
            // 准备输入端口值列表

            ips.reserve(outs.size());
            // 预分配

            for (const auto& s : outs) ips.append(s);
            // 逐个追加

            c["inputPortValues"] = ips;
            // 写回输入端口值

        }
        // 条件结束

    }
    // output/led 分支结束

}
// updateOneComponent 结束
// ——

void Circuit::applyEngineOutputsToContexts() {
    // 定义 applyEngineOutputsToContexts：把事件引擎算出的结果"回灌"到 Circuit 的上下文数据结构里


    if (!m_eventEngine) return;
    // 【提前返回】引擎不存在则无事可做

    if (m_engineDirty) {
        // 【条件】拓扑已过期时，先重建

        rebuildEventEngine();
        // 重建引擎拓扑

    }
    // 重建分支结束

    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    // 判断"当前是在编辑某个子电路（而非浏览、而非根）"

    Context* targetPtr = editingSubAsRoot ? ctxById(m_currentCtxId) : &m_root;
    // 取得目标上下文指针

    if (!targetPtr) targetPtr = &m_root;
    // 兜底：指针为空时退回根上下文

    Context& targetCtx = *targetPtr;
    // 解引用得到目标上下文的引用

    if (m_needFullApply) {
        // 【分支】全量刷新

        m_needFullApply = false;
        // 先消费掉"需要全量"的标记

        for (int i = 0; i < targetCtx.components.size(); ++i) {
            // 按索引遍历目标上下文的所有元件

            QVariantMap c = targetCtx.components[i].toMap();
            // 取出第 i 个元件并转成可修改的 QVariantMap

            updateOneComponent(c, m_eventEngine, m_subContexts);
            // 让静态函数把该元件的输出/输入值写进 c

            targetCtx.components[i] = c;
            // 把修改后的 map 写回列表

        }
        // 全量遍历结束

        m_eventEngine->clearDirtyQids();
        // 清空引擎内部的"脏节点"集合

        return;
        // 【提前返回】全量模式结束

    }
    // 全量分支结束

    QVector<QString> dirty = m_eventEngine->takeDirtyQids();
    // 从引擎"取走"脏节点限定的 ID 列表

    if (dirty.isEmpty()) return;
    // 【提前返回】没有脏节点

    QHash<QString, int> idToIdx;
    // 建立"元件 ID → 在 targetCtx.components 中的索引"的映射

    idToIdx.reserve(targetCtx.components.size() * 2);
    // 预分配哈希桶

    for (int i = 0; i < targetCtx.components.size(); ++i)
        // 遍历所有元件，建立映射

        idToIdx.insert(targetCtx.components[i].toMap().value("id").toString(), i);
    // 插入 "元件ID → 索引"

    QSet<QString> instanceIdsToUpdate;
    // 收集"需要更新"的元件 ID 集合

    for (const QString& qid : dirty) {
        // 遍历每个脏的限定 ID

        int slash = qid.indexOf('/');
        // 查找限定 ID 中第一个 '/' 的位置

        QString topId = (slash < 0) ? qid : qid.left(slash);
        // 截取顶层元件 ID

        if (idToIdx.contains(topId))
            // 【条件】该顶层 ID 确实是本上下文里的元件

            instanceIdsToUpdate.insert(topId);
        // 加入待更新集合

    }
    // 脏 ID 归一化结束

    for (const QString& id : instanceIdsToUpdate) {
        // 遍历去重后的待更新元件 ID

        int i = idToIdx.value(id, -1);
        // 查索引，查不到给 -1

        if (i < 0) continue;
        // 【跳过条件】索引非法时跳过

        QVariantMap c = targetCtx.components[i].toMap();
        // 取出该元件 map

        updateOneComponent(c, m_eventEngine, m_subContexts);
        // 按类型把引擎结果写入 c

        targetCtx.components[i] = c;
        // 写回上下文

    }
    // 增量写回结束

    if (m_viewOnly) refreshCurrentSubContext();
    // 【条件】浏览模式下，额外刷新正在浏览的子电路上下文

}
// applyEngineOutputsToContexts 结束
// ——

QStringList Circuit::engineQidsFor(const QString& ctxId, const QString& localCompId) const {
    // 定义 engineQidsFor：把"某个上下文里的本地元件 ID"翻译成"事件引擎里的限定 ID 列表"


    if (!m_currentCtxId.isEmpty() && !m_viewOnly)
        // 【条件 1】当前是在"编辑子电路作为根"的模式


        return QStringList{ localCompId };
    // 用初始化列表构造单元素 QStringList 返回

    if (ctxId.isEmpty()) return QStringList{ localCompId };
    // 【条件 2】ctxId 为空表示"根上下文里的元件"

    auto it = m_ctxSubInstCache.constFind(ctxId);
    // 在缓存中查找"该子电路 ID → 引用它的实例 ID 列表"

    QStringList instIds;
    // 存放实例 ID 列表

    if (it != m_ctxSubInstCache.constEnd()) {
        // 【条件】缓存命中

        instIds = it.value();
        // 从缓存取出实例列表

    } else {
        // 【条件】缓存未命中

        for (const auto& cv : m_root.components) {
            // 遍历根上下文的全部元件

            QVariantMap c = cv.toMap();
            // 转成 map 便于读字段

            if (c.value("type").toString() != "sub") continue;
            // 【跳过条件】非 sub 元件跳过

            if (c.value("subId").toString() == ctxId)
                // 【条件】该实例引用的正是目标子电路

                instIds.append(c.value("id").toString());
            // 记录实例 ID

        }
        // 扫描结束

        m_ctxSubInstCache.insert(ctxId, instIds);
        // 把结果写入缓存

    }
    // 缓存分支结束

    QStringList result;
    // 准备最终结果列表

    result.reserve(instIds.size());
    // 预分配

    for (const QString& instId : instIds)
        // 遍历每个实例

        result.append(instId + "/" + localCompId);
    // 拼出限定 ID

    return result;
    // 返回全部限定 ID

}
// engineQidsFor 结束
// ——

// ★ 修复：起始 advance 至少 10000（原值 ~200 会导致大电路需要一路翻倍，
//   每次翻倍都受 budget=50万 上限约束，最终跑不完）
// ★ 二次修复：对振荡电路（如 not 门自环、奇数反相器环）事件队列永不空闲，
//   单次预热必须设"总事件上限"，否则会在 UI 线程同步跑 200 轮 × budget = 1 亿事件，
//   卡死数十秒。新增 kRunFullyTotalBudget = 30000：任何一次预热最多处理 3 万事件，
//   超过立即放弃，剩余事件交给 onSimTimerTick 每帧推进。正常组合逻辑电路的收敛事件数
//   远小于 3 万，会被正常跑完；振荡器被截断后立即返回，UI 恢复响应，用户能真正看到闪烁。
void Circuit::runEventEngineFully(quint64 advance, quint64 budget) {
    // 定义 runEventEngineFully：把事件引擎"跑到空闲"为止，内部自动重试并放大时间步

    if (!m_eventEngine) return;
    // 【提前返回】引擎不存在则直接返回

    if (budget < 1) budget = 200000;
    // 【兜底】调用方传 0 或负数预算时，用 200000 顶上

    quint64 a = qMax<quint64>(advance, 10000ULL);
    // 计算起始时间步长：取传入值与 10000 的最大者

    // ★ 单次预热的总事件上限：防止振荡器把 UI 线程卡死
    //   - 正常组合逻辑的收敛事件数（初始 Recalc + 传播）远小于 3 万，会被正常跑完；
    //   - 振荡器每 2 单位时间就会产生新事件，永无空闲，3 万事件约 5~15ms 后放弃，
    //     剩余事件留在队列里，由 onSimTimerTick 每帧继续推进，UI 保持响应。
    const quint64 kRunFullyTotalBudget = 30000ULL;
    quint64 totalUsed = 0;

    for (int round = 0; round < 200; ++round) {
        // 最多重试 200 轮

        if (m_eventEngine->isIdle()) break;
        // 【循环条件】进入每一轮前先检查是否已空闲

        if (totalUsed >= kRunFullyTotalBudget) break;
        // ★ 总预算耗尽：立即放弃预热，剩余事件交给帧驱动

        quint64 remain = kRunFullyTotalBudget - totalUsed;
        // 本轮还能用的总预算

        quint64 roundBudget = qMin(budget, remain);
        // 本轮实际预算 = min(调用方预算, 剩余总预算)

        bool ok = m_eventEngine->run(a, roundBudget);
        // 在 a 个时间单位内、最多处理 roundBudget 个事件，并记录是否"完全跑完"

        totalUsed += roundBudget;
        // 累计已用预算（保守估计：按上限计入，不会低估）

        if (m_eventEngine->isIdle()) break;
        // run 之后再检查一次空闲

        if (ok) {
            // 【分支】本次 run 成功（窗口内跑完，但仍有后续事件被排在窗口外）

            if (a < 100000000ULL) a *= 2;
            // 【分支】翻倍时间步，但不超过 1e8

            else a = 100000000ULL;
            // 【分支】达到上限后固定为 1e8

        }
        // ok 分支结束

    }
    // 重试循环结束

}
// runEventEngineFully 结束
// ——

// ============================================================================
// ④ 文档生命周期：清空、错误检查、序列化、文件读写
// ============================================================================

void Circuit::clear() {
    // 定义 clear：把整个电路重置为"新建文档"的初始状态

    if (m_viewOnly) return;
    // 【提前返回】只读浏览模式下禁止清空

    pushUndo();
    // 清空之前先压一次撤销快照

    m_root = Context();
    // 用一个默认构造的 Context 覆盖根上下文

    m_subContexts.clear();
    // 清空所有子电路定义

    m_subNames.clear();
    // 清空子电路名称表

    m_currentCtxId = "";
    // 把当前上下文切回根

    m_viewOnly = false;
    // 退出只读浏览标志

    m_subCounter = 0;
    // 子电路编号计数器归零

    m_importCounter = 0;
    // 导入前缀计数器归零

    m_errors.clear();
    // 清空缓存的错误列表

    m_clockStates.clear();
    // 清空每个时钟元件的状态表

    m_clockTickCount = 0;
    // 时钟跳变计数归零

    m_clipboardCache.clear();
    // 清空内部剪贴板

    m_needFullApply = true;
    // 标记下一次 apply 需要全量

    m_importedFiles.clear();
    // 清空"已导入文件"记录

    if (m_clockRunning) stopClock();
    // 【条件】若时钟在跑就停掉

    if (m_simulationRunning) pauseSimulation();
    // 【条件】若仿真在跑就暂停

    if (m_eventEngine) m_eventEngine->clear();
    // 清空事件引擎内部的事件队列与节点数据

    m_engineDirty = true;
    // 标记引擎拓扑已失效

    m_ctxSubInstCache.clear();
    // 清空实例缓存

    emit contextChanged();
    // 通知上下文已切换（回到根）

    emit subcircuitsChanged();
    // 通知子电路列表变化（现在为空）

    emit editContextsChanged();
    // 通知可编辑上下文列表变化（只剩"主电路"）

    emit errorsChanged();
    // 通知错误列表变化

    emit clockChanged();
    // 通知时钟状态变化

    emit changed();
    // 通知通用数据变化

}
// clear 结束
// ——

void Circuit::refreshErrors() {
    // 定义 refreshErrors：扫描所有上下文的连线，找出"源端口位宽 ≠ 目标端口位宽"的错误，写入 m_errors

    m_errors.clear();
    // 先清空旧错误

    auto checkCtx = [&](const Context& ctx, const QString& label) {
        // 定义局部 lambda checkCtx：对一个上下文做错误扫描

        QHash<QString, int> idToIdx;
        // 建立"元件 ID → 索引"映射

        idToIdx.reserve(ctx.components.size() * 2);
        // 预分配 2 倍容量

        for (int i = 0; i < ctx.components.size(); ++i)
            // 遍历元件建表

            idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);
        // 插入映射

        for (auto& wv : ctx.wires) {
            // 遍历该上下文的所有连线

            QVariantMap w = wv.toMap();
            // 转成 map 便于读字段

            QString wt = w.value("wireType").toString();
            // 读取连线类型

            if (wt == "ortho" || wt == "virtual") continue;
            // 【跳过条件】跳过正交线与虚拟线

            QString fromId = w.value("fromComp").toString();
            // 取源元件 ID

            QString toId = w.value("toComp").toString();
            // 取目标元件 ID

            int fromPort = w.value("fromPort").toInt();
            // 取源端口索引

            int toPort = w.value("toPort").toInt();
            // 取目标端口索引

            int fi = idToIdx.value(fromId, -1);
            // 查源元件索引，查不到给 -1

            int ti = idToIdx.value(toId, -1);
            // 查目标元件索引

            if (fi < 0 || ti < 0) continue;
            // 【跳过条件】任一端指向不存在的元件就跳过

            int fbw = ComponentLogic::outputBitWidth(ctx.components[fi].toMap(), fromPort);
            // 计算源元件第 fromPort 个输出端口的位宽

            int tbw = ComponentLogic::inputBitWidth(ctx.components[ti].toMap(), toPort);
            // 计算目标元件第 toPort 个输入端口的位宽

            if (fbw != tbw) {
                // 【条件】两端位宽不等 → 记录错误

                QVariantMap err;
                // 创建一条错误记录

                err["type"] = "width_mismatch";
                // 错误类型标记

                err["msg"] = QString("%1：%2 位宽不匹配（源 %3 位，目标 %4 位）")
                                 .arg(label, w.value("id").toString()).arg(fbw).arg(tbw);
                // 生成人类可读的中文错误消息

                m_errors.append(err);
                // 把错误追加进缓存列表

            }
            // 位宽不匹配分支结束

        }
        // 连线遍历结束

    };
    // lambda 定义结束

    checkCtx(m_root, "主电路");
    // 检查根上下文，标签为"主电路"

    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 遍历所有子电路逐一检查

        checkCtx(it.value(), m_subNames.value(it.key(), it.key()));
    // 用子电路名作为标签

    emit errorsChanged();
    // 通知 UI 错误列表已更新

}
// refreshErrors 结束
// ——

QVariantMap Circuit::toJson() const {
    // 定义 toJson：把整个电路（含子电路、时钟状态、引擎参数）导出为一个 QVariantMap

    QHash<QString, quint64> gateDelays;
    // 准备"门类型 → 独立延迟"的覆盖表

    if (m_eventEngine) gateDelays = m_eventEngine->gateTypeDelays();
    // 从事件引擎读出延迟覆盖表

    return JsonCodec::encode(m_root, m_subContexts, m_subNames,
                             m_subCounter, m_clockFrequency, m_clockTickCount,
                             m_engineType, globalGateDelay(), wireDelay(),
                             m_simulationSpeed, m_simulationSpeedExp, m_autoSpeed,
                             gateDelays);
    // 调用 JsonCodec::encode 把全部状态编码成 map

}
// toJson 结束
// ——

bool Circuit::fromJson(const QVariantMap& data) {
    // 定义 fromJson：从 QVariantMap 恢复整个电路状态

    CircuitUndoData* d = und();
    // 取得撤销数据指针

    bool wasRestoring = d ? d->restoring : false;
    // 记录调用前的 restoring 状态

    if (d) d->restoring = true;
    // 进入"恢复中"状态

    JsonCodec::DecodedData dec = JsonCodec::decode(data);
    // 把 map 解码成强类型结构 DecodedData

    m_root = dec.root;
    // 用解码结果替换根上下文

    m_subContexts = dec.subContexts;
    // 替换所有子电路定义

    m_subNames = dec.subNames;
    // 替换子电路名称表

    m_currentCtxId = "";
    // 加载后回到根上下文

    m_viewOnly = false;
    // 退出只读浏览

    m_clockStates = dec.clockStates;
    // 恢复每个时钟元件的通断状态

    m_subCounter = dec.subCounter;
    // 恢复子电路编号计数器

    m_clockTickCount = dec.clockTickCount;
    // 恢复时钟跳变计数

    m_needFullApply = true;
    // 标记下次 apply 全量

    m_importedFiles.clear();
    // 清空导入记录

    if (m_eventEngine) {
        // 【条件】引擎存在时才处理延迟覆盖

        m_eventEngine->clearGateTypeDelays();
        // 先清空引擎里现有的延迟覆盖

        for (auto it = dec.gateTypeDelays.begin(); it != dec.gateTypeDelays.end(); ++it)
            // 遍历解码出的覆盖表逐项设置

            m_eventEngine->setGateTypeDelay(it.key(), it.value());
        // 把 "门类型 → 延迟" 写入引擎

    }
    // 延迟恢复块结束

    if (m_engineType == "event") rebuildEventEngine();
    // 【条件】事件引擎模式下重建拓扑

    else evaluateAll();
    // 【另一分支】迭代引擎模式下直接全量求值

    if (!wasRestoring) {
        // 【条件】只有当本次调用不是"嵌套在已有恢复流程中"时才清理撤销历史

        if (d) {
            // 【条件】撤销数据存在才清

            d->undoStack.clear();
            // 清空撤销栈

            d->redoStack.clear();
            // 清空重做栈

            d->lastMergeKey.clear();
            // 清空上次的合并键

            d->lastUndoTime = QDateTime();
            // 重置合并时间戳为"无效时间"

        }
        // 清栈块结束

    }
    // 历史清理条件块结束

    if (d) d->restoring = wasRestoring;
    // 恢复调用前的 restoring 状态

    if (wasRestoring) return true;
    // 【提前返回】嵌套调用时到此为止，不发信号

    emit contextChanged();
    // 通知上下文切换（已回到根）

    emit subcircuitsChanged();
    // 通知子电路列表变化

    emit editContextsChanged();
    // 通知可编辑上下文列表变化

    emit errorsChanged();
    // 通知错误列表变化

    emit clockChanged();
    // 通知时钟状态变化

    emit engineChanged();
    // 通知引擎类型变化

    emit engineSettingsChanged();
    // 通知引擎参数变化

    emit structureChanged();
    // 通知结构变化

    emit changed();
    // 通知通用数据变化

    return true;
    // 返回成功

}
// fromJson 结束
// ——

static QByteArray extractFirstJsonObject(const QByteArray& data) {
    // 定义文件级静态工具函数：从一段可能"脏"的字节里提取出第一个完整的 JSON 对象

    int depth = 0;
    // 当前花括号嵌套深度

    int start = -1;
    // 第一个对象的起始字节下标；-1 表示尚未找到

    bool inStr = false;
    // 是否处于字符串字面量内部

    bool esc = false;
    // 上一个字符是否是反斜杠转义

    for (int i = 0; i < data.size(); ++i) {
        // 逐字节扫描

        char c = data[i];
        // 取当前字节

        if (esc) { esc = false; continue; }
        // 【状态 1】若上一字符是转义符，本字符被跳过，并清除转义标记

        if (inStr) {
            // 【状态 2】在字符串内部

            if (c == '\\') { esc = true; continue; }
            // 【分支】遇到反斜杠：标记转义并跳过下一个字节

            if (c == '"') { inStr = false; }
            // 【分支】遇到引号：字符串结束

            continue;
            // 字符串内部其余字符一律忽略

        }
        // 字符串状态块结束

        if (c == '"') { inStr = true; continue; }
        // 遇到引号：进入字符串状态

        if (c == '{') {
            // 【状态 3】遇到左花括号

            if (depth == 0) start = i;
            // 深度为 0 时说明这是最外层对象的第一个 '{'，记录起点

            ++depth;
            // 深度加一

        } else if (c == '}') {
            // 【状态 4】遇到右花括号

            --depth;
            // 深度减一

            if (depth == 0 && start >= 0)
                // 【条件】深度回到 0 且确实记录过起点 → 第一个对象到此结束

                return data.mid(start, i - start + 1);
            // 返回 [start, i] 区间（含首尾括号）的子串

        }
        // 括号处理结束

    }
    // 扫描结束

    return QByteArray();
    // 返回空字节数组表示"提取失败"

}
// extractFirstJsonObject 结束
// ——

static bool isOnlyWhitespaceFrom(const QByteArray& data, int pos) {
    // 定义工具函数：判断从 pos 开始到末尾是否全是空白字符

    for (int i = pos; i < data.size(); ++i) {
        // 从 pos 扫到末尾

        char c = data[i];
        // 取当前字节

        if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) return false;
        // 若遇到非空白字节：立即返回 false

    }
    // 扫描结束

    return true;
    // 返回 true 表示"从 pos 起全是空白"

}
// isOnlyWhitespaceFrom 结束
// ——

bool Circuit::saveToFile(const QString& path) {
    // 定义 saveToFile：把当前电路序列化并以带缩进的 JSON 写入指定路径

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空直接失败

    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    // 把 toJson() 得到的 QVariantMap 转成 QJsonObject

    QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    // 序列化成带缩进的 UTF-8 字节

    if (bytes.isEmpty()) return false;
    // 【提前返回】序列化结果为空视为失败

    QFile f(path);
    // 以原始路径构造 QFile 对象（尚未打开）

    if (path.startsWith("content://")) {
        // 【分支】Android 的 content URI 路径

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        // 以"只写 + 文本模式"打开

    } else {
        // 【分支】普通文件路径（含 file:// URI）

        QString local = path;
        // 准备本地路径副本

        if (local.startsWith("file://")) local = QUrl(local).toLocalFile();
        // 若是 file:// URI，转换成平台本地路径

        f.setFileName(local);
        // 把 QFile 的文件名换成转换后的本地路径

        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        // 以"只写 + 截断 + 文本"打开

    }
    // 路径分支结束

    qint64 n = f.write(bytes);
    // 写入全部字节，返回实际写入的字节数

    if (n != bytes.size()) { f.close(); return false; }
    // 【错误处理】写入字节数不等于预期 → 关闭文件并返回失败

    f.close();
    // 正常路径下关闭文件

    addRecentFile(path);
    // 把该路径加入"最近打开文件"列表

    return true;
    // 返回成功

}
// saveToFile 结束
// ——

bool Circuit::loadFromFile(const QString& path) {
    // 定义 loadFromFile：从文件读取电路，解析 JSON（带容错），交给 fromJson 恢复

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空失败

    QFile f(path);
    // 用原始路径构造 QFile

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    // 以"只读 + 文本"打开

    QByteArray data = f.readAll();
    // 一次性读入全部内容

    f.close();
    // 读完后立即关闭文件

    QJsonParseError err;
    // 声明解析错误输出参数

    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    // 先尝试整体解析

    QByteArray cleanData;
    // 准备"抢救出来的干净数据"

    bool needsHeal = false;
    // 是否需要"治愈"（回写清理后的文件）

    if (err.error != QJsonParseError::NoError) {
        // 【容错分支】整体解析失败

        cleanData = extractFirstJsonObject(data);
        // 尝试提取第一个完整的 { ... } 块

        if (cleanData.isEmpty()) return false;
        // 【提前返回】提取失败（文件里根本没有完整对象）→ 打开失败

        if (cleanData.size() == data.size()) return false;
        // 【提前返回】提取结果与原数据等长 → 视为失败

        int endPos = data.indexOf(cleanData) + cleanData.size();
        // 计算被提取对象在原数据中的结束位置

        needsHeal = !isOnlyWhitespaceFrom(data, endPos);
        // 检测对象之后是否有非空白内容

        QJsonParseError err2;
        // 为第二次解析准备独立的错误输出

        QJsonDocument doc2 = QJsonDocument::fromJson(cleanData, &err2);
        // 解析提取出来的片段

        if (err2.error != QJsonParseError::NoError) return false;
        // 【提前返回】片段解析仍失败 → 打开失败

        if (!doc2.isObject()) return false;
        // 【提前返回】片段不是对象（例如是数组）→ 失败

        doc = doc2;
        // 用抢救出的解析结果替换 doc

    }
    // 容错分支结束

    if (!doc.isObject()) return false;
    // 【统一校验】解析成功但顶层不是对象 → 失败

    bool ok = fromJson(doc.object().toVariantMap());
    // 把 JSON 对象转成 QVariantMap 并恢复电路

    if (!ok) return false;
    // 【提前返回】恢复失败 → 打开失败

    if (needsHeal) {
        // 【条件】需要治愈：把"干净片段"回写到原文件

        QFile wf(path);
        // 为回写重新构造一个 QFile

        bool openOk = path.startsWith("content://")
                          ? wf.open(QIODevice::WriteOnly | QIODevice::Text)
                          : wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        // 按路径类型选择打开标志

        if (openOk) { wf.write(cleanData); wf.close(); }
        // 打开成功则写入干净数据并关闭

    }
    // 治愈分支结束

    addRecentFile(path);
    // 记录最近打开的文件

    return ok;
    // 返回结果（此处必为 true）

}
// loadFromFile 结束
// ——

// ============================================================================
// ⑤ 外部导入生命周期：把一个电路文件挂载为若干子电路
// ============================================================================

void Circuit::removeSubContextsByPrefix(const QString& prefix) {
    // 定义 removeSubContextsByPrefix：把所有 ID 以 prefix 开头的子电路连同它们在所有上下文里的实例一起删除

    if (prefix.isEmpty()) return;
    // 【提前返回】空前缀直接返回

    QSet<QString> subIdSet;
    // 收集所有以 prefix 开头的子电路 ID

    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        // 遍历所有子电路

        if (it.key().startsWith(prefix)) subIdSet.insert(it.key());
        // 【条件】ID 以 prefix 开头则加入集合

    }
    // 收集结束

    if (subIdSet.isEmpty()) return;
    // 【提前返回】没有任何匹配 → 无事可做

    for (const auto& sid : subIdSet) {
        // 逐个删除已收集的子电路

        m_subContexts.remove(sid);
        // 删除子电路定义

        m_subNames.remove(sid);
        // 删除对应名称

    }
    // 删除结束

    auto removeInstances = [&](Context& ctx) {
        // 定义局部 lambda：从一个上下文里删除所有引用了已删子电路的 sub 实例，以及这些实例的连线

        for (int i = ctx.components.size() - 1; i >= 0; --i) {
            // 从后往前遍历元件

            auto c = ctx.components[i].toMap();
            // 取出当前元件 map

            if (c.value("type").toString() != "sub") continue;
            // 【跳过条件】不是子电路实例的元件跳过

            if (!subIdSet.contains(c.value("subId").toString())) continue;
            // 【跳过条件】实例引用的子电路不在待删集合里则跳过

            QString id = c.value("id").toString();
            // 记录该实例的 ID

            for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                // 倒序遍历连线，删除与这个实例相连的所有线

                auto w = ctx.wires[j].toMap();
                // 取出连线 map

                if (w.value("fromComp").toString() == id ||
                    w.value("toComp").toString() == id)
                    // 【条件】任一端是待删实例

                    ctx.wires.removeAt(j);
                // 删除该连线

            }
            // 连线删除循环结束

            ctx.components.removeAt(i);
            // 删除该 sub 实例元件

        }
        // 元件遍历结束

    };
    // lambda 定义结束

    removeInstances(m_root);
    // 先从根上下文清理

    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 再遍历剩余的子电路上下文

        removeInstances(it.value());
    // 对该子电路执行清理

    if (subIdSet.contains(m_currentCtxId)) {
        // 【条件】当前正在浏览/编辑的上下文恰好是被删的子电路之一

        m_currentCtxId = "";
        // 切回根上下文

        m_viewOnly = false;
        // 退出只读浏览

        m_currentInstancePath.clear();
        // 清空实例路径

        m_browseStack.clear();
        // 清空浏览栈

        emit contextChanged();
        // 通知 UI 上下文已切换

    }
    // 当前位置修复分支结束

    m_ctxSubInstCache.clear();
    // 清空实例缓存

}
// removeSubContextsByPrefix 结束
// ——

bool Circuit::importFromFile(const QString& path) {
    // 定义 importFromFile：把外部电路文件导入为当前文档中的一组子电路

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空失败

    if (m_viewOnly) return false;
    // 【提前返回】只读浏览禁止导入

    QFile f(path);
    // 打开源文件

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    // 只读 + 文本模式打开

    QByteArray data = f.readAll();
    // 读入全部内容

    f.close();
    // 关闭文件

    if (data.isEmpty()) return false;
    // 【提前返回】空文件视为失败

    QJsonParseError err;
    // 解析错误输出

    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    // 先整体解析

    if (err.error != QJsonParseError::NoError) {
        // 【容错分支】整体解析失败

        QByteArray clean = extractFirstJsonObject(data);
        // 提取第一个 JSON 对象

        if (clean.isEmpty()) return false;
        // 【提前返回】提取失败 → 导入失败

        QJsonParseError err2;
        // 第二次解析的错误输出

        QJsonDocument doc2 = QJsonDocument::fromJson(clean, &err2);
        // 解析提取出的片段

        if (err2.error != QJsonParseError::NoError || !doc2.isObject()) return false;
        // 【提前返回】片段非法或不是对象 → 失败

        doc = doc2;
        // 用法结果替换 doc

    }
    // 容错分支结束

    if (!doc.isObject()) return false;
    // 【统一校验】顶层必须是对象

    JsonCodec::DecodedData dec = JsonCodec::decode(doc.object().toVariantMap());
    // 解码成强类型结构

    QString baseName = QFileInfo(path).completeBaseName();
    // 取源文件的"基名"（去掉最后一个扩展名的文件名）

    if (baseName.isEmpty()) baseName = "外部电路";
    // 基名为空时用默认名"外部电路"

    if (m_importedFiles.contains(path)) {
        // 【分支 A】这个文件之前已经导入过 → 先卸载旧的导入

        QString oldPrefix = m_importedFiles.value(path).value("prefix").toString();
        // 取出上次导入使用的前缀

        removeSubContextsByPrefix(oldPrefix);
        // 按前缀删除上次导入的全部子电路与实例

        m_importedFiles.remove(path);
        // 从导入记录里移除旧条目

    } else {
        // 【分支 B】首次导入这个文件

        QString suffix = QString("（%1）").arg(baseName);
        // 构造导入时使用的名称后缀

        QStringList stale;
        // 收集"看起来是该文件导入产物"的子电路 ID

        for (auto it = m_subNames.constBegin(); it != m_subNames.constEnd(); ++it) {
            // 遍历所有子电路名

            if (it.value().endsWith(suffix)) stale.append(it.key());
            // 【条件】名字以该后缀结尾 → 认定为上次导入的残留

        }
        // 扫描结束

        if (!stale.isEmpty()) {
            // 【条件】确实发现了残留

            pushUndoAll();
            // 清理之前先压一次全量撤销快照

            QSet<QString> staleSet;
            // 把残留 ID 转成集合

            for (const auto& sid : stale) staleSet.insert(sid);
            // 逐个插入集合

            for (const auto& sid : stale) {
                // 逐个删除残留子电路定义与名称

                m_subContexts.remove(sid);
                // 删除定义

                m_subNames.remove(sid);
                // 删除名称

            }
            // 删除结束

            auto removeInstances = [&](Context& ctx) {
                // 定义局部 lambda：从某上下文中删除引用了残留子电路的实例及连线

                for (int i = ctx.components.size() - 1; i >= 0; --i) {
                    // 倒序遍历元件

                    auto c = ctx.components[i].toMap();
                    // 取元件 map

                    if (c.value("type").toString() != "sub") continue;
                    // 【跳过条件】非 sub 跳过

                    if (!staleSet.contains(c.value("subId").toString())) continue;
                    // 【跳过条件】引用的不是残留子电路则跳过

                    QString id = c.value("id").toString();
                    // 记录实例 ID

                    for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                        // 倒序遍历连线

                        auto w = ctx.wires[j].toMap();
                        // 取连线 map

                        if (w.value("fromComp").toString() == id ||
                            w.value("toComp").toString() == id)
                            // 【条件】源端/目标端是待删实例

                            ctx.wires.removeAt(j);
                        // 删除该线

                    }
                    // 内层循环结束

                    ctx.components.removeAt(i);
                    // 删除该实例元件

                }
                // 元件循环结束

            };
            // lambda 定义结束

            removeInstances(m_root);
            // 清理根上下文

            for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
                // 遍历剩余子电路继续清理

                removeInstances(it.value());
            // 清理该子电路

            m_ctxSubInstCache.clear();
            // 清空实例缓存

        } else {
            // 【分支】没有残留

            pushUndoAll();
            // 仍然压一次全量撤销快照

        }
        // residual 分支结束

    }
    // 已导入分支结束

    QString prefix;
    // 准备前缀字符串

    do {
        // do-while 循环：至少执行一次，直到生成的前缀未被占用

        prefix = QString("IMP%1_").arg(++m_importCounter);
        // 前缀自增后格式化为 "IMP<n>_"

    } while (m_subContexts.contains(prefix + "ROOT"));
    // 若该前缀的 ROOT 已存在则继续自增重试

    QHash<QString, QString> subIdMap;
    // 建立"导入文件内的旧子电路 ID → 本文件内的新子电路 ID"映射

    subIdMap.insert(QString(), prefix + "ROOT");
    // 为"空 ID"（即导入文件的根上下文）建立映射

    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it)
        // 遍历导入文件里的每个子电路

        subIdMap.insert(it.key(), prefix + it.key());
    // 新 ID = 前缀 + 旧 ID

    auto processCtx = [&](CircuitContext ctx) -> CircuitContext {
        // 定义 lambda：对一个按值传入的上下文重写所有 ID，返回修改后的副本

        for (int i = 0; i < ctx.components.size(); ++i) {
            // 遍历该上下文的所有元件，逐个改 ID

            QVariantMap c = ctx.components[i].toMap();
            // 取元件 map

            QString oldId = c.value("id").toString();
            // 记录旧 ID

            c["id"] = prefix + oldId;
            // 把元件 ID 改成 "前缀+旧ID"

            if (c.value("type").toString() == "sub") {
                // 【条件】该元件是子电路实例时，还要改写它的 subId

                QString oldSubId = c.value("subId").toString();
                // 取旧的 subId

                if (subIdMap.contains(oldSubId))
                    // 【条件】映射表里有这个旧 ID 才改写

                    c["subId"] = subIdMap.value(oldSubId);
                // 替换为新 ID

            }
            // subId 改写结束

            ctx.components[i] = c;
            // 写回元件

        }
        // 元件遍历结束

        for (int i = 0; i < ctx.wires.size(); ++i) {
            // 遍历所有连线，改写 ID 与两端引用

            QVariantMap w = ctx.wires[i].toMap();
            // 取连线 map

            w["id"] = prefix + w.value("id").toString();
            // 连线自身 ID 前缀化

            if (w.contains("fromComp"))
                // 【条件】有源元件字段才改写

                w["fromComp"] = prefix + w.value("fromComp").toString();
            // 源元件 ID 前缀化

            if (w.contains("toComp"))
                // 【条件】有目标元件字段才改写

                w["toComp"] = prefix + w.value("toComp").toString();
            // 目标元件 ID 前缀化

            ctx.wires[i] = w;
            // 写回连线

        }
        // 连线遍历结束

        ctx.compCounter = ctx.components.size();
        // 重置元件 ID 计数器为当前元件数量

        ctx.wireCounter = ctx.wires.size();
        // 重置连线计数器

        return ctx;
        // 返回前缀化后的上下文副本

    };
    // processCtx lambda 结束

    QStringList newSubIds;
    // 收集本次导入产生的所有新子电路 ID

    {
        // 用一个复合语句块包住"导入根电路"的逻辑

        CircuitContext ctx = processCtx(dec.root);
        // 把导入文件的根上下文前缀化

        QString newId = subIdMap.value(QString());
        // 取根对应的新 ID

        m_subContexts[newId] = ctx;
        // 把新子电路写入定义表

        m_subNames[newId] = QString("主电路（%1）").arg(baseName);
        // 命名："主电路（文件名）"

        newSubIds.append(newId);
        // 记录进新 ID 列表

    }
    // 根电路导入块结束

    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it) {
        // 遍历导入文件里的每个子电路，逐个导入

        CircuitContext ctx = processCtx(it.value());
        // 前缀化该子电路

        QString newId = subIdMap.value(it.key());
        // 查它的新 ID

        m_subContexts[newId] = ctx;
        // 写入定义表

        QString oldName = dec.subNames.value(it.key(), it.key());
        // 取导入文件里原有的显示名

        m_subNames[newId] = QString("%1（%2）").arg(oldName, baseName);
        // 新名称 = "原名（文件名）"

        newSubIds.append(newId);
        // 记录新 ID

    }
    // 子电路导入循环结束

    QVariantMap info;
    // 构造导入记录

    info["path"] = path;
    // 记录源文件路径

    info["prefix"] = prefix;
    // 记录本次使用的唯一前缀

    info["baseName"] = baseName;
    // 记录基名

    info["subIds"] = newSubIds;
    // 记录该文件产生的全部新子电路 ID

    m_importedFiles[path] = info;
    // 把记录登记到导入表

    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        // 扫描全部子电路（含已有的，不只新导入的）

        for (const auto& cv : it.value().components) {
            // 遍历该子电路的元件

            QVariantMap c = cv.toMap();
            // 转成 map

            if (c.value("type").toString() == "clock") {
                // 【条件】是时钟元件

                if (!m_clockStates.contains(c.value("id").toString()))
                    // 【条件】状态表里还没有该 ID

                    m_clockStates[c.value("id").toString()] = false;
                // 新时钟初始状态设为 false

            }
            // 时钟判断结束

        }
        // 元件遍历结束

    }
    // 时钟扫描结束

    m_ctxSubInstCache.clear();
    // 清空实例缓存

    evaluateAll();
    // 对整个电路重新求值

    emit subcircuitsChanged();
    // 通知子电路列表变化

    emit editContextsChanged();
    // 通知可编辑上下文列表变化

    emit changed();
    // 通知通用数据变化

    emit structureChanged();
    // 通知结构变化

    return true;
    // 导入成功

}
// importFromFile 结束
// ——

bool Circuit::removeImportedFile(const QString& path) {
    // 定义 removeImportedFile：把某次导入产生的全部子电路与实例卸载掉

    if (m_viewOnly) return false;
    // 【提前返回】浏览模式禁止

    if (!m_importedFiles.contains(path)) return false;
    // 【提前返回】没导入过这个文件 → 无事可做

    pushUndoAll();
    // 压一次全量撤销快照

    QString prefix = m_importedFiles.value(path).value("prefix").toString();
    // 取出该次导入的前缀

    removeSubContextsByPrefix(prefix);
    // 按前缀删除所有相关子电路与实例

    m_importedFiles.remove(path);
    // 移除导入记录

    evaluateAll();
    // 重新求值

    emit subcircuitsChanged();
    // 通知子电路列表变化

    emit editContextsChanged();
    // 通知可编辑上下文列表变化

    emit changed();
    // 通知通用数据变化

    emit structureChanged();
    // 通知结构变化

    return true;
    // 返回成功

}
// removeImportedFile 结束
// ——

QVariantList Circuit::importedFiles() const {
    // 定义 importedFiles：返回已导入文件的信息列表，供 UI 展示与卸载

    QVariantList out;
    // 结果列表

    for (auto it = m_importedFiles.constBegin(); it != m_importedFiles.constEnd(); ++it) {
        // 遍历导入记录

        QVariantMap m;
        // 构造一条对外记录

        m["path"] = it.value().value("path");
        // 复制路径

        m["prefix"] = it.value().value("prefix");
        // 复制前缀

        m["baseName"] = it.value().value("baseName");
        // 复制基名

        m["subCount"] = it.value().value("subIds").toList().size();
        // 计算该文件产生了多少个子电路

        out.append(m);
        // 追加到结果

    }
    // 遍历结束

    return out;
    // 返回列表

}
// importedFiles 结束
// ——

// ============================================================================
// ⑥ 撤销重做与最近文件
// ============================================================================

UndoEntry Circuit::captureUndoEntry(UndoScope scope, const QString& ctxId,
                                    const QString& mergeKey) const
// 定义 captureUndoEntry：按指定范围捕获一份"当前状态的快照"

{
    // 函数体开始

    UndoEntry e;
    // 创建快照条目

    e.scope = scope;
    // 记录范围

    e.mergeKey = mergeKey;
    // 记录合并键

    e.time = QDateTime::currentDateTime();
    // 记录快照时间

    if (scope == UndoScope::CurrentCtx) {
        // 【分支 1】当前上下文范围

        e.ctxId = ctxId;
        // 记录捕获的是哪个上下文

        if (ctxId.isEmpty()) e.singleCtx = m_root;
        // 【子分支】空 ID 表示根上下文，直接拷贝 m_root

        else {
            // 【子分支】非空 ID：从子电路表里取

            auto it = m_subContexts.constFind(ctxId);
            // 定位该子电路

            if (it != m_subContexts.constEnd()) e.singleCtx = it.value();
            // 【条件】存在才拷贝

        }
        // 子分支结束

    } else if (scope == UndoScope::Full) {
        // 【分支 2】全量范围

        e.allContexts.insert(QString(), m_root);
        // 把根上下文以空串 key 存入 allContexts

        for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it)
            // 遍历所有子电路

            e.allContexts.insert(it.key(), it.value());
        // 逐个拷入 allContexts

        e.subNames = m_subNames;
        // 保存子电路名称表

        e.subCounter = m_subCounter;
        // 保存子电路计数器

        e.clockStates = m_clockStates;
        // 保存时钟状态

        e.importedFiles = m_importedFiles;
        // 保存导入记录

    } else {
        // 【分支 3】MetaOnly（只保存元信息）

        e.subNames = m_subNames;
        // 保存名称表

        e.subCounter = m_subCounter;
        // 保存计数器

    }
    // 范围分支结束

    return e;
    // 返回快照

}
// captureUndoEntry 结束
// ——

UndoEntry Circuit::captureSameScopeAs(const UndoEntry& ref) const {
    // 定义 captureSameScopeAs：按"参考快照的范围"捕获当前状态

    return captureUndoEntry(ref.scope, ref.ctxId, QString());
    // 用参考快照的 scope 与 ctxId 调用 captureUndoEntry，mergeKey 传空

}
// captureSameScopeAs 结束
// ——

void Circuit::applyUndoEntry(const UndoEntry& e) {
    // 定义 applyUndoEntry：把一份快照写回当前状态，并重新求值、刷新界面

    QString savedCtx = m_currentCtxId;
    // 暂存当前上下文 ID

    bool savedView = m_viewOnly;
    // 暂存只读浏览标志

    if (e.scope == UndoScope::CurrentCtx) {
        // 【分支 1】当前上下文快照

        if (e.ctxId.isEmpty()) m_root = e.singleCtx;
        // 【子分支】快照属于根 → 覆盖 m_root

        else m_subContexts[e.ctxId] = e.singleCtx;
        // 【子分支】快照属于某子电路 → 覆盖对应项

    } else if (e.scope == UndoScope::Full) {
        // 【分支 2】全量快照

        auto it = e.allContexts.constFind(QString());
        // 在快照的 allContexts 里找根上下文

        if (it != e.allContexts.constEnd()) m_root = it.value();
        // 【条件】找到了才覆盖根

        m_subContexts.clear();
        // 先清空当前所有子电路

        for (auto it2 = e.allContexts.constBegin(); it2 != e.allContexts.constEnd(); ++it2) {
            // 遍历快照里的所有上下文

            if (!it2.key().isEmpty())
                // 【条件】跳过空 key

                m_subContexts.insert(it2.key(), it2.value());
            // 插入子电路

        }
        // 填充循环结束

        m_subNames = e.subNames;
        // 恢复名称表

        m_subCounter = e.subCounter;
        // 恢复计数器

        m_clockStates = e.clockStates;
        // 恢复时钟状态

        m_importedFiles = e.importedFiles;
        // 恢复导入记录

    } else {
        // 【分支 3】MetaOnly

        m_subNames = e.subNames;
        // 恢复名称表

        m_subCounter = e.subCounter;
        // 恢复计数器

    }
    // 范围分支结束

    if (!savedCtx.isEmpty() && !m_subContexts.contains(savedCtx)) {
        // 【条件】原本所在的上下文在新状态里已不存在

        savedCtx = "";
        // 改写为根

        savedView = false;
        // 同时退出浏览态

    }
    // 位置修复结束

    bool ctxChanged = (savedCtx != m_currentCtxId) || (savedView != m_viewOnly);
    // 判断这次恢复是否改变了"用户所在的上下文位置"

    m_currentCtxId = savedCtx;
    // 写回上下文 ID

    m_viewOnly = savedView;
    // 写回浏览标志

    m_engineDirty = true;
    // 标记引擎拓扑已失效

    if (m_engineType == "event") {
        // 【分支】事件引擎模式

        rebuildEventEngine();
        // 重建拓扑

        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        // 读线延迟

        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        // 读全局门延迟

        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 跑完初始事件
        // ★ runEventEngineFully 内部有总事件预算 3 万，振荡电路不会在此卡死。

        applyEngineOutputsToContexts();
        // 把结果写回上下文

    } else {
        // 【分支】迭代引擎模式

        evaluateAll();
        // 全量迭代求值

    }
    // 引擎分支结束

    if (ctxChanged) emit contextChanged();
    // 【条件】位置真的变了才发 contextChanged

    emit subcircuitsChanged();
    // 通知子电路列表变化

    emit editContextsChanged();
    // 通知可编辑上下文列表变化

    emit errorsChanged();
    // 通知错误列表变化

    emit clockChanged();
    // 通知时钟状态变化

    emit structureChanged();
    // 通知结构变化

    emit changed();
    // 通知通用数据变化

}
// applyUndoEntry 结束
// ——

void Circuit::pushUndoInternal(UndoScope scope, const QString& ctxId,
                               const QString& mergeKey)
// 定义 pushUndoInternal：所有 pushUndo* 变体的统一实现

{
    // 函数体开始

    CircuitUndoData* d = und();
    // 取得撤销数据指针

    if (!d) return;
    // 【提前返回】没有撤销数据

    if (m_viewOnly) return;
    // 【提前返回】浏览模式下不产生撤销记录

    if (d->restoring) return;
    // 【提前返回】正在应用快照时不入栈

    if (d->redoRunning) return;
    // 【提前返回】重做进行中不入栈

    if (!mergeKey.isEmpty() && mergeKey == d->lastMergeKey) {
        // 【条件】合并判断

        if (d->lastUndoTime.isValid() &&
            d->lastUndoTime.msecsTo(QDateTime::currentDateTime()) < 500) {
            // 【条件】距上次入栈不到 500 毫秒

            d->lastUndoTime = QDateTime::currentDateTime();
            // 刷新时间戳到"现在"

            return;
            // 【提前返回】跳过本次入栈

        }
        // 时间窗口判断结束

    }
    // 合并判断结束

    UndoEntry e = captureUndoEntry(scope, ctxId, mergeKey);
    // 按范围捕获当前状态

    d->undoStack.push_back(e);
    // 把快照压入撤销栈尾部

    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        // 当撤销栈超过上限时，从开头裁剪

        d->undoStack.erase(d->undoStack.begin());
    // 删除最早的一条

    d->redoStack.clear();
    // 清空重做栈

    d->lastMergeKey = mergeKey;
    // 记录本次的合并键

    d->lastUndoTime = QDateTime::currentDateTime();
    // 记录本次入栈时间

    emit undoRedoChanged();
    // 通知撤销/重做状态变化

}
// pushUndoInternal 结束
// ——

void Circuit::pushUndo(const QString& mergeKey) {
    // 定义 pushUndo：为"当前上下文内的编辑"压快照

    pushUndoInternal(UndoScope::CurrentCtx, m_currentCtxId, mergeKey);
    // 以 CurrentCtx 范围、当前上下文 ID 调用统一实现

}
// pushUndo 结束
// ——

void Circuit::pushUndoAll(const QString& mergeKey) {
    // 定义 pushUndoAll：为"跨上下文的结构性操作"压全量快照

    pushUndoInternal(UndoScope::Full, QString(), mergeKey);
    // 以 Full 范围调用统一实现

}
// pushUndoAll 结束
// ——

void Circuit::pushUndoMeta(const QString& mergeKey) {
    // 定义 pushUndoMeta：为"只改元信息"的操作压快照

    pushUndoInternal(UndoScope::MetaOnly, QString(), mergeKey);
    // 以 MetaOnly 范围调用统一实现

}
// pushUndoMeta 结束
// ——

void Circuit::breakUndoMerge() {
    // 定义 breakUndoMerge：打断撤销合并窗口

    CircuitUndoData* d = und();
    // 取得撤销数据指针

    if (!d) return;
    // 【提前返回】没有撤销数据

    d->lastMergeKey.clear();
    // 清空上次的合并键

    d->lastUndoTime = QDateTime();
    // 把上次入栈时间重置为无效时间

}
// breakUndoMerge 结束
// ——

void Circuit::undo() {
    // 定义 undo：撤销一步

    CircuitUndoData* d = und();
    // 取得撤销数据

    if (!d) return;
    // 【提前返回】无数据

    if (m_viewOnly) return;
    // 【提前返回】浏览模式禁止撤销

    if (d->redoRunning) return;
    // 【提前返回】重做进行中禁止撤销

    if (d->undoStack.empty()) return;
    // 【提前返回】撤销栈为空

    d->redoRunning = true;
    // 置位"重做进行中"标志

    UndoEntry toUndo = d->undoStack.back();
    // 取撤销栈顶快照

    d->undoStack.pop_back();
    // 弹出栈顶

    UndoEntry cur = captureSameScopeAs(toUndo);
    // 以被撤销快照的范围捕获当前状态

    d->redoStack.push_back(cur);
    // 当前状态压入重做栈

    while (d->redoStack.size() > static_cast<size_t>(d->maxUndo))
        // 重做栈同样受上限约束

        d->redoStack.erase(d->redoStack.begin());
    // 删除最早的重做项

    breakUndoMerge();
    // 打断合并窗口

    d->restoring = true;
    // 进入"恢复中"状态

    applyUndoEntry(toUndo);
    // 应用旧快照

    d->restoring = false;
    // 退出恢复状态

    d->redoRunning = false;
    // 释放互斥锁

    emit undoRedoChanged();
    // 通知栈状态变化

}
// undo 结束
// ——

void Circuit::redo() {
    // 定义 redo：重做一步

    CircuitUndoData* d = und();
    // 取得撤销数据

    if (!d) return;
    // 【提前返回】无数据

    if (m_viewOnly) return;
    // 【提前返回】浏览模式禁止

    if (d->redoRunning) return;
    // 【提前返回】已在进行中

    if (d->redoStack.empty()) return;
    // 【提前返回】没有可重做的项

    d->redoRunning = true;
    // 加锁

    UndoEntry toRedo = d->redoStack.back();
    // 取重做栈顶

    d->redoStack.pop_back();
    // 弹出

    UndoEntry cur = captureSameScopeAs(toRedo);
    // 以相同范围捕获当前状态

    d->undoStack.push_back(cur);
    // 压入撤销栈

    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        // 撤销栈超限裁剪

        d->undoStack.erase(d->undoStack.begin());
    // 删除最老的一条

    breakUndoMerge();
    // 打断合并窗口

    d->restoring = true;
    // 进入恢复态

    applyUndoEntry(toRedo);
    // 应用快照

    d->restoring = false;
    // 退出恢复态

    d->redoRunning = false;
    // 释放锁

    emit undoRedoChanged();
    // 通知栈状态变化

}
// redo 结束
// ——

void Circuit::loadRecentFiles() { m_recentFiles = RecentStore::load(); }
// 定义 loadRecentFiles：从持久化存储读取最近文件列表

void Circuit::saveRecentFiles() { RecentStore::save(m_recentFiles); }
// 定义 saveRecentFiles：把当前列表写回持久化存储

void Circuit::addRecentFile(const QString& path) {
    // 定义 addRecentFile：把一次"打开/保存"记录到最近文件列表

    if (path.isEmpty()) return;
    // 【提前返回】空路径不记录

    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        // 倒序遍历现有列表，删除所有同路径的旧记录

        if (m_recentFiles[i].toMap().value("path").toString() == path)
            // 【条件】第 i 项的路径与要添加的相同

            m_recentFiles.removeAt(i);
        // 删除该旧项

    }
    // 去重结束

    QVariantMap m;
    // 构造新记录

    m["path"] = path;
    // 保存完整路径

    QFileInfo fi(path);
    // 构造 QFileInfo 用于提取文件名

    m["name"] = fi.fileName();
    // 取纯文件名

    m["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    // 记录当前时间

    m_recentFiles.prepend(m);
    // 插入到列表最前面

    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    // 超出 10 条时丢弃最旧的

    saveRecentFiles();
    // 持久化

    QMetaObject::invokeMethod(this, [this]() {
        // 通过事件循环投递一个 lambda，稍后在对象所属线程中执行

        emit recentFilesChanged();
        // 在下一轮事件循环里真正发出信号

    }, Qt::QueuedConnection);
    // 指定连接类型为排队连接：跨事件循环调用

}
// addRecentFile 结束
// ——

void Circuit::removeRecentFile(const QString& path) {
    // 定义 removeRecentFile：从最近文件列表中删除指定路径

    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        // 倒序遍历并删除所有匹配项

        if (m_recentFiles[i].toMap().value("path").toString() == path)
            // 【条件】路径匹配

            m_recentFiles.removeAt(i);
        // 删除

    }
    // 遍历结束

    saveRecentFiles();
    // 持久化

    emit recentFilesChanged();
    // 通知 UI 刷新

}
// removeRecentFile 结束
// ——

void Circuit::clearRecentFiles() {
    // 定义 clearRecentFiles：清空整个最近文件列表

    m_recentFiles.clear();
    // 清空内存列表

    saveRecentFiles();
    // 持久化空列表

    emit recentFilesChanged();
    // 通知 UI 刷新为空

}
// clearRecentFiles 结束
// 本文件到此结束