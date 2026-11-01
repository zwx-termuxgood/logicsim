
#include "../Circuit.h"
//包含 Circuit 自身的类声明（src/Circuit.h），本文件所有函数签名、成员变量都必须先有完整声明才能定义
// .cpp 的第一件事永远是包含自己所属类的头文件：成员访问、Qt moc 生成代码都依赖它；路径用 ../ 表示"从子目录 src/circuit/ 回到 src/"

#include "../core/Component.h"
// 引入元件基础层 Component 的定义，提供元件类型判断等工具
// 本文件处理"元器件 map"的通用逻辑（updateOneComponent 要按 type 区分 input/output/led/sub）时依赖该层；Component.h 在本包中未提供，按其被引用的调用方式推断为元件类型/元数据的轻量封装

#include "../core/ValueCodec.h"
// 引入数值编解码器：二进制串与各进制字符串互转
// 虽然本文件没有直接调用 ValueCodec 的函数，但电路值的表示（QVariantList 位列表 / QString 二进制串）由它定义，JsonCodec 等下游层依赖它，纳入本 TU 可保证类型与接口一致


#include "../logic/ComponentLogic.h"
// 引入元件逻辑层：outputBitWidth / inputBitWidth / outputPorts / inputPorts 等按类型计算端口几何与位宽的纯函数
// refreshErrors 要判断"源端口位宽 vs 目标端口位宽"，位宽不是存在 map 里的固定字段，而是由元件类型与配置推算出来的，必须走这一层

#include "../logic/WireNetworkSolver.h"
// 引入线网求解器：generateVirtualWires 从既有导线推导出"虚拟连接"，connectionDots 计算三通接头圆点
// 事件引擎要的是完整的连接关系，而 ortho 线只记录两端点坐标、bezier 线只记录端口索引，靠 generateVirtualWires 把两者统一补全后再交给引擎


#include "../logic/iter/IterEngine.h"
// 引入迭代引擎：evaluateAll / evaluateStandalone / syncSubPins 三个静态入口
// "iter" 引擎模式（以及子电路浏览时的局部求值）全靠它；syncSubPins 还负责把子电路定义的端口方向同步给所有实例


#include "../logic/event/EventEngine.h"
// 引入事件驱动引擎 EventEngine 的完整定义（build / run / step / isIdle / getOutputs / triggerInputString / takeDirtyQids 等）
// Circuit.h 里只有 class EventEngine 的前置声明，一旦在本文件里调用它的成员函数就必须看到完整定义；事件引擎是本包默认引擎，本文件大量与之交互


#include "../io/JsonCodec.h"
// 引入序列化编解码器：encode 把上下文树编码为 QVariantMap，decode 反向还原为 DecodedData
// 本文件负责 toJson/fromJson/saveToFile/loadFromFile/importFromFile，Circuit 自身不关心 JSON 字段格式，全部委托这一层，保证"文件格式"只有一处定义


#include "../io/RecentStore.h"
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
// saveToFile/loadFromFile/importFromFile 都用它；注意本文件用的是直接 QFile 写盘，并未使用 QSaveFile


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
    //        const 是因为纯查询、不修改任何成员，也据此保证内部缓存（m_ctxSubInstCache 是 mutable）以外不产生副作用


    QVariantList out;
    // 创建输出列表 out，最终作为返回值返回
    // 先把所有子电路的摘要攒起来再一次性 return，避免边遍历边 return；QVariantList 每个元素是 QVariantMap


    for (auto it = m_subNames.begin(); it != m_subNames.end(); ++it) {
        // 遍历 m_subNames（QHash<QString,QString>：子电路 ID -> 显示名），it.key() 是 ID，it.value() 是名字
        // 子电路的"存在性"由 m_subNames 定义（m_subContexts 才是内容），按名字表遍历能保证列表里每个条目都有显示名；
        //        QHash 遍历顺序不确定，这是可接受的：UI 侧不需要稳定顺序，需要稳定顺序的地方见 editContexts 里先排序

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
        //        初值 0 保证即便上下文缺失（ctx 为空）也能返回合法的 0/0


        if (ctx) {
            // 【分支】只有上下文存在才统计
            // 防御空指针：ID 与名称表不一致（例如正在删除、或旧存档缺上下文）时不能崩溃，返回 0/0 即可


            for (auto& cv : ctx->components) {
                // 遍历该子电路里的所有元件（每个元素是 QVariant 包装的 QVariantMap）
                // 子电路的"端口"不是单独存的，而是靠里面的 type=="input"/"output" 元件体现，所以要扫全部元件


                QString t = cv.toMap().value("type").toString();
                // 把 QVariant 转成 QVariantMap，取出 "type" 字段并转为 QString
                // toMap() 会产生一次拷贝，但这只是查询函数，可读性优先；value() 而非 [] 是因为 QVariantMap::value 在 key 不存在时返回默认 QVariant 而不会插入空项

                if (t == "input") inCount++;
                // 【分支】类型为 input 则输入计数 +1
                // 输入元件（input）是子电路的输入端口来源，统计它就等于统计端口数

                else if (t == "output") outCount++;
                // 【分支】类型为 output 则输出计数 +1
                // output 元件是子电路的输出端口来源；用 else if 而不是两个独立 if，避免未来出现同时是两者的类型时重复计数


            }
            // 元件遍历体结束
            // 循环体只有上面的分支判断，结束无额外逻辑

        }
        // if (ctx) 判断块结束
        // 统计完成后两个计数器要么是 0/0，要么是真实数量


        m["inCount"] = inCount;
        // 把输入端口数写入摘要
        // 键名是 QML 侧约定的接口名，不能随意改


        m["outCount"] = outCount;
        // 把输出端口数写入摘要
        // 同上，固定键名
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
// 每个函数定义都用显式收尾花括号，与头文件声明一一对应

QVariantList Circuit::editContexts() const {
    // 定义 editContexts()：返回"可编辑上下文"列表 = 根上下文 + 所有子电路
    // 界面上有一个"当前正在编辑哪个电路"的下拉框：根电路（主电路）和每个子电路都必须是可切换的目标；
    //        与 subcircuits() 的区别：subcircuits() 是"有哪些子电路"（带端口统计），editContexts() 是"能在哪些上下文里编辑"（带根条目、且顺序稳定）


    QVariantList out;
    // 结果列表
    // 根条目 + 子电路条目会依次追加到此


    QVariantMap rootMap;
    // 为"根上下文"创建一条记录
    // 根上下文不是一个子电路，不会被 m_subNames 覆盖，必须手工补一条，否则下拉框里选不回根电路


    rootMap["id"] = "";
    // 根上下文的 ID 约定为空串
    // 整个工程统一约定"空串 = 根"（见 Circuit.h 的 isRootContext()，以及本文件大量 m_currentCtxId.isEmpty() 判断），
    //        这里必须用同一个约定，保证 switchEditContext("") 能正确切回根

    rootMap["name"] = "主电路";
    // 根上下文的显示名固定为中文"主电路"
    // 显示名只给 UI 看，根电路没有 m_subNames 条目可查，所以在这里硬编码（与 refreshErrors 里 checkCtx(m_root,"主电路") 的标签一致）


    out.append(rootMap);
    // 把根条目放在列表最前面
    // 根电路是最常用、默认的编辑目标，放第一位符合下拉框的操作习惯

    QStringList ids = m_subNames.keys();
    // 取出所有子电路 ID 组成 QStringList
    // QHash::keys() 每次都会新建容器并遍历，属于一次显式拷贝；拿到 keys 后就能对它排序而不破坏原哈希表的遍历顺序


    std::sort(ids.begin(), ids.end());
    // 对 ID 列表做升序排序（字典序）
    // QHash 遍历顺序不确定，如果不同步排序，下拉框的条目会在每次重建时乱跳；排序后 "S1,S2,S10,S11" 虽不是自然数序，但至少稳定可预期

    for (auto& id : ids) {
        // 按排序后的顺序遍历每个子电路 ID
        // 用引用是为了下面直接赋值/读取而不拷贝 QString（Qt5 中 QString 本身是隐式共享，代价不大，但语义更清晰）


        QVariantMap m;
        // 为当前子电路创建一条记录
        // 键与根条目保持一致（id/name），这样 QML 可以用同一套模型（delegate）渲染所有条目


        m["id"] = id;
        // 写入子电路 ID
        // 用户在下拉框里选中后，QML 直接把 id 传给 switchEditContext(ctxId)


        m["name"] = m_subNames.value(id);
        // 从名称表取值写入显示名
        // 用 value() 而不是 operator[]：const 函数里不能调用非 const 的 operator[]（它会插入默认值），
        //        且 m_subNames.value(id) 在 id 必然存在（来自 keys()）时安全


        out.append(m);
        // 追加该子电路条目
        // 与根条目使用同一个列表，保证 QML 模型一致


    }
    // 子电路遍历结束
    // 结果顺序：["主电路", S1, S2, ...] 升序


    return out;
    // 返回完整列表
    // 按值返回（隐式共享不额外深拷贝）

}
// editContexts() 结束
// ——


// ============================================================================
// ② 求值生命周期：引擎切换、全量求值
// ============================================================================

void Circuit::setEngineType(const QString& t) {
    // 定义 setEngineType：切换底层求值引擎（"iter" 迭代引擎 或 "event" 事件驱动引擎），切换后立即把新引擎跑出结果并刷新界面
    // 是 Q_PROPERTY 的 WRITE 函数（见 Circuit.h 第 73 行），QML 里直接给 engineType 赋值就会走这里；
    //        两种引擎的数据模型完全不同（迭代引擎每次全量重算；事件引擎维护事件队列与仿真时间），
    //        所以"切换"不能只改个字符串，必须把状态迁移过去


    if (m_engineType == t) return;
    // 【提前返回】如果目标类型与当前一致，直接返回，不做任何事
    // 避免无意义的重建与信号风暴：QML 属性赋值、设置同步可能重复写入同一个值，若不判等会引发多余的 rebuildEventEngine + changed 信号（进而界面重绘）

    if (t != "iter" && t != "event") return;
    // 【校验】只接受 "iter" 与 "event" 两个字面值，其它参数（含空串、拼写错误）一律忽略
    // 引擎类型是驱动整个仿真逻辑的关键开关，非法值一旦写入，后续所有 if (m_engineType == "event") 判断都会走到"都不是"的状态，
    //        造成界面显示与真实引擎不一致，所以这里做白名单校验


    if (m_simulationRunning) pauseSimulation();
    // 若仿真正在运行，先暂停
    // 引擎会被重建/替换，运行中的仿真定时器若继续 tick，会访问正在被替换的引擎状态；
    //        先 pauseSimulation() 停掉 m_simTimer 并同步 m_simulationRunning=false，保证切换期间没有并发推进


    if (m_clockRunning) stopClock();
    // 若时钟定时器在跑，先停止
    // 与上一条同理：时钟 tick 会翻转 clock 元件并驱动求值，引擎正被替换时同样不安全；
    //        stopClock() 会停掉 m_clockTimer 并把 m_clockRunning 置 false


    m_engineType = t;
    // 真正写入新的引擎类型
    // 前面的副作用已经停干净了，此刻才改状态；后面的代码全部基于这个新值分派


    saveSettings();
    // 把新引擎类型持久化到 QSettings
    // 引擎选择属于用户偏好，下次启动要沿用（loadSettings 会读回）；放在切换动作之前保存，确保即便后续求值抛错也已经记住用户选择


    if (m_engineType == "event") {
        // 【分支 A】切换到事件驱动引擎：重建引擎并把初始事件跑完
        // 事件引擎是"惰性"的：build() 只建拓扑，引擎内部的事件队列里还没有算出来的输出；
        //        必须 run 一次让所有的门节点至少算出一轮，否则 applyEngineOutputsToContexts 会把"空输出"写回元件


        rebuildEventEngine();
        // 用当前所有上下文（根 + 子电路）的元件与线重新构建事件引擎拓扑
        // 事件引擎不感知上层数据结构，切换后必须整体重建；rebuild 内部会收集 wire、生成虚拟线、调用 EventEngine::build，并清空实例缓存

        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        // 读取事件引擎当前的"线延迟"（每个信号穿过一根线所需的仿真时间单位），引擎为空时退回 1
        // 后面要按延迟估算"跑多久才能收敛"，延迟越大收敛越慢；用三元表达式兜底，避免空指针解引用。
        //        注意：Circuit.h 里 wireDelay() 的声明是 qulonglong，而这里用 quint64 接收——在 Qt 中两者是同一底层类型（unsigned long long）的别名，不会产生截断


        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        // 读取"全局门延迟"（每个逻辑门被求值后的输出要延迟多久才有效），引擎为空时退回 1
        // 同上，用于估算收敛所需的仿真时间；门延迟与线延迟共同决定信号从输入传播到输出的最坏路径长度

        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 把事件引擎一直推进到空闲：advance = (线延迟 + 门延迟) × 64 + 200，budget = 500000 个事件
        // "×64" 是经验系数：假设最长的组合逻辑深度约 64 级，每级消耗一次 (线延迟+门延迟)，"+200" 留出余量；
        //        runEventEngineFully 内部会在一次 run 没跑完时把 advance 翻倍重试，直到引擎空闲或预算耗尽（详见该函数注释）


        applyEngineOutputsToContexts();
        // 把引擎算出来的每个节点输出写回元件 map（c["outputPorts"] 等），界面才有值可画
        // 这是"引擎结果 → UI 数据"的唯一出口；因为 rebuild 后 m_needFullApply 为 true，这里走的是全量刷新分支


    } else {
        // 【分支 B】切换到迭代引擎："iter" 没有需要重建的常驻对象，直接一次性全量求值即可
        // 迭代引擎是纯函数式的（IterEngine::evaluateAll 静态调用），不维护跨调用的状态，
        //        但必须先把"事件引擎留下的输出"先 apply 一次再迭代，否则事件引擎耗时算出的中间值会在迭代中被当作初始值


        applyEngineOutputsToContexts();
        // 先把事件引擎（若存在）最后一次的结果写回上下文
        // 保证"切走之前的状态"在数据里是完整的：事件引擎可能在切换前几帧就停止 apply 了，
        //        这里先补齐再交给迭代引擎做收敛的起点，避免迭代引擎从陈旧的输入开始


        evaluateAll();
        // 用迭代引擎做一次全量收敛求值
        // m_engineType 已经是 "iter"，evaluateAll 内部会走 IterEngine::evaluateAll 分支；
        //        它还会先 syncSubPins、再单独求值当前正在编辑的子电路，保证所有上下文的显示值都是收敛后的


    }
    // 分支结束
    // 两种引擎都已把结果写回上下文

    emit engineChanged();
    // 发出 engineChanged 信号
    // engineType 属性的 NOTIFY 就是这个信号，QML 据此刷新"引擎选择"相关控件


    emit engineSettingsChanged();
    // 发出 engineSettingsChanged 信号
    // 延迟、速度等引擎参数面板依赖此信号刷新；切换引擎后这些参数的可用性/取值可能变化（例如 iter 引擎不看延迟），故一并通知


    emit changed();
    // 发出通用数据变化信号
    // components/wires 等属性的 NOTIFY 都是 changed，切换后的输出值已变化，需要重绘画布

    emit structureChanged();
    // 发出结构变化信号
    // 事件引擎会把虚拟线算出的连接写回（以及 clean 状态），QML 中的连线排序/连接圆点等缓存需要重建


}
// setEngineType 结束
// ——

void Circuit::evaluateAll() {
    // 定义 evaluateAll：整个电路模型最核心的"求值"入口，负责把算法结果写进当前所有上下文的元件 map
    // 所有会改变逻辑的写操作（连线、改属性、粘贴、切换引擎、撤销……）最终都会落到这里：
    //        先保证子电路端口的元信息一致（syncSubPins），再按引擎类型选择"事件推进"或"迭代求值"，
    //        最后还要单独把"当前正在浏览的子电路"求值一遍，保证用户切进去看到的是它自己的逻辑结果


    IterEngine::syncSubPins(m_root, m_subContexts);
    // 同步根上下文中所有 sub 实例的端口定义：根据被引用子电路里 input/output 元件，生成该实例的输入/输出端口列表
    // 子电路的端口是"定义"在子电路里的，但"实例"画在父上下文里；父上下文里 sub 元件的端口 must 与定义保持一致，
    //        否则连线会连到不存在的端口，迭代引擎读到的端口索引也会错位。这一步就是"实例 ← 定义"的单向同步


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 对每个子电路上下文再次调用 syncSubPins，把"嵌套子电路的端口"同步到子电路内部的 sub 实例上
        // 子电路里也可以放别的子电路实例；m_root 的同步只覆盖根那一层，嵌套层必须逐个上下文处理，
        //        否则多层嵌套时内层实例的端口会停留在旧定义上。注意这一行与下一行合起来才是完整的 for 语句


        IterEngine::syncSubPins(it.value(), m_subContexts);
    // 对当前子电路上下文执行端口同步
    // it.value() 是引用，syncSubPins 直接修改它的元件；第二个参数仍传全局 m_subContexts，因为解析 subId 时要查定义表

    if (m_viewOnly && !m_currentCtxId.isEmpty())
        // 【条件】当处于"浏览子电路实例"的只读模式时，把父实例上的输入值同步进子电路上下文
        // 浏览模式下用户看的是"某一个实例"的子电路内部，它的输入元件（type=="input"）的值应该等于父上下文里该实例接线柱收到的值；
        //        若不先同步，子电路内部会按自己保存的旧值求值，显示结果与父电路整体仿真结果矛盾


    syncSubStateFromParent(m_currentCtxId);
    // 执行上述同步：从 m_currentInstancePath 指定的实例链，逐个 input 元件写入 inputBits / outputOverride
    // 单独拆成函数（定义在 Circuit_subcircuit.cpp），因为"找实例链 + 取输入值"逻辑较长，且初始化/进入子电路时也要复用


    if (m_engineType == "event") {
        // 【分支】事件引擎模式
        // 事件引擎有两种工作状态：仿真运行中（增量推进 + 局部 apply）与未运行（只保证数据一致、用迭代求值占位），处理方式完全不同


        if (m_simulationRunning && m_eventEngine) {
            // 【子分支】仿真正在运行且引擎实例存在
            // 这是"真正的增量仿真"路径：引擎在持续推进，结构改动后必须重建拓扑、把已有事件跑完，再把脏节点输出回写


            rebuildEventEngine();
            // 按当前结构重建引擎拓扑
            // evaluateAll 通常是被"结构改动"调起来的（增删元件/连线），旧拓扑已经失效；
            //        重建会把 m_engineDirty 置 false、m_needFullApply 置 true


            {
                // 开一个局部作用域，把"读延迟、跑引擎"两步包起来
                // 只是为了给后面的变量名一个独立作用域，避免与函数后面可能出现的同名变量冲突；不含逻辑


                quint64 wd = m_eventEngine->wireDelay();
                // 读取线延迟
                // rebuild 后引擎是新的，延迟参数可能被 setWireDelay 改过，必须重新读，不能用旧值


                quint64 gd = m_eventEngine->globalGateDelay();
                // 读取全局门延迟
                // 同上，用于估算收敛时间


                runEventEngineFully((wd + gd) * 64 + 200, 500000);
                // 把重建后引擎里堆积的初始事件全部跑完
                // ★ 关键：build() 之后事件队列里只有"初始重算事件"，如果不跑完就 apply，会把初始默认值（一般全 0）写回上下文，
                //        覆盖掉原本正确的输出——这正是 Circuit_simulation.cpp 里多处"★ 修复"注释所述的同一类坑


            }
            // 局部作用域结束
            // wd/gd 释放，不再需要


            applyEngineOutputsToContexts();
            // 把引擎输出写回上下文
            // 此时引擎已空闲（或预算耗尽），输出是稳定值；因为 m_needFullApply==true，这次 apply 是全量刷新，覆盖所有元件


        } else {
            // 【子分支】事件引擎模式，但仿真没在运行 / 引擎实例不存在
            // 事件引擎未运行时不会主动推进，若结构改动后什么都不做，界面上的输出值会停留在改动前；
            //        于是用一个"一次性迭代求值"来立即给出正确的组合逻辑结果，代价是丢弃事件引擎的历史时间轴

            m_engineDirty = true;
            // 标记引擎拓扑已失效
            // 结构确实变了（否则不会调用 evaluateAll），下次 startSimulation/onSimTimerTick 看到这个标志会先 rebuild 再推进，
            //        避免用陈旧拓扑跑仿真

            IterEngine::evaluateAll(m_root, m_subContexts);
            // 用迭代引擎求一次全量结果（对整棵上下文树）
            // 迭代引擎是组合逻辑的不动点迭代，很适合"无状态地算一遍"；它的输出与事件引擎在"输入稳定后"的结果一致


        }
        // 事件模式子分支结束
        // ——

    } else {
        // 【分支】迭代引擎模式（m_engineType != "event"）
        // 迭代引擎没有"运行/暂停"概念，每次都是全量重算，所以直接调用即可

        IterEngine::evaluateAll(m_root, m_subContexts);
        // 对根上下文与所有子电路做全量不动点迭代
        // 第二个参数是子电路定义表，用于解析 sub 实例内部逻辑；迭代引擎内部会先算子电路再算引用它的父层


    }
    // 引擎分支结束
    // ——


    bool eventSimOnCurrent = !m_viewOnly && m_engineType == "event" && m_simulationRunning;
    // 计算一个"当前上下文已经被事件引擎的 apply 覆盖过"的标志
    // applyEngineOutputsToContexts 在编辑模式下只刷新"当前编辑的上下文"（见该函数），所以：
    //        · 编辑模式 + 事件仿真 → 当前上下文的数据已经是权威结果，不需要也不应该再用迭代引擎覆盖
    //        · 浏览模式（m_viewOnly）→ apply 刷新的是根上下文，浏览中的子电路没被覆盖，仍需要单独求值
    //        · iter 模式 → apply 根本不存在，全局迭代已经算过，但当前子电路仍可能要单独求值以套用父实例输入

    if (!m_currentCtxId.isEmpty() && !eventSimOnCurrent) {
        // 【条件】当正在查看某个子电路上下文、且它没有被子事件引擎覆盖时，额外对它做一次"独立求值"
        // 用户切进子电路后看到的是该子电路本身（并不一定在某实例上下文中），它的 input 元件值可能来自 syncSubStateFromParent
        //        或来自自身保存的默认值；单独求值能保证"子电路内部逻辑 vs 它的输入"自洽，而不是显示父层的整体结果


        auto it = m_subContexts.find(m_currentCtxId);
        // 在子电路表中定位当前上下文
        // 用 find 而不是 operator[]：const 语义、找不到时返回 end() 便于防御，且不会插入空上下文


        if (it != m_subContexts.end())
            // 【条件】找到了才求值
            // 防御：m_currentCtxId 指向的子电路可能刚被删除（删除流程里会清空它，但存在时序窗口）

        IterEngine::evaluateStandalone(it.value(), m_subContexts);
        // 只对该子电路上下文求值一次（不带父实例上下文）
        // evaluateStandalone 与 evaluateAll 的区别：它忽略"这个子电路正被哪些实例引用"，
        //        只把该上下文当成一个独立电路来算，适合"用户正在单独看它"的场景，避免父层输入影响显示


    }
    // 当前子电路独立求值分支结束
    // ——

    emit subcircuitsChanged();
    // 发出子电路列表变化信号
    // 求值可能改变子电路端口定义（syncSubPins 会按定义重建实例端口），端口数统计会变，侧边栏需要刷新；
    //        这个信号放在函数末尾，保证所有数据都已是最新


}
// evaluateAll 结束
// ——


void Circuit::evaluate() {
    // 定义 evaluate：Q_INVOKABLE 的公开求值入口
    // QML / 外部代码不能直接调用 private 的 evaluateAll（见 Circuit.h 第 491 行，evaluateAll 是 private，
    //        evaluate 是 Q_INVOKABLE public 包装），这层包装把"实现细节"与"对外契约"分开


    evaluateAll();
    // 直接委托给 evaluateAll
    // 保持单一实现，避免两套求值路径；目前是纯转发，没有任何额外逻辑

}
// evaluate 结束
// ——


// ============================================================================
// ③ 事件引擎生命周期
// ============================================================================

void Circuit::rebuildEventEngine() {
    // 定义 rebuildEventEngine：把 Circuit 的上下文树"拍平"成事件引擎能理解的节点-边图，并重建引擎
    // 事件引擎只认识两种东西：元件节点（按 qualified id）与连接关系（线）。
    //        Circuit 的数据是分层的（根 + 多个子电路 + 嵌套实例），线还有 ortho/bezier/virtual 三种表示，
    //        本函数负责把它们合并成"一个平面图"再交给 EventEngine::build


    if (!m_eventEngine) return;
    // 【提前返回】引擎实例不存在时什么都不做
    // m_eventEngine 由构造函数创建、析构函数 delete；若处于构造/析构或加载失败阶段，它可能为空，调用成员前必须判空


    IterEngine::syncSubPins(m_root, m_subContexts);
    // 构建前先同步根上下文中 sub 实例的端口定义
    // 引擎构建会按端口索引建立连接，如果实例端口列表与子电路定义不一致，build 出来的边会连到错误端口，导致仿真结果全错


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 对每个子电路上下文同步其内部嵌套实例的端口
        // 与 evaluateAll 里完全相同的理由：多层嵌套时根层同步覆盖不到内层


        IterEngine::syncSubPins(it.value(), m_subContexts);
    // 执行该层同步
    // 第二个参数传完整定义表，供解析 subId 使用


    QHash<QString, QVariantList> subComps, subWires;
    // 准备两份"快照"：每个子电路 ID 对应的元件列表与连线列表
    // 事件引擎支持"实例化"：同一个 subId 被多个实例引用时，引擎需要拿到该子电路内部的结构来展开成多份节点；
    //        build 的签名要求分别传组件表与线表，所以这里先各拷一份出来


    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        // 遍历所有子电路，填充上述两份快照
        // 拷贝而不是传引用：EventEngine::build 的签名接收 by-value 的 QHash（或至少不承诺不修改），
        //        且 QVariantList 是隐式共享的，拷贝成本很低（只增引用计数），真正写入时才分裂


        subComps[it.key()] = it.value().components;
        // 把该子电路的元件列表存入快照，key 为 subId
        // 引擎按 "所在上下文 ID" 查表展开实例

        subWires[it.key()] = it.value().wires;
        // 把该子电路的连线列表存入快照
        // 同上；注意这里存的是**原始**线（不含虚拟线），虚拟线只对"被当作根来仿真"的那一层生成


    }
    // 快照填充结束
    // ——


    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    // 判断当前是否处于"把子电路当作根来编辑/仿真"的模式
    // 用户切进子电路编辑时（非浏览模式），希望仿真只围绕这个子电路进行，而不是整个根电路；
    //        这个布尔量决定下面"把哪一层当作顶层电路"交给引擎


    if (editingSubAsRoot) {
        // 【分支】编辑子电路作为根
        // 此分支下，交给引擎的"顶层"是子电路，根电路不参与仿真


        auto it = m_subContexts.find(m_currentCtxId);
        // 定位当前编辑的子电路
        // 用 find 并判 end()，防御 m_currentCtxId 悬空（子电路被删但标志未清）

        if (it != m_subContexts.end()) {
            // 【分支】找到了当前子电路
            // 找不到则直接掉到函数末尾的根电路分支（相当于退回主电路仿真），不会崩溃

            QVariantList allWires = it.value().wires;
            // 以该子电路的原始连线为起点，创建一个可追加的列表
            // QVariantList 隐式共享：这一步只是"拷贝句柄"，随后 append 虚拟线时才会真正分裂（写时复制），成本很低

            QVariantList vw = WireNetworkSolver::generateVirtualWires(
                // 根据该子电路的元件与连线，推导出"虚拟连接"列表，存入 vw
                // ortho 线（只存 path 顶点）在引擎看来没有端口信息；从线中间引出的 T 型连接也没有显式记录；
                //        生成虚拟线的目的就是把这些隐含连接显式化，让引擎拓扑完整。
                //        注意这是跨两行的函数调用（第一行只写了一半）


                it.value().components, it.value().wires);
            // 传入该子电路的元件与原始连线（含 ortho 线）
            // 虚拟线要么由 ortho 线端点反推端口，要么由 T 型点与元件端口求交，两者缺一不可


            for (const auto& w : vw) allWires.append(w);
            // 把虚拟线逐条追加到 allWires（现在是"原始线 + 虚拟线"的合并列表）
            // 这里用按值复制的 for 循环变量 w，append 到**另一个**容器 allWires 不会造成迭代器失效，也不会改变 vw


            m_eventEngine->build(it.value().components, allWires, subComps, subWires);
            // 用"该子电路元件 + 合并后的线 + 全部子电路定义"构建引擎
            // 第二个参数必须是合并后的线列表，否则 T 型接入等连接会丢失；
            //        第三/四个参数给的是"其它子电路"的定义，让该子电路里嵌套的 sub 实例能被正确展开

            m_engineDirty = false;
            // 标记拓扑已是最新
            // 仿真定时器/单步等入口看到 m_engineDirty==false 才会直接推进；如果忘记清，会每次 tick 都重复 rebuild，性能灾难


            m_needFullApply = true;
            // 标记下一次 apply 必须全量刷新
            // 引擎是新构建的，内部没有"脏节点"记录（所有节点都算"刚生成"），
            //        如果走增量 apply，界面上大部分元件都收不到更新，会显示旧值


            m_ctxSubInstCache.clear();
            // 清空"上下文 → 子电路实例 ID 列表"缓存
            // 该缓存记录的是 m_root.components 里每个 subId 对应哪些实例 ID；每次 rebuild 都可能伴随结构变化（这个函数本就是在结构变化后调用的），
            //        缓存一旦过期，engineQidsFor 会返回不存在或漏掉的 qualified id，导致 setInputFromString 等操作打向错误的节点


            return;
            // 【提前返回】子电路作根的分支处理完毕，直接结束函数
            // 避免继续执行下面"用 m_root 构建"的代码——那样会把引擎覆盖成根电路拓扑，前面的工作全部白费


        }
        // 编辑子电路分支结束
        // ——


    }
    // editingSubAsRoot 外层分支结束
    // 两种可能：①上面的 return 已结束；②没找到当前子电路，继续往下走根电路分支

    QVariantList allWires = m_root.wires;
    // 以根上下文的原始连线为起点
    // 隐式共享，写时才分裂；与上面子电路分支同样的思路


    QVariantList vw = WireNetworkSolver::generateVirtualWires(
        // 为根上下文生成虚拟连接
        // root 层的 ortho 线与 T 型接入同样需要显式化，否则以根为顶层仿真时连接不完整（跨两行的调用第一行）


        m_root.components, m_root.wires);
    // 传入根的元件与原始连线
    // ——


    for (const auto& w : vw) allWires.append(w);
    // 合并虚拟线
    // 同上，虚拟线放在原始线之后不影响引擎构建（引擎按 id 建索引，与顺序无关）


    m_eventEngine->build(m_root.components, allWires, subComps, subWires);
    // 以根上下文为顶层构建引擎
    // 这是最常见路径（用户在主电路里仿真）；subComps/subWires 提供全部子电路定义，供嵌套实例展开

    m_engineDirty = false;
    // 标记拓扑最新
    // 同子电路分支，避免重复 rebuild


    m_needFullApply = true;
    // 下次 apply 全量
    // 同子电路分支：新引擎没有脏节点记录

    m_ctxSubInstCache.clear();
    // 清空实例缓存
    // 结构可能已变，缓存必须失效（与子电路分支同理）

}
// rebuildEventEngine 结束
// ——

static void updateOneComponent(QVariantMap& c,
                               // 定义文件级静态函数 updateOneComponent：把事件引擎中某**一个**元件的输出写回它的数据 map
                               // applyEngineOutputsToContexts 需要对大量元件做同样的事，抽成 static 函数可以避免反复的成员访问、也天然限制在本 TU 内；
                               //        static 表示内部链接，不导出符号，减少符号冲突。参数 c 是**引用**——这是回调式原地修改的关键（跨三行的声明第一行）

                               EventEngine* eng,
                               // 事件引擎指针，用于查询输出
                               // 调用方保证非空（apply 入口已判空），所以函数内不再判空

                               const QHash<QString, CircuitContext>& subContexts)
// 子电路定义表：当元件是 sub 实例时，需要知道它引用了哪个子电路、里面有哪些 input/output
// sub 实例的输出不是引擎直接给的（它对应一批内部节点），必须靠定义表把"内部节点名"拼出来；const 引用避免拷贝


{
    // 函数体开始
    // ——


    QString localId = c.value("id").toString();
    // 取出该元件在**本地上下文**中的 ID
    // 上下文里的元件 ID 只在本地唯一；引擎里的 qualified id 则可能是 "实例ID/内部ID" 形式，
    //        本函数要用 localId 去拼这个限定 ID，所以先取出来


    QString type = c.value("type").toString();
    // 取出元件类型字符串
    // 不同类型元件的输出字段完全不同：sub 要写 outputPorts + inputPortValues；
    //        input/const 还要写 inputBits；output/led 只写 inputPortValues；其它闸类只写 outputPorts


    if (type == "sub") {
        // 【分支】子电路实例：需要特殊处理
        // sub 实例在引擎里不是单个节点，而是一整块子图；它的"输出"来自子电路内部 output 元件的输出，
        //        它的"输入"则对应内部 input 元件的输入值——两者都必须逐个映射回实例的端口数组
        QString subId = c.value("subId").toString();
        // 取出该实例引用的子电路定义 ID
        // 引擎里的内部节点 qualified id 形如 "localId/内部元件ID"，但必须先确认定义存在，才能知道内部有哪些 input/output


        auto sit = subContexts.find(subId);
        // 在定义表中查找该子电路
        // 用 find 便于判 end()；若定义缺失（例如导入了不完整的文件），直接跳过这个实例而不是崩溃


        if (sit == subContexts.end()) return;
        // 【提前返回】定义不存在就不更新这个实例
        // 防御式编程：宁可显示旧值，也不能因为拼出错误的 qualified id 而取到别的节点的输出


        QVariantList outVals;
        // 准备收集该实例的输出端口值
        // 顺序必须与实例的 outputPorts 顺序一致（即子电路内 output 元件的存储顺序），否则界面上的线会接错位

        for (const auto& scv : sit.value().components) {
            // 遍历子电路定义中的所有元件
            // 只能通过遍历找出 type=="output" 的元件，因为端口定义就是这批元件


            QVariantMap sc = scv.toMap();
            // 把 QVariant 转成 QVariantMap
            // 后面多处读取字段，转换一次比每次 toMap() 便宜


            if (sc.value("type").toString() != "output") continue;
            // 【跳过条件】不是 output 元件的直接跳过
            // 只有 output 元件构成该实例的"输出端口"，其它内部元件的结果不需要暴露到父层


            QString innerQid = localId + "/" + sc.value("id").toString();
            // 拼出内部 output 元件在引擎中的限定 ID："实例本地ID/内部元件ID"
            // 引擎把每个实例展开成独立节点集合，用 "/" 分隔路径是工程约定的命名规则；
            //        这个规则同时被 engineQidsFor、syncSubStateFromParent 使用，三处必须一致


            QStringList outs = eng->getOutputs(innerQid);
            // 向引擎查询该节点的输出端口值，返回 QStringList（每个元素是二进制串，可能含 'X'/'Z'/'E' 特殊态）
            // 用 getOutputs 而不是 getInputs：对 output 元件而言，它的"输入"是外部信号，而这里要的是它自身的输出值


            QString v = outs.isEmpty() ? QString() : outs[0];
            // 取第一个输出端口的值；若引擎没有该节点的记录，则得到空串
            // output 元件只有 1 个输出端口，取 outs[0]；用三目而不是直接 outs[0]，避免空列表越界崩溃


            if (v.isEmpty()) {
                // 【条件】引擎里没有这个节点的值时，构造一个"全 0"的合法值
                // 界面渲染要求每个端口都有确定长度的二进制串；空串会导致 QML 侧解析位串时出错或画出空隙

                int w = sc.value("bitWidth").toInt();
                // 读取该 output 元件的位宽
                // 回退值的长度必须等于位宽，才能与其它端口对齐


                if (w < 1) w = 1;
                // 【钳位】位宽至少为 1
                // 防御：缺字段时 toInt() 返回 0，QString(0,'0') 会得到空串，等于没有回退

                v = QString(w, '0');
                // 构造长度为 w 的全 '0' 字符串
                // QString(int, QChar) 是 Qt 的"重复字符"构造函数，比手工循环更短更快

            }
            // 回退分支结束
            // ——

            outVals.append(v);
            // 把该输出端口的值追加到结果列表
            // 追加顺序 = 遍历子电路元件的顺序，与端口索引一一对应

        }
        // 输出端口遍历结束
        // ——

        if (!outVals.isEmpty()) c["outputPorts"] = outVals;
        // 若有输出端口，写回实例 map 的 "outputPorts" 字段
        // 加 !isEmpty() 判断：没有输出端口的子电路不要凭空写入空数组，
        //        保持"无此字段"与"空数组"的区别，避免 UI 侧误判为"有端口但全是空值"

        QVariantList inVals;
        // 准备收集该实例的输入端口值
        // inputPortValues 用于在实例上显示"每个接线柱当前收到的值"，也用于浏览子电路时把父实例的值同步进去

        for (const auto& scv : sit.value().components) {
            // 再次遍历子电路定义中的元件，这次找 input
            // 为什么不再一次遍历同时收集？因为输入/输出必须分别按各自元件的顺序排列，分开两轮逻辑最清晰、不易错

            QVariantMap sc = scv.toMap();
            // 转成 map
            // 同上一轮

            if (sc.value("type").toString() != "input") continue;
            // 【跳过条件】只要 input 元件
            // 只有 input 元件构成实例的输入端口

            QString innerQid = localId + "/" + sc.value("id").toString();
            // 拼出内部 input 元件的限定 ID
            // 与输出侧完全相同的命名规则

            QStringList outs = eng->getOutputs(innerQid);
            // 查询该 input 节点的输出值
            // 注意：这里查的是 **getOutputs** 而不是 getInputs——因为子电路内部，input 元件是"信号源"，它自身的输出就是父实例接线柱送进来的值；
            //        对父层而言这正是"输入端口上的值"

            QString v = outs.isEmpty() ? QString() : outs[0];
            // 取端口值，空则留空串
            // 与输出侧同构，防止越界

            if (v.isEmpty()) {
                // 【条件】值为空时回退全 0
                // 界面需要确定长度的字符串

                int w = sc.value("bitWidth").toInt();
                // 读取位宽
                // 决定回退串长度

                if (w < 1) w = 1;
                // 【钳位】位宽至少 1
                // 同上

                v = QString(w, '0');
                // 构造全 0 串
                // 同上

            }
            // 输入回退分支结束
            // ——

            inVals.append(v);
            // 追加到输入端口值列表
            // 顺序与 input 元件顺序一致

        }
        // 输入端口遍历结束
        // ——

        if (!inVals.isEmpty()) c["inputPortValues"] = inVals;
        // 写回实例的 "inputPortValues" 字段
        // 同样避免写入空数组；这个字段会被 syncSubStateFromParent 读取，用于浏览子电路时注入输入值

        return;
        // 【提前返回】sub 分支处理完毕，结束函数
        // 下面的普通元件逻辑不适用于 sub（它没有直接的 outputPorts 语义），必须提前 return，否则会被错误改写

    }
    // sub 分支结束
    // ——

    QStringList outs = eng->getOutputs(localId);
    // 对普通元件，直接用自己的本地 ID 向引擎查询输出
    // 在"当前上下文即顶层"的情况下（编辑子电路作为根、或根电路），元件的 qualified id 就是它的本地 ID；
    //        只有内层实例里的节点才需要加前缀（见 engineQidsFor 的说明）

    if (!outs.isEmpty()) {
        // 【条件】引擎确实有这个节点、且有输出端口时才写回
        // 没有输出端口的元件（如纯输入接线柱之外的某些辅助类型）或引擎里不存在的节点，不该被凭空写入 outputPorts

        QVariantList ops;
        // 准备输出端口值列表
        // 引擎返回的是 QStringList，而 UI 数据统一用 QVariantList 承载，需要一次转换

        ops.reserve(outs.size());
        // 预分配容量
        // 避免 append 过程中多次重新分配内存（QList 的容量增长是倍增的，reserve 可以精确一次到位）

        for (const auto& s : outs) ops.append(s);
        // 逐个把 QString 追加进 QVariantList（隐式转换为 QVariant(QString)）
        // 单行循环体省略花括号，代码更紧凑；转换是隐式的，QList<QVariant> 会为 QString 建一个 tag 存共享指针，代价很小

        c["outputPorts"] = ops;
        // 把输出端口值写回元件 map
        // QML 渲染元件输出引脚、连线着色都读这个字段；它是"引擎结果 → 元件数据"的主要通道

        if (type == "input" || type == "const") {
            // 【条件】input（输入开关）与 const（常量）元件还要额外同步"位数组"
            // 这两类元件的界面是"一排可点击的位按钮"，按钮状态读的是 inputBits（QVariantList<bool>），
            //        而 outputPorts 是字符串；两者必须保持一致，否则按钮显示与导线上的值不符

            int bw = c.value("bitWidth").toInt();
            // 读取位宽
            // inputBits 必须是恰好 bw 个布尔值

            if (bw < 1) bw = 1;
            // 【钳位】位宽至少 1
            // 缺字段/非法值时保证位数组非空

            QString s = outs[0];
            // 取该元件的第一个输出串（input/const 只有 1 个输出端口）
            // 把二进制串"拆位"成布尔数组是这个分支的核心；s[0] 在最左侧，按 MSB→LSB 顺序

            if (s.length() != bw) {
                // 【条件】字符串长度与位宽不一致时做规整
                // 引擎可能返回带 'X'/'Z' 的串、或长度与配置不一致的串（例如刚改过 bitWidth 还没重建）；
                //        UI 的位数组必须与当前位宽严格对齐，否则按钮数量会错

                if (s.length() < bw) s = QString(bw - s.length(), '0') + s;
                // 【分支】长度不足时左侧补 '0' 到 bw 位
                // 左侧是高位；补 0 表示"高位无值按 0 处理"，保持数值语义

                else                 s = s.right(bw);
                // 【分支】长度超出时只保留最右侧 bw 位
                // right(bw) 即保留低位、截掉高位，与整数截断语义一致（等价于按 2^bw 取模）

            }
            // 规整分支结束
            // 此时 s 的长度一定等于 bw

            QVariantList bits;
            // 准备位数组
            // 元素为 QVariant(bool)，QML 可当作 bool 列表直接用

            bits.reserve(bw);
            // 预分配 bw 个元素容量
            // 避免逐次扩容

            for (int i = 0; i < bw; ++i) bits.append(s[i] == '1');
            // 逐位比较：字符为 '1' 记 true，其它（'0'、'X'、'Z' 等）记 false
            // 'X'（未知）与 'Z'（高阻）在"位按钮"上只能显示为未点亮，故统一映射为 false；
            //        若需要区分三态，应改读 outputOverride（见 Circuit_subcircuit.cpp:264 的处理方式）

            c["inputBits"] = bits;
            // 写回位数组
            // 界面上的位按钮据此刷新

        }
        // input/const 附加分支结束
        // ——

    }
    // 普通元件输出写回分支结束
    // ——

    if (type == "output" || type == "led") {
        // 【条件】output（输出探针）与 led（指示灯）还要同步"输入端口值"
        // 这两类元件的语义是"显示收到的信号"：它们只有输入、没有输出（或输出无意义），
        //        界面显示的是接线柱上的值，因此要写 inputPortValues 而不是 inputBits

        if (!outs.isEmpty()) {
            // 【条件】引擎有值才写
            // 避免把空值写入，覆盖原有显示

            QVariantList ips;
            // 准备输入端口值列表
            // 与上面的 ops 同理，需要 QStringList → QVariantList 转换

            ips.reserve(outs.size());
            // 预分配
            // 避免扩容

            for (const auto& s : outs) ips.append(s);
            // 逐个追加
            // ——（对 output/led 而言，引擎 getOutputs 返回的其实是"它观测到的输入值"，因为这类元件在引擎里被建模为直通节点）

            c["inputPortValues"] = ips;
            // 写回输入端口值
            // 界面用它显示数值小标签/指示灯颜色

        }
        // 条件结束
        // ——

    }
    // output/led 分支结束
    // 函数到此为止，调用方持有的是被修改过的 c（引用传入）

}
// updateOneComponent 结束
// ——

void Circuit::applyEngineOutputsToContexts() {
    // 定义 applyEngineOutputsToContexts：把事件引擎算出的结果"回灌"到 Circuit 的上下文数据结构里
    // 这是引擎世界与 UI 世界之间唯一的同步点。它有两种模式：
    //        ① 全量模式（m_needFullApply==true）：遍历目标上下文的**所有**元件更新一遍，用于刚 rebuild 之后；
    //        ② 增量模式：只更新"引擎标记为脏"的元件，用于仿真推进的每一帧，避免大电路每帧全量 O(N) 的遍历与信号风暴

    if (!m_eventEngine) return;
    // 【提前返回】引擎不存在则无事可做
    // iter 模式下引擎指针本来就为空，调用方偶尔也会无条件调用本函数，必须判空

    if (m_engineDirty) {
        // 【条件】拓扑已过期时，先重建
        // apply 的前提是引擎拓扑与上下文结构一致；结构变了但没重建，引擎里缺节点，取输出会得到空值，
        //        进而写出全 0，界面显示错误。这里做一次"懒重建"，把一致性责任收在本函数内

        rebuildEventEngine();
        // 重建引擎拓扑（会同时把 m_engineDirty 置 false、m_needFullApply 置 true）
        // 重建后自动进入全量分支，保证所有元件都拿到新引擎的结果

    }
    // 重建分支结束
    // ——

    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;
    // 判断"当前是在编辑某个子电路（而非浏览、而非根）"
    // 决定把引擎结果写回**哪个**上下文：编辑子电路时，引擎的顶层就是该子电路（见 rebuildEventEngine），
    //        结果自然要写回它；否则写回根上下文

    Context* targetPtr = editingSubAsRoot ? ctxById(m_currentCtxId) : &m_root;
    // 取得目标上下文指针
    // 用指针而不是引用，是因为 ctxById 在 ID 不存在时返回 nullptr，需要先判断；
    //        这里用三目保持一行，逻辑上"编辑子电路 → 查子电路；否则 → 根"

    if (!targetPtr) targetPtr = &m_root;
    // 兜底：指针为空时退回根上下文
    // 防御 m_currentCtxId 悬空；此时宁可把结果写到根上（虽然语义略有偏差），也不能解引用空指针

    Context& targetCtx = *targetPtr;
    // 解引用得到目标上下文的引用，后续直接修改其 components
    // 引用写法让后面代码无需重复写 (*targetPtr)；注意它绑定的是哈希表/成员里的对象，生命周期由 Circuit 保证

    if (m_needFullApply) {
        // 【分支】全量刷新
        // rebuild 之后必备：新引擎没有脏节点记录，且上下文里所有元件的输出都可能变了

        m_needFullApply = false;
        // 先消费掉"需要全量"的标记
        // 放在循环之前而不是之后：即便后面遍历中抛出异常（理论上不会），也不会陷入"永远全量"的状态；
        //        语义上这是"本次调用已经承担了全量职责"

        for (int i = 0; i < targetCtx.components.size(); ++i) {
            // 按索引遍历目标上下文的所有元件
            // 用索引而不是范围 for：后面要把修改后的 map 写回 components[i]，索引最直接；
            //        且 components 是 QVariantList（QList），随机访问 O(1)

            QVariantMap c = targetCtx.components[i].toMap();
            // 取出第 i 个元件并转成可修改的 QVariantMap
            // QVariant 里存的是 map；toMap() 返回**值**（隐式共享），在这个副本上改，改完再写回，避免直接改共享数据

            updateOneComponent(c, m_eventEngine, m_subContexts);
            // 让静态函数把该元件的输出/输入值写进 c
            // 把"按类型取字段"的细节集中到一处，本循环只负责遍历与写回

            targetCtx.components[i] = c;
            // 把修改后的 map 写回列表
            // QList 的元素是 QVariant，赋值会触发"隐式共享分裂"：如果别处还持有旧 map 的引用，这里会真正拷贝一份，
            //        从而保证其它持有者（例如撤销快照）看到的是旧数据，不被本次修改污染

        }
        // 全量遍历结束
        // 此时 targetCtx 中所有元件的显示字段都已同步

        m_eventEngine->clearDirtyQids();
        // 清空引擎内部的"脏节点"集合
        // 全量刷新已经覆盖了所有节点，脏集合里积累的记录已无意义；
        //        若不清空，下一次增量 apply 会把同一批节点再更新一遍（冗余但无害），更容易造成"以为增量在生效"的误解

        return;
        // 【提前返回】全量模式结束
        // 不需要再走增量逻辑；同时跳过末尾的 refreshCurrentSubContext（全量已经覆盖当前上下文）

    }
    // 全量分支结束
    // ——

    QVector<QString> dirty = m_eventEngine->takeDirtyQids();
    // 从引擎"取走"脏节点限定的 ID 列表
    // take 而非 get：取走即清空引擎内部集合，保证每个脏标记只被处理一次；
    //        用 QVector 因为它是纯顺序容器、连续内存，遍历更快，且我们只做一次性消费

    if (dirty.isEmpty()) return;
    // 【提前返回】没有脏节点 → 这一帧没有任何变化，直接结束
    // 仿真中大多数帧其实没有值变化（信号稳定后），提前返回避免了建索引、遍历、写回等所有开销，
    //        这是让大电路在 60fps 下依然流畅的关键

    QHash<QString, int> idToIdx;
    // 建立"元件 ID → 在 targetCtx.components 中的索引"的映射
    // 脏列表里是字符串 ID，而写回需要索引；对每个脏 ID 都线性扫描一次列表会是 O(D×N)，
    //        预先建一张哈希表把查找降到 O(1)

    idToIdx.reserve(targetCtx.components.size() * 2);
    // 预分配哈希桶：容量取元件数的 2 倍
    // QHash 默认负载因子约 0.5~0.6，预留 2 倍容量可以避免插入过程中反复 rehash；
    //        多花一点内存换取稳定性能，对"每帧都会执行"的路径值得

    for (int i = 0; i < targetCtx.components.size(); ++i)
        // 遍历所有元件，建立映射
        // 必须覆盖全部元件（而不是只覆盖脏的），因为脏 ID 是"顶层 ID"（可能带 '/' 前缀），
        //        要能在表里查到"它属于本上下文的哪个元件"（下一行的语句体）

        idToIdx.insert(targetCtx.components[i].toMap().value("id").toString(), i);
    // 插入 "元件ID → 索引"
    // ID 在上下文内唯一，不会重复覆盖；toMap() 会有一次隐式共享拷贝，但只读其 id 字段，代价可控

    QSet<QString> instanceIdsToUpdate;
    // 收集"需要更新"的元件 ID 集合
    // 用 QSet 而不是 QList 是因为：同一个实例下可能有多个内部节点脏了（例如子电路里有 5 个门变了），
    //        但对父层来说只需把这个实例更新一次；集合天然去重

    for (const QString& qid : dirty) {
        // 遍历每个脏的限定 ID
        // qid 可能是 "G1"（顶层元件），也可能是 "U2/G3"（实例 U2 内部的门 G3）

        int slash = qid.indexOf('/');
        // 查找限定 ID 中第一个 '/' 的位置
        // 命名规则是 "实例ID/内部ID/..."，第一个 '/' 之前就是顶层（本上下文）元件 ID；
        //        返回 -1 表示没有 '/'，即本身就是顶层元件

        QString topId = (slash < 0) ? qid : qid.left(slash);
        // 截取顶层元件 ID
        // 无论嵌套多深，父层只需要知道"哪个最外层实例变了"，内部的细节由该实例的整块刷新处理

        if (idToIdx.contains(topId))
            // 【条件】该顶层 ID 确实是本上下文里的元件
            // 脏列表可能包含不属于当前 targetCtx 的节点（例如引擎是以根为顶层构建、但脏记录里混入子电路定义中的节点），
            //        不在表里的直接忽略，避免更新到不存在的元件

        instanceIdsToUpdate.insert(topId);
        // 加入待更新集合
        // 同一实例的多个脏节点在此去重

    }
    // 脏 ID 归一化结束
    // ——

    for (const QString& id : instanceIdsToUpdate) {
        // 遍历去重后的待更新元件 ID
        // QSet 遍历顺序不确定，但这批更新的顺序无关紧要（每个元件独立更新）

        int i = idToIdx.value(id, -1);
        // 查索引，查不到给 -1
        // 用 value(key, defaultValue) 而不是 operator[]：后者在 key 不存在时插入默认值，会污染哈希表并返回错误索引

        if (i < 0) continue;
        // 【跳过条件】索引非法（理论上不会，因为集合只从表里来）时跳过
        // 多重防御，保证不会越界访问 components[i]

        QVariantMap c = targetCtx.components[i].toMap();
        // 取出该元件 map
        // 与全量分支相同的"拷贝 → 修改 → 写回"模式

        updateOneComponent(c, m_eventEngine, m_subContexts);
        // 按类型把引擎结果写入 c
        // 注意对 sub 实例，updateOneComponent 会重新读取它**所有**内部 output/input 的输出，相当于"整实例刷新"，
        //        这正是上面把脏节点归一化到顶层 ID 的原因

        targetCtx.components[i] = c;
        // 写回上下文
        // 触发隐式共享分裂，保证其它持有者不受影响

    }
    // 增量写回结束
    // ——

    if (m_viewOnly) refreshCurrentSubContext();
    // 【条件】浏览模式下，额外刷新正在浏览的子电路上下文
    // 浏览模式下 targetCtx 是根上下文（因为 editingSubAsRoot 为 false），但用户眼睛看着的是子电路内部：
    //        子电路的输入值可能刚随父层变化，必须把它自己的逻辑再求值一遍，界面才对得上；
    //        编辑模式下不需要——targetCtx 本身就是被刷新的那个上下文

}
// applyEngineOutputsToContexts 结束
// ——

QStringList Circuit::engineQidsFor(const QString& ctxId, const QString& localCompId) const {
    // 定义 engineQidsFor：把"某个上下文里的本地元件 ID"翻译成"事件引擎里的限定 ID 列表"
    // 同一个子电路定义可能被实例化多次：定义里的元件 G1 在引擎里会变成 "U1/G1"、"U2/G1"……
    //        当用户直接编辑子电路定义（而该定义正被多个实例引用）时，一次修改必须作用到**所有**实例对应的引擎节点上，
    //        所以返回的是列表而不是单个字符串。const 函数却能改 m_ctxSubInstCache —— 靠把该成员声明为 mutable（见 Circuit.h 第 436 行）

    if (!m_currentCtxId.isEmpty() && !m_viewOnly)
        // 【条件 1】当前是在"编辑子电路作为根"的模式
        // 此时引擎的顶层就是该子电路，元件的限定 ID 就是它自己的本地 ID，没有任何实例前缀；
        //        直接返回单个 ID 即可，无需查缓存、无需拼前缀——这是最快的路径

    return QStringList{ localCompId };
    // 用初始化列表构造单元素 QStringList 返回
    // C++11 列表初始化写法，比 QStringList() << localCompId 更短；返回是隐式共享的，开销很小

    if (ctxId.isEmpty()) return QStringList{ localCompId };
    // 【条件 2】ctxId 为空表示"根上下文里的元件"
    // 根上下文在引擎里就是顶层，不需要前缀；这也覆盖了"在根上下文中编辑普通元件"的常见情况

    auto it = m_ctxSubInstCache.constFind(ctxId);
    // 在缓存中查找"该子电路 ID → 引用它的实例 ID 列表"
    // 这个查询会被 setInputFromString / tick / toggleClock 等高频调用，每次都扫一遍 m_root.components 代价不小；
    //        用 constFind 因为这是 const 函数（返回 const_iterator）

    QStringList instIds;
    // 存放实例 ID 列表（先取缓存，缓存空则现算）
    // 后续要统一遍历 instIds 拼限定 ID，先把数据来源二选一地准备好，避免两段重复代码

    if (it != m_ctxSubInstCache.constEnd()) {
        // 【条件】缓存命中
        // 命中则直接用，省去 O(N) 扫描

        instIds = it.value();
        // 从缓存取出实例列表（隐式共享拷贝）
        // ——（注意：这里只查了根上下文 m_root，不查嵌套在子电路里的实例，因为引擎的顶层就是根，
        //        内层由各实例内部的节点名自行表达）

    } else {
        // 【条件】缓存未命中
        // 需要现算：扫描 m_root 里所有 type=="sub" 且 subId==ctxId 的元件

        for (const auto& cv : m_root.components) {
            // 遍历根上下文的全部元件
            // 只有根层直接引用的实例才会产生 "实例ID/..." 形式的顶层限定 ID

            QVariantMap c = cv.toMap();
            // 转成 map 便于读字段
            // ——

            if (c.value("type").toString() != "sub") continue;
            // 【跳过条件】非 sub 元件跳过
            // 只有子电路实例才可能引用某个子电路定义

            if (c.value("subId").toString() == ctxId)
                // 【条件】该实例引用的正是目标子电路
                // 同一个子电路可能有多个实例，全部收集

            instIds.append(c.value("id").toString());
            // 记录实例 ID
            // 后面要拼成 "instId/localCompId"

        }
        // 扫描结束
        // ——

        m_ctxSubInstCache.insert(ctxId, instIds);
        // 把结果写入缓存（mutable 成员允许在 const 函数中修改）
        // 缓存失效点：rebuildEventEngine、clear、fromJson、importFromFile、removeSubContextsByPrefix 等都会 clear 它；
        //        这是"用一点内存换高频查询速度"的典型折中

    }
    // 缓存分支结束
    // ——

    QStringList result;
    // 准备最终结果列表
    // 元素个数 = 实例个数，每个是 "实例ID/本地元件ID"

    result.reserve(instIds.size());
    // 预分配
    // QStringList 即 QList<QString>，reserve 避免扩容

    for (const QString& instId : instIds)
        // 遍历每个实例
        // 每个实例在引擎里都有独立的节点集合，都必须被更新

        result.append(instId + "/" + localCompId);
    // 拼出限定 ID
    //  '/' 分隔符与 rebuildEventEngine / updateOneComponent 中的命名规则严格一致；
    //        若实例本身又是嵌套的（即 instId 所在层不是根），这里不会处理——因为该方法只面向根层实例，嵌套情况由各自的 ctxId 表达

    return result;
    // 返回全部限定 ID
    // 调用方（如 setInputFromString）会逐个 triggerInputString，让每个实例里的同名元件都收到新值

}
// engineQidsFor 结束
// ——

// ★ 修复：起始 advance 至少 10000（原值 ~200 会导致大电路需要一路翻倍，
//   每次翻倍都受 budget=50万 上限约束，最终跑不完）
// ★ 修复段注释：说明下面 runEventEngineFully 的起始时间步为什么要"至少 10000"
// 历史缺陷：原先调用方传 (wd+gd)*64+200 ≈ 200 起步。大电路里一个信号穿过上百级门所需的仿真时间远超 200，
//        run 会在时间步用尽后返回"未空闲"，外层把 a 翻倍重试；但每次 run 最多只处理 budget=500000 个事件，
//        在"时间步过小、事件过多"时反复翻倍仍然跑不完（或触发上限而放弃），表现为仿真停在中途、界面显示半成品结果。
//        修复方式：给一个远大于普通电路所需的起始步长（10000），让第一次 run 基本就能一次跑完；
//        同时保留翻倍机制作为兜底，用于极端规模的电路

void Circuit::runEventEngineFully(quint64 advance, quint64 budget) {
    // 定义 runEventEngineFully：把事件引擎"跑到空闲"为止，内部自动重试并放大时间步
    // EventEngine::run(advance, budget) 的语义是"在 advance 个仿真时间单位内、最多处理 budget 个事件"，
    //        两者任一耗尽就会返回 false。大电路往往一次 run 收敛不了，所以需要外层循环：
    //        每次失败就把 advance 翻倍（给信号更多传播时间），直到空闲（收敛）或重试用尽

    if (!m_eventEngine) return;
    // 【提前返回】引擎不存在则直接返回
    // iter 模式或构造期 m_eventEngine 为空

    if (budget < 1) budget = 200000;
    // 【兜底】调用方传 0 或负数预算时，用 200000 顶上
    // budget 是 quint64，传负值会回绕成天文数字；这里把"非法/未指定"统一视为一个合理默认值，
    //        防止一次 run 试图处理近乎无限的事件而卡死界面

    quint64 a = qMax<quint64>(advance, 10000ULL);
    // 计算起始时间步长：取传入值与 10000 的最大者
    // ★ 就是上面修复注释所说的核心改动：避免从 ~200 这种过小的步长开始，减少"翻倍→仍不够→再翻倍"的无效轮次；
    //        显式写 qMax<quint64> 是为了让两个参数在同一无符号类型下比较，避免模板推导歧义

    for (int round = 0; round < 200; ++round) {
        // 最多重试 200 轮
        // 轮次上限是"安全阀"：如果电路存在振荡（例如无稳态的环形振荡器）或引擎有 bug，事件队列永远不空闲，
        //        没有上限就会死循环卡死 UI 线程；200 轮 × 每次翻倍足以覆盖任何合理电路

        if (m_eventEngine->isIdle()) break;
        // 【循环条件】进入每一轮前先检查是否已空闲，空闲就提前跳出
        // 上一轮 run 之后可能已经收敛，再 run 一次不仅浪费，还可能把仿真时间推进到不必要的远处

        bool ok = m_eventEngine->run(a, budget);
        // 在 a 个时间单位内、最多处理 budget 个事件，并记录是否"完全跑完"
        // ok==true 表示事件队列在该时间窗口内被清空（收敛）；false 表示窗口不够或事件数超预算，
        //        需要下一轮用更大的 a 重试

        if (m_eventEngine->isIdle()) break;
        // run 之后再检查一次空闲，空闲立即结束
        // run 返回 false 也可能是因为预算耗尽但队列恰好清空，此时无需继续；这里以引擎真实状态为准而非返回值

        if (ok) {
            // 【分支】本次 run 成功（窗口内跑完，但仍有后续事件被排在窗口外）
            // 说明时间方向还有待处理的事件（例如延迟传播中的信号），需要更长的窗口把它们推进完

            if (a < 100000000ULL) a *= 2;
            // 【分支】翻倍时间步，但不超过 1e8
            // 指数放大能快速跳出"窗口不够"的状态；设 1e8 上限是防止 a 溢出或让仿真时间跳到荒谬的数值

            else a = 100000000ULL;
            // 【分支】达到上限后固定为 1e8
            // 继续翻倍会溢出 quint64 的时间计算；固定在 1e8 已远超任何电路的传播深度

        }
        // ok 分支结束
        // ——

    }
    // 重试循环结束
    // 退出条件：引擎空闲（正常收敛）、或跑满 200 轮（放弃，保持当前中间结果）

}
// runEventEngineFully 结束
// ——

// ============================================================================
// ④ 文档生命周期：清空、错误检查、序列化、文件读写
// ============================================================================

void Circuit::clear() {
    // 定义 clear：把整个电路重置为"新建文档"的初始状态
    // "新建"菜单要用它。难点在于：Circuit 有十几处相互关联的状态（根、子电路、名称表、计数器、时钟状态、剪贴板、
    //        引擎、缓存、错误列表……），漏掉任何一处都会造成"新建后还残留旧数据"的诡异 bug，所以这里是逐字段穷举重置

    if (m_viewOnly) return;
    // 【提前返回】只读浏览模式下禁止清空
    // 浏览模式是"看别人的实例内部"，此时点"新建"应当无效，否则会破坏正被浏览的电路

    pushUndo();
    // 清空之前先压一次撤销快照
    // "新建"是破坏性操作；有了快照，用户误点后可以用撤销找回原电路。注意这里用的是 pushUndo()（当前上下文范围），
    //        对根上下文清空而言足以恢复 m_root，但不会恢复子电路——这是一处已知的取舍，见下面 m_subContexts.clear() 的说明

    m_root = Context();
    // 用一个默认构造的 Context 覆盖根上下文，清空所有元件与连线
    // 赋值默认构造对象比手工 clear() 每个成员更彻底，也不会漏字段；Context 是值语义的轻量结构

    m_subContexts.clear();
    // 清空所有子电路定义
    // 新建文档不该残留旧子电路。注意：撤销快照（pushUndo 只存了当前上下文）并不包含它们，撤销后子电路不会回来

    m_subNames.clear();
    // 清空子电路名称表
    // 名称表与上下文必须同步清空，否则会留下"有名字没有内容"的幽灵条目

    m_currentCtxId = "";
    // 把当前上下文切回根（空串约定）
    // 若不清，m_currentCtxId 会指向已被删除的子电路，后续 ctxById 返回 nullptr，出现各种空指针防御分支

    m_viewOnly = false;
    // 退出只读浏览标志
    // 新建后必然是编辑态

    m_subCounter = 0;
    // 子电路编号计数器归零
    // 让新建的文档从 S1 开始编号，与全新程序一致；若不清零，下一个子电路可能叫 S37，令人困惑

    m_importCounter = 0;
    // 导入前缀计数器归零
    // 同理，导入编号从 IMP1_ 重新开始

    m_errors.clear();
    // 清空缓存的错误列表
    // 旧电路的错误与新电路无关；signals 区后面会发 errorsChanged 让 UI 同步

    m_clockStates.clear();
    // 清空每个时钟元件的状态表（连"所有时钟 ID 都消失"这件事也一起清）
    // 时钟状态是"以元件 ID 为 key"的，旧 ID 在新文档里不存在，不清会留下无主条目，让 tick() 翻转不存在的时钟

    m_clockTickCount = 0;
    // 时钟跳变计数归零
    // 这是显示/调试用的累计值，新文档应从 0 开始

    m_clipboardCache.clear();
    // 清空内部剪贴板
    // 剪贴板里的元件引用了旧上下文的坐标系与子电路 ID，直接粘到新文档会带入陈旧引用（经典 bug），故一并清空

    m_needFullApply = true;
    // 标记下一次 apply 需要全量
    // 上下文已换，引擎里还残留着旧节点；下次 apply 必须整体覆盖而不是增量

    m_importedFiles.clear();
    // 清空"已导入文件"记录
    // 导入的子电路刚被 m_subContexts.clear() 删掉了，记录若还在，UI 会显示一批已不存在的导入项，
    //        再点"移除"还会按前缀去删空集合

    if (m_clockRunning) stopClock();
    // 【条件】若时钟在跑就停掉
    // 时钟 tick 会翻转 clock 元件并调用 evaluateAll；在数据正被清空的过程中继续 tick 会读到半新半旧的状态

    if (m_simulationRunning) pauseSimulation();
    // 【条件】若仿真在跑就暂停
    // 同理：仿真定时器会在重建后的空电路上推进，浪费 CPU 且可能写回无意义数据

    if (m_eventEngine) m_eventEngine->clear();
    // 清空事件引擎内部的事件队列与节点数据
    // 引擎本身（对象）被复用，只清内容；这比 delete + new 便宜，也避免指针悬空

    m_engineDirty = true;
    // 标记引擎拓扑已失效
    // 清空后引擎里没有节点，但上下文是"空电路"——两者虽都空，仍需下一次重建来建立正确的空拓扑标记

    m_ctxSubInstCache.clear();
    // 清空实例缓存
    // 所有子电路都没了，缓存必然过期

    emit contextChanged();
    // 通知上下文已切换（回到根）
    // Q_PROPERTY contextId/contextName/isRootContext/isViewOnly 的 NOTIFY 是这个信号，UI 要刷新面包屑

    emit subcircuitsChanged();
    // 通知子电路列表变化（现在为空）
    // 侧边栏清空

    emit editContextsChanged();
    // 通知可编辑上下文列表变化（只剩"主电路"）
    // 下拉框重建

    emit errorsChanged();
    // 通知错误列表变化
    // 错误面板清空

    emit clockChanged();
    // 通知时钟状态变化（已停、计数归零）
    // 播放按钮回到停止态

    emit changed();
    // 通知通用数据变化
    // components/wires 的 NOTIFY 是 changed，画布重绘成空白

}
// clear 结束
// ——

void Circuit::refreshErrors() {
    // 定义 refreshErrors：扫描所有上下文的连线，找出"源端口位宽 ≠ 目标端口位宽"的错误，写入 m_errors
    // 位宽不匹配是电路编辑器最常见的用户错误（例如 4 位信号接到 1 位输入），
    //        这类错误不会让仿真崩溃（引擎会截断/补零），但结果一定是错的，所以必须主动检查并提示

    m_errors.clear();
    // 先清空旧错误
    // 本函数是"全量重算"而不是"增量追加"，不清空会导致同一错误越积越多

    auto checkCtx = [&](const Context& ctx, const QString& label) {
        // 定义局部 lambda checkCtx：对**一个**上下文做错误扫描，label 是该上下文的显示名（用于错误消息前缀）
        // 根上下文与每个子电路的检查逻辑完全相同，只差"叫什么名字"，用 lambda 避免复制一遍循环；[&] 按引用捕获 m_errors 等

        QHash<QString, int> idToIdx;
        // 建立"元件 ID → 索引"映射
        // 连线里存的是元件 ID，要取元件 map 计算端口位宽就得转成索引；建表后 O(1) 查找，整体从 O(W×N) 降到 O(N+W)

        idToIdx.reserve(ctx.components.size() * 2);
        // 预分配 2 倍容量
        // 同 applyEngineOutputsToContexts，避免插入时 rehash

        for (int i = 0; i < ctx.components.size(); ++i)
            // 遍历元件建表
            // ——（循环体在下一行）

            idToIdx.insert(ctx.components[i].toMap().value("id").toString(), i);
        // 插入映射
        // ID 唯一，后插覆盖前插不会发生

        for (auto& wv : ctx.wires) {
            // 遍历该上下文的所有连线
            // 位宽不匹配只能从"一条线两端"看出来，所以检查单位是连线

            QVariantMap w = wv.toMap();
            // 转成 map 便于读字段
            // ——（注意 wv 是 QVariant 引用，这里只读）

            QString wt = w.value("wireType").toString();
            // 读取连线类型
            // 不同类型线的检查方式不同：ortho/virtual 不检查，bezier（含缺省）才检查

            if (wt == "ortho" || wt == "virtual") continue;
            // 【跳过条件】跳过正交线与虚拟线
            // ★ 重要设计：ortho 线只记录 path 顶点坐标，**不保证**保存了 fromComp/toComp/端口索引（从线中间引出的 T 型线甚至没有起点元件），
            //        virtual 线是求解器推导出来的派生数据。对它们做位宽检查会得到大量误报，
            //        所以位宽校验只针对"显式记录了两端端口"的 bezier 线

            QString fromId = w.value("fromComp").toString();
            // 取源元件 ID
            // 用来查源元件 map，从而计算源端口位宽

            QString toId = w.value("toComp").toString();
            // 取目标元件 ID
            // 同上

            int fromPort = w.value("fromPort").toInt();
            // 取源端口索引
            // 同一元件可能有一个输出端口，也可能有多个（如 splitter），必须精确到端口

            int toPort = w.value("toPort").toInt();
            // 取目标端口索引
            // 同上

            int fi = idToIdx.value(fromId, -1);
            // 查源元件索引，查不到给 -1
            // value(key, -1) 避免 operator[] 插入

            int ti = idToIdx.value(toId, -1);
            // 查目标元件索引
            // 同上

            if (fi < 0 || ti < 0) continue;
            // 【跳过条件】任一端指向不存在的元件就跳过
            // 悬空连线是另一种错误（本函数不负责报告），若继续执行会用 (-1) 索引 components 而越界崩溃

            int fbw = ComponentLogic::outputBitWidth(ctx.components[fi].toMap(), fromPort);
            // 计算源元件第 fromPort 个输出端口的位宽
            // 位宽不是 map 里的固定字段：对一个 8 位 splitter，不同输出端口位宽不同；对 adder 之类还有推算规则，
            //        必须交给 ComponentLogic 按类型计算

            int tbw = ComponentLogic::inputBitWidth(ctx.components[ti].toMap(), toPort);
            // 计算目标元件第 toPort 个输入端口的位宽
            // 同上，用 inputBitWidth 而不是 outputBitWidth——输入端口有自己的位宽规则

            if (fbw != tbw) {
                // 【条件】两端位宽不等 → 记录错误
                // 相等是正常情况；不等时引擎会自动截断或补零，结果是"能跑但错"，所以必须提示用户

                QVariantMap err;
                // 创建一条错误记录
                // 错误列表是 QVariantList，元素为 QVariantMap，UI 按 key 渲染

                err["type"] = "width_mismatch";
                // 错误类型标记
                // UI 可以据此决定图标/颜色，也便于将来扩展更多错误类型（如悬空端口、短路输出）

                err["msg"] = QString("%1：%2 位宽不匹配（源 %3 位，目标 %4 位）")
                                 // 生成人类可读的中文错误消息，占位符依次为：上下文名、连线 ID、源位宽、目标位宽
                                 // 用户需要知道"在哪个电路、哪条线、差了多少位"才能去修；用 QString::arg 链式替换占位符（注意这行只写了一半）

                                 .arg(label, w.value("id").toString()).arg(fbw).arg(tbw);
                // 依次填充 4 个占位符：label 与连线 ID 一次 arg 传入两个（对应 %1 %2），然后 %3=fbw、%4=tbw
                // QString::arg 有 (QString,QString) 重载，一次替换两个占位符，省一次调用；
                //        注意 %3/%4 是 int 重载，若位宽为负会被格式化正确显示

                m_errors.append(err);
                // 把错误追加进缓存列表
                // m_errors 就是 errors 属性的数据源（errors() getter 直接返回它）

            }
            // 位宽不匹配分支结束
            // ——

        }
        // 连线遍历结束
        // ——

    };
    // lambda 定义结束（注意末尾分号）
    // auto 变量初始化为 lambda 后必须加分号

    checkCtx(m_root, "主电路");
    // 检查根上下文，标签为"主电路"
    // 与 editContexts() 里根条目的显示名保持一致，用户在错误面板看到的"主电路"就是下拉框里的那一项

    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 遍历所有子电路逐一检查
        // 错误可能存在于任何一层上下文，只检查根会漏报

        checkCtx(it.value(), m_subNames.value(it.key(), it.key()));
    // 用子电路名作为标签（取不到名字时退回用 ID）
    // m_subNames.value(key, default) 的第二个参数是默认值：名称为空/缺失时，标签显示成 "S7" 也比空白好

    emit errorsChanged();
    // 通知 UI 错误列表已更新
    // errors 属性的 NOTIFY 信号；放在最后，保证 UI 拿到的列表是完整的

}
// refreshErrors 结束
// ——

QVariantMap Circuit::toJson() const {
    // 定义 toJson：把整个电路（含子电路、时钟状态、引擎参数）导出为一个 QVariantMap
    // 这是"保存"与"导入"共用的序列化入口；Circuit 只负责收集状态，字段格式全部交给 JsonCodec::encode，
    //        保证文件格式在任何地方都只有一份定义

    QHash<QString, quint64> gateDelays;
    // 准备"门类型 → 独立延迟"的覆盖表
    // 用户可以给某类门单独设置延迟（setGateTypeDelay），这些覆盖必须随文件保存，否则重新打开后延迟配置丢失；
    //        元素类型是 quint64，与 EventEngine::gateTypeDelays() 的返回类型一致

    if (m_eventEngine) gateDelays = m_eventEngine->gateTypeDelays();
    // 从事件引擎读出延迟覆盖表（引擎为空则保持空表）
    // engine 可能不存在（构造中/iter 模式），判空后赋值；赋值是隐式共享拷贝，开销很小

    return JsonCodec::encode(m_root, m_subContexts, m_subNames,
                             // 调用 JsonCodec::encode 把全部状态编码成 map，参数依次为：根上下文、子电路表、名称表……
                             // 把"电路数据"与"引擎参数"一起保存：文件必须自包含，打开后能完整还原当时的状态；跨多行的调用（第一行）

                             m_subCounter, m_clockFrequency, m_clockTickCount,
                             // 继续传参：子电路计数器、时钟频率、时钟跳变计数
                             // 计数器决定下一个子电路 ID（不保存会导致新子电路与旧 ID 冲突）；时钟参数属于电路行为的一部分

                             m_engineType, globalGateDelay(), wireDelay(),
                             // 继续传参：引擎类型、全局门延迟、线延迟
                             // globalGateDelay()/wireDelay() 都是 Circuit 的 getter（内部转发给引擎），直接调用取值而不读私有字段

                             m_simulationSpeed, m_simulationSpeedExp, m_autoSpeed,
                             // 继续传参：仿真速度、速度指数、自动速度标志
                             // 这三个共同描述"仿真跑多快"，属于用户对电路的可重现配置

                             gateDelays);
    // 最后一个参数：门类型延迟覆盖表；至此调用闭合
    // ——（注意 showSimTime/wireMode 等纯 UI 显示偏好没有进文件，它们属于全局 QSettings 而不是单个电路文件）

}
// toJson 结束
// ——

bool Circuit::fromJson(const QVariantMap& data) {
    // 定义 fromJson：从 QVariantMap 恢复整个电路状态
    // 这是"打开文件"与"导入文件"共用的反序列化入口。难点是撤销系统的交互：
    //        恢复是一大堆字段的改写，如果每个字段都触发 pushUndo，会导致撤销栈被垃圾填满；
    //        所以这里用 restoring 标志把整个恢复过程"包起来"，并在恢复正常时清空撤销/重做栈（因为旧的历史对新文档无意义）

    CircuitUndoData* d = und();
    // 取得撤销数据指针
    // und() 通过动态属性取指针（Circuit.h 第 476 行说明：用 void* 包装以避免头文件暴露完整类型）；可能返回 nullptr（构造期）

    bool wasRestoring = d ? d->restoring : false;
    // 记录调用前的 restoring 状态
    // 嵌套调用时（例如"撤销的操作恰好触发了 fromJson"）要把状态原样还回去，不能简单地恢复成 false，
    //        否则会破坏外层撤销流程对 restoring 的判断

    if (d) d->restoring = true;
    // 进入"恢复中"状态
    // restoring 为真时 pushUndoInternal 会直接返回（见其实现），从而屏蔽后续所有内部的入栈请求

    JsonCodec::DecodedData dec = JsonCodec::decode(data);
    // 把 map 解码成强类型结构 DecodedData（含 root / subContexts / subNames / 时钟 / 计数器 / 引擎延迟表等）
    // 与 encode 对称：所有"字段名 → 成员"的映射都集中在 JsonCodec，Circuit 不接触 JSON 字段名，便于格式演进

    m_root = dec.root;
    // 用解码结果替换根上下文
    // 这是核心数据；赋值后旧根的元件与连线全部被丢弃

    m_subContexts = dec.subContexts;
    // 替换所有子电路定义
    // 同上

    m_subNames = dec.subNames;
    // 替换子电路名称表
    // 名称与定义必须成对恢复，否则 subcircuits() 的遍历可能出现"有名字无内容"

    m_currentCtxId = "";
    // 加载后回到根上下文
    // 文件里保存的"当前在编辑哪个子电路"没有意义（打开文件应先展示主电路）；
    //        同时清掉悬空 ID，避免后续 ctxById 返回 nullptr

    m_viewOnly = false;
    // 退出只读浏览
    // 新打开的文件一定处于可编辑状态

    m_clockStates = dec.clockStates;
    // 恢复每个时钟元件的通断状态
    // 时钟状态是"电路行为"的一部分：保存时如果某个时钟是 1，重开后应保持 1，否则会以为电路变了

    m_subCounter = dec.subCounter;
    // 恢复子电路编号计数器
    // ★ 必须恢复：若从头计数，新加的子电路可能生成已存在的 ID（"S2"），导致 m_subContexts[S2] 被静默覆盖，旧子电路凭空消失

    m_clockTickCount = dec.clockTickCount;
    // 恢复时钟跳变计数
    // 显示用累计值，保持一致

    m_needFullApply = true;
    // 标记下次 apply 全量
    // 引擎即将被重建，新引擎没有脏记录

    m_importedFiles.clear();
    // 清空导入记录
    // 导入记录（路径 → 前缀 → 子电路列表）没有写进文件（属于会话级状态），打开新文件后旧记录必然失效

    if (m_eventEngine) {
        // 【条件】引擎存在时才处理延迟覆盖
        // 引擎为空的极端情况（构造异常）下跳过，避免空指针

        m_eventEngine->clearGateTypeDelays();
        // 先清空引擎里现有的延迟覆盖
        // 文件里的覆盖表可能是空表，若不清空，上一个文档留下的覆盖会"串"到当前文档上

        for (auto it = dec.gateTypeDelays.begin(); it != dec.gateTypeDelays.end(); ++it)
            // 遍历解码出的覆盖表逐项设置
            // EventEngine 只提供 setGateTypeDelay 单键接口，没有批量接口，只能循环（循环体在下一行）

            m_eventEngine->setGateTypeDelay(it.key(), it.value());
        // 把 "门类型 → 延迟" 写入引擎
        // it.key() 是类型名，it.value() 是 quint64 延迟值

    }
    // 延迟恢复块结束
    // ——

    if (m_engineType == "event") rebuildEventEngine();
    // 【条件】事件引擎模式下重建拓扑
    // 换了一整套元件/连线，引擎必须整体重建；rebuild 会把 m_engineDirty 置 false、m_needFullApply 置 true

    else evaluateAll();
    // 【另一分支】迭代引擎模式下直接全量求值
    // iter 引擎无状态，算一遍即可得到所有元件的输出；这样打开文件后界面立刻显示正确的值，
    //        而不是文件里保存的（可能过期的）outputPorts

    if (!wasRestoring) {
        // 【条件】只有当本次调用不是"嵌套在已有恢复流程中"时才清理撤销历史
        // 如果是嵌套调用（wasRestoring 为真），说明外面还有一层在操作快照栈，这里清空会破坏外层的 undo/redo 逻辑

        if (d) {
            // 【条件】撤销数据存在才清
            // d 为空的构造期没有栈可清

            d->undoStack.clear();
            // 清空撤销栈
            // ★ 关键设计：撤销栈里的快照属于"旧文档"，让用户对新打开的文件撤销，会出现"撤销后发现电路变成了另一个文件"的灾难；
            //        因此"打开文件"被视为历史的分界线，历史全部作废

            d->redoStack.clear();
            // 清空重做栈
            // 同理，重做指向的也是旧历史

            d->lastMergeKey.clear();
            // 清空上次的合并键
            // 否则下一次操作若恰好用同一个 mergeKey，会被误判为"可以合并"而跳过入栈，导致丢失一次撤销点

            d->lastUndoTime = QDateTime();
            // 重置合并时间戳为"无效时间"
            // 无效时间使 lastUndoTime.isValid() 为 false，后续合并判断直接失败，保证新历史从干净状态开始

        }
        // 清栈块结束
        // ——

    }
    // 历史清理条件块结束
    // ——

    if (d) d->restoring = wasRestoring;
    // 恢复调用前的 restoring 状态
    // 必须用 wasRestoring 而不是硬写 false：嵌套调用时外层仍处于恢复中，写成 false 会解除外层的保护

    if (wasRestoring) return true;
    // 【提前返回】嵌套调用时到此为止，不发信号
    // ★ 这是一个容易被忽略但很关键的细节：外层恢复流程结束时会统一发信号刷新界面；
    //        如果内层也发一遍，轻则重复重绘，重则引发 UI 依赖 changed 重新进入某些流程造成递归

    emit contextChanged();
    // 通知上下文切换（已回到根）
    // 面包屑/标题需要更新

    emit subcircuitsChanged();
    // 通知子电路列表变化
    // 侧边栏重建

    emit editContextsChanged();
    // 通知可编辑上下文列表变化
    // 下拉框重建

    emit errorsChanged();
    // 通知错误列表变化
    // 错误列表是按旧电路算的，现在已经不适用；这里没有重新调用 refreshErrors（加载路径不强制重算），
    //        由 UI 侧按需触发

    emit clockChanged();
    // 通知时钟状态变化
    // 时钟状态表被整体替换

    emit engineChanged();
    // 通知引擎类型变化
    // 文件里保存的 engineType 会在别处（loadSettings/JsonCodec 协作）同步到 m_engineType，此处一并通知以防万一

    emit engineSettingsChanged();
    // 通知引擎参数变化
    // 延迟、速度等参数已从文件恢复

    emit structureChanged();
    // 通知结构变化
    // 整套元件/连线都换了，QML 的连线网络缓存必须重建

    emit changed();
    // 通知通用数据变化
    // 画布重绘

    return true;
    // 返回成功
    // 注意：即使 data 完全无效（JsonCodec::decode 给出空结构），这里也返回 true——
    //        目前的实现把"能解码"视为成功，不做内容校验；若需要严格校验，应在 decode 后检查 root 是否合理

}
// fromJson 结束
// ——

static QByteArray extractFirstJsonObject(const QByteArray& data) {
    // 定义文件级静态工具函数：从一段可能"脏"的字节里提取出第一个完整的 JSON 对象（括号配对的那一段）
    // 实际使用中电路文件可能被外部工具追加了注释、日志、截断的尾巴等内容，导致 QJsonDocument::fromJson 整体解析失败；
    //        与其直接报错，不如尝试"抢救"出第一个 { ... } 块——这是导入/打开文件容错能力的基础

    int depth = 0;
    // 当前花括号嵌套深度
    // 用深度配对判断"对象何时结束"：遇到 '{' 加一，'}' 减一，回到 0 即第一个对象收尾

    int start = -1;
    // 第一个对象的起始字节下标；-1 表示尚未找到
    // 只有在 depth 从 0 变 1 的那一刻才是"外层对象开始"

    bool inStr = false;
    // 是否处于字符串字面量内部
    // 字符串里的 '{' '}' 不能算结构括号，否则 "a{b" 这样的值会打乱配对

    bool esc = false;
    // 上一个字符是否是反斜杠转义
    // "\"" 是转义引号，不能当作字符串结束；需要跳过被转义的字符

    for (int i = 0; i < data.size(); ++i) {
        // 逐字节扫描
        // JSON 结构解析必须按字节状态机走，不能用 indexOf('}') 这种简单办法

        char c = data[i];
        // 取当前字节
        // QByteArray::operator[] 是 O(1) 且不拷贝

        if (esc) { esc = false; continue; }
        // 【状态 1】若上一字符是转义符，本字符被跳过，并清除转义标记
        // 转义序列是两字节（\\、\"、\n……），第二个字节无论是什么都不影响结构配对

        if (inStr) {
            // 【状态 2】在字符串内部
            // 只有两种字符有意义：转义符 '\\' 与结束引号 '"'

            if (c == '\\') { esc = true; continue; }
            // 【分支】遇到反斜杠：标记转义并跳过下一个字节
            // 见上

            if (c == '"') { inStr = false; }
            // 【分支】遇到引号：字符串结束
            // 注意这里不 continue，直接落到后面的 continue（在字符串分支末尾），行为是一致的

            continue;
            // 字符串内部其余字符一律忽略
            // 不参与括号计数

        }
        // 字符串状态块结束
        // ——

        if (c == '"') { inStr = true; continue; }
        // 遇到引号：进入字符串状态
        // 从这里开始直到配对的引号，所有括号都不计

        if (c == '{') {
            // 【状态 3】遇到左花括号
            // 可能是外层对象开始，也可能是嵌套对象

            if (depth == 0) start = i;
            // 深度为 0 时说明这是最外层对象的第一个 '{'，记录起点
            // 若深度不为 0，说明在某个对象内部，这个 '{' 属于嵌套

            ++depth;
            // 深度加一
            // ——（位置放在记录 start 之后，保证记录的是"进入前"的深度判断）

        } else if (c == '}') {
            // 【状态 4】遇到右花括号
            // 与 '{' 配对，可能标志外层对象结束

            --depth;
            // 深度减一
            // ——

            if (depth == 0 && start >= 0)
                // 【条件】深度回到 0 且确实记录过起点 → 第一个对象到此结束
                // start < 0 说明还没有遇到任何 '{'（例如前导文本里只有一个孤立的 '}'），此时不能返回

            return data.mid(start, i - start + 1);
            // 返回 [start, i] 区间（含首尾括号）的子串
            // mid(start, len) 的 len = i - start + 1，正好把收尾的 '}' 包含进来；
            //        按值返回 QByteArray 是隐式共享的，不会真的复制这段字节

        }
        // 括号处理结束
        // ——

    }
    // 扫描结束
    // 如果没有在循环里 return，说明没有找到完整的配对对象

    return QByteArray();
    // 返回空字节数组表示"提取失败"
    // 调用方用 isEmpty() 判断失败，而不是引入额外的返回值/异常

}
// extractFirstJsonObject 结束
// ——

static bool isOnlyWhitespaceFrom(const QByteArray& data, int pos) {
    // 定义工具函数：判断从 pos 开始到末尾是否全是空白字符
    // loadFromFile 抢救文件后要决定"要不要回写修复"：如果对象后面只有空白（换行、缩进），说明文件本来就是干净的，
    //        无需回写；只有后面还有实质内容（被追加的垃圾）才值得回写清理

    for (int i = pos; i < data.size(); ++i) {
        // 从 pos 扫到末尾
        // ——

        char c = data[i];
        // 取当前字节
        // ——

        if (c != 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) return false;
        // 若遇到非空白字节：空格(0x20)/制表(0x09)/换行(0x0A)/回车(0x0D) 之外的任何字节，立即返回 false
        // 手工比较 ASCII 码而不用 QChar::isSpace，是因为这里直接操作 QByteArray（字节层），不走 Unicode 解码，更快也更安全

    }
    // 扫描结束
    // 没遇到非空白字节

    return true;
    // 返回 true 表示"从 pos 起全是空白"
    // ——（pos 越界时循环不执行，也返回 true，语义上"空区间里没有非空白"成立）

}
// isOnlyWhitespaceFrom 结束
// ——

bool Circuit::saveToFile(const QString& path) {
    // 定义 saveToFile：把当前电路序列化并以带缩进的 JSON 写入指定路径
    // 这是"保存 / 另存为"的实现。流程：toJson → QJsonObject → 格式化字节 → 打开文件 → 写入 → 校验写入长度 → 记录最近文件

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空直接失败
    // 没有路径就无法确定写到哪；返回 false 让调用方（QML 文件对话框）自己提示

    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    // 把 toJson() 得到的 QVariantMap 转成 QJsonObject
    // QJsonDocument 只能包裹 QJsonObject/QJsonArray；fromVariantMap 会递归把 QVariant 里的 map/list/基本类型转换成 JSON 兼容类型

    QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    // 序列化成带缩进的 UTF-8 字节
    // Indented 格式可读、可 diff、便于用户手工排查；如果更在意体积应改用 Compact

    if (bytes.isEmpty()) return false;
    // 【提前返回】序列化结果为空视为失败
    // 空字节通常意味着序列化环节出了异常；此时若继续写文件，会把原文件截断成 0 字节，造成数据丢失

    QFile f(path);
    // 以原始路径构造 QFile 对象（尚未打开）
    // QFile 的构造不会碰磁盘，真正打开在下一段；先构造是为了统一处理"content://"与普通路径两种情况的变量

    if (path.startsWith("content://")) {
        // 【分支】Android 的 content URI 路径
        // Android 的 SAF（存储访问框架）返回的是 content:// 形式的 URI，不能经 toLocalFile() 转成本地路径，
        //        但 Qt 在 Android 上可以让 QFile 直接接受这种 URI；因此这条分支保持原路径不变

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        // 以"只写 + 文本模式"打开
        // ★ 这里没有 Truncate：SAF 返回的流是"整体替换"语义，QIODevice::Truncate 在某些 provider 上会失败或行为异常，
        //        所以 content:// 分支只用 WriteOnly|Text；Text 模式在 Windows 上会把 '\n' 转成 "\r\n"，保证记事本也能正常显示

    } else {
        // 【分支】普通文件路径（含 file:// URI）
        // 桌面平台走这里

        QString local = path;
        // 准备本地路径副本
        // 下面可能要把 file:// 转成普通路径，不能直接改参数 path（保存到最近文件列表时需要用原始 path）

        if (local.startsWith("file://")) local = QUrl(local).toLocalFile();
        // 若是 file:// URI，转换成平台本地路径
        // QML 的文件对话框在部分平台返回 file:///C:/x.json 这种形式，直接给 QFile 会因为冒号/斜杠处理不当而打不开

        f.setFileName(local);
        // 把 QFile 的文件名换成转换后的本地路径
        // QFile 在构造时已绑定原路径，这里必须显式更新

        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        // 以"只写 + 截断 + 文本"打开
        // Truncate 保证旧内容被清除（否则新内容比旧的短时，尾部会残留旧字节，生成非法 JSON）；
        //        ★ 注意：本文件包含头文件 QSaveFile 却没有用它——这里用的是"直接截断再写"，若写入过程崩溃，原文件会被破坏；
        //        更健壮的写法是使用 QSaveFile + commit()，属于已知的可改进点

    }
    // 路径分支结束
    // 此时文件已打开（f.isOpen()==true）

    qint64 n = f.write(bytes);
    // 写入全部字节，返回实际写入的字节数
    // QIODevice::write 可能部分写入（磁盘满、管道），返回实际字节数用于校验

    if (n != bytes.size()) { f.close(); return false; }
    // 【错误处理】写入字节数不等于预期 → 关闭文件并返回失败
    // ★ 必须先 close 再返回：不关闭会泄漏文件句柄，且在 Windows 上句柄未释放时文件被锁住，用户再次保存可能失败

    f.close();
    // 正常路径下关闭文件
    // 写完后必须 flush 并释放句柄，才能保证数据真正落盘、文件解锁

    addRecentFile(path);
    // 把该路径加入"最近打开文件"列表（去重、置顶、截断、持久化）
    // 用原始的 path 而不是转换后的 local：最近文件列表要保存 QML 能再次直接打开的原始形式

    return true;
    // 返回成功
    // ——

}
// saveToFile 结束
// ——

bool Circuit::loadFromFile(const QString& path) {
    // 定义 loadFromFile：从文件读取电路，解析 JSON（带容错），交给 fromJson 恢复
    // 这是"打开文件"的实现。相比 saveToFile，它多了三层容错：
    //        ① 整体解析失败时尝试提取第一个完整 JSON 对象；
    //        ② 恢复后如果发现文件里对象后面还有垃圾，回写"清洁版"；
    //        ③ 任何一步失败都返回 false 而不是崩溃，保证已加载的旧文档不被破坏

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空失败
    // 同 saveToFile

    QFile f(path);
    // 用原始路径构造 QFile
    // 注意：这里**没有**处理 file:// → toLocalFile 的转换（与 saveToFile 不同），
    //        这是两者行为上的一处不一致；在桌面平台上若 QML 传 file:// URI，需要调用方自行转换

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    // 以"只读 + 文本"打开
    // 文本模式会做换行转换，读到的内容与 JSON 解析器期望的一致；打不开（不存在/无权限）直接失败

    QByteArray data = f.readAll();
    // 一次性读入全部内容
    // .qcirc 这类电路文件都是几 KB～几 MB 的纯文本，一次 readAll 最简单；分段读只会增加状态机复杂度

    f.close();
    // 读完后立即关闭文件
    // 尽早释放句柄，后续（修复回写时）如果要对同一路径重新打开写入，Windows 上的独占锁才不会冲突

    QJsonParseError err;
    // 声明解析错误输出参数
    // fromJson 失败时必须区分"文件不是 JSON"与"JSON 合法但内容不对"，需要 err 来区分

    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    // 先尝试整体解析
    // 正常文件（99% 情况）一次解析成功，走最快路径；只有失败才进入容错分支

    QByteArray cleanData;
    // 准备"抢救出来的干净数据"
    // 只有在容错分支里才会赋值，默认空；后面用 needsHeal 决定是否回写

    bool needsHeal = false;
    // 是否需要"治愈"（回写清理后的文件）
    // 默认不需要；只有"成功抢救 + 对象后面还有垃圾"时才需要

    if (err.error != QJsonParseError::NoError) {
        // 【容错分支】整体解析失败
        // 文件可能被外部工具追加了日志/注释，或者被截断、混入 BOM 等

        cleanData = extractFirstJsonObject(data);
        // 尝试提取第一个完整的 { ... } 块
        // 见该函数的实现说明：用括号配对 + 字符串状态机，能可靠地切出第一个 JSON 对象

        if (cleanData.isEmpty()) return false;
        // 【提前返回】提取失败（文件里根本没有完整对象）→ 打开失败
        // 宁可不加载，也不能加载半个电路覆盖用户当前文档

        if (cleanData.size() == data.size()) return false;
        // 【提前返回】提取结果与原数据等长 → 视为失败
        // ★ 看起来矛盾（等长说明整个文件就是一个对象，那应该能解析才对）——正因为如此，
        //        能走到这里说明**整体解析失败但括号又是配对的**，意味着内容本身有语法错误（例如对象里有非法字符），
        //        再解析一遍 cleanData 也会失败；这里直接返回可以省一次解析

        int endPos = data.indexOf(cleanData) + cleanData.size();
        // 计算被提取对象在原数据中的结束位置（下一字节的下标）
        // 后面要从这个位置往后检查是否还有垃圾内容。
        //        注意：indexOf(cleanData) 若返回 -1（理论上不会，因为 cleanData 来自 data），endPos 会变成 -1+size，
        //        属于已知的边界瑕疵

        needsHeal = !isOnlyWhitespaceFrom(data, endPos);
        // 检测对象之后是否有非空白内容
        // 有垃圾 → 记为需要回写清理；只有空白 → 文件本来就是干净的，不必写盘（避免无谓的 IO 与时间戳变化）

        QJsonParseError err2;
        // 为第二次解析准备独立的错误输出
        // 不能复用 err：那样会把第一次的错误信息覆盖掉（虽然此处已不再需要），语义上也不清晰

        QJsonDocument doc2 = QJsonDocument::fromJson(cleanData, &err2);
        // 解析提取出来的片段
        // 片段可能本身也不合法（例如字符串在中间被截断），必须再验证一次

        if (err2.error != QJsonParseError::NoError) return false;
        // 【提前返回】片段解析仍失败 → 打开失败
        // ——

        if (!doc2.isObject()) return false;
        // 【提前返回】片段不是对象（例如是数组）→ 失败
        // 电路文件顶层约定为对象；数组无法交给 toVariantMap

        doc = doc2;
        // 用抢救出的解析结果替换 doc
        // 后面统一走 doc 处理，两条路径在此汇合

    }
    // 容错分支结束
    // ——

    if (!doc.isObject()) return false;
    // 【统一校验】解析成功但顶层不是对象 → 失败
    // 这是两条路径（正常解析 / 容错解析）汇合后的统一检查，防止把数组当电路

    bool ok = fromJson(doc.object().toVariantMap());
    // 把 JSON 对象转成 QVariantMap 并恢复电路
    // toVariantMap 会递归转回 QVariant 容器；fromJson 内部再交给 JsonCodec::decode 做字段级映射

    if (!ok) return false;
    // 【提前返回】恢复失败 → 打开失败
    // 目前的 fromJson 总是返回 true，这条分支是防御性的扩展点

    if (needsHeal) {
        // 【条件】需要治愈：把"干净片段"回写到原文件
        // 用户下次再打开时就能走正常解析路径；同时清理掉被外部追加的垃圾，避免文件不断膨胀

        QFile wf(path);
        // 为回写重新构造一个 QFile
        // 之前的 f 已经关闭，且重新构造可以避免遗留状态（如位置指针）；注意这里同样没有处理 file:// URI

        bool openOk = path.startsWith("content://")
                          // 按路径类型选择打开标志（跨两行的三目表达式）
                          // 与 saveToFile 完全相同的逻辑：content:// 不能 Truncate，普通路径要 Truncate

                          ? wf.open(QIODevice::WriteOnly | QIODevice::Text)
                          // 【分支】content:// 分支的打开标志
                          // 见 saveToFile 中的详细说明

                          : wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        // 【分支】普通路径的打开标志（含 Truncate）
        // 回写的 cleanData 通常比原文件短，不截断会残留尾部旧字节，文件仍然是非法的

        if (openOk) { wf.write(cleanData); wf.close(); }
        // 打开成功则写入干净数据并关闭
        // ★ 回写失败**不影响**打开成功的结论：电路已经加载到内存，文件修复只是附加收益；
        //        因此写入结果不检查，也不因失败而返回 false（这保证了"只读介质的文件也能被打开"）

    }
    // 治愈分支结束
    // ——

    addRecentFile(path);
    // 记录最近打开的文件
    // 和保存一样，打开也是"最近文件"的主要来源

    return ok;
    // 返回结果（此处必为 true）
    // 用 ok 而不是字面量 true，是为了将来 fromJson 可能返回 false 时逻辑自动正确

}
// loadFromFile 结束
// ——

// ============================================================================
// ⑤ 外部导入生命周期：把一个电路文件挂载为若干子电路
// ============================================================================

void Circuit::removeSubContextsByPrefix(const QString& prefix) {
    // 定义 removeSubContextsByPrefix：把所有 ID 以 prefix 开头的子电路连同它们在**所有**上下文里的实例一起删除
    // 导入外部文件时，为了不与当前文档的 ID 冲突，会给导入进来的每个 ID 加统一前缀（形如 "IMP3_"）；
    //        卸载或重新导入时，按这个前缀就能一次性精确地找出"某次导入产生的所有东西"，不会误删用户自己的子电路

    if (prefix.isEmpty()) return;
    // 【提前返回】空前缀直接返回
    // ★ 安全阀：空前缀 startsWith("") 对所有字符串都为真，会把**全部**子电路删光——这是灾难性后果，
    //        必须在这里挡住

    QSet<QString> subIdSet;
    // 收集所有以 prefix 开头的子电路 ID
    // 先收集再处理：避免边遍历 m_subContexts 边删除（QHash 在遍历中删除元素会使迭代器失效）；
    //        用 QSet 是因为后面要在多个上下文的元件里做"是否属于这批"的 O(1) 判断

    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        // 遍历所有子电路
        // 用 constBegin/constEnd，因为本轮只读不写；另一个好处是某些 Qt 版本中对 const 迭代器的写时复制检测更本分

        if (it.key().startsWith(prefix)) subIdSet.insert(it.key());
        // 【条件】ID 以 prefix 开头则加入集合
        // startsWith 是前缀匹配的线性操作，前缀通常很短，开销可忽略

    }
    // 收集结束
    // ——

    if (subIdSet.isEmpty()) return;
    // 【提前返回】没有任何匹配 → 无事可做
    // 幂等性：重复调用（例如 removeImportedFile 被连点两次）不会出错

    for (const auto& sid : subIdSet) {
        // 逐个删除已收集的子电路
        // 此时已经从遍历中退出，可以安全修改容器；用 sid 的**副本**（const auto& 绑定到集合元素，集合本身未被修改）删除哈希表项是安全的

        m_subContexts.remove(sid);
        // 删除子电路定义
        // QHash::remove 对不存在的 key 是空操作，不必先 contains 判断

        m_subNames.remove(sid);
        // 删除对应名称
        // 名称表必须与内容同步，否则 subcircuits() 会出现幽灵条目

    }
    // 删除结束
    // 此时这些子电路的"定义"已经不存在，但引用它们的 sub 实例还留在各上下文里

    auto removeInstances = [&](Context& ctx) {
        // 定义局部 lambda：从**一个**上下文里删除所有引用了已删子电路的 sub 实例，以及这些实例的连线
        // 实例残留会引用不存在的 subId，渲染时找不到定义、求值时找不到内部元件，属于必须清理的悬空引用

        for (int i = ctx.components.size() - 1; i >= 0; --i) {
            // 从后往前遍历元件
            // ★ 倒序遍历是删除容器元素的标准技巧：删除下标 i 后，前面（i-1 及之前）的索引不受影响，循环仍正确；
            //        若正序删除，后面元素会前移，导致漏删或越界

            auto c = ctx.components[i].toMap();
            // 取出当前元件 map
            // ——（auto 推导为 QVariantMap）

            if (c.value("type").toString() != "sub") continue;
            // 【跳过条件】不是子电路实例的元件跳过
            // 只有 sub 实例才可能引用被删的子电路

            if (!subIdSet.contains(c.value("subId").toString())) continue;
            // 【跳过条件】实例引用的子电路不在待删集合里则跳过
            // 精确清理：只删"这次导入产生的"实例，用户自己的子电路实例保持不动

            QString id = c.value("id").toString();
            // 记录该实例的 ID
            // 删实例之前要先删所有碰到它的连线，需要用它做匹配

            for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                // 倒序遍历连线，删除与这个实例相连的所有线
                // 同样倒序保证删除安全；★ 必须先删线再删元件：如果先删元件，连线里就会留下指向不存在 ID 的悬空引用，
                //        下一次求值时引擎/命中测试会因此访问越界或得到错误结果

                auto w = ctx.wires[j].toMap();
                // 取出连线 map
                // ——

                if (w.value("fromComp").toString() == id ||
                    // 【条件（前半）】连线的源端是待删实例
                    // 两个条件用 || 连接，只要任一端相连就要删（跨两行的 if 条件）

                        w.value("toComp").toString() == id)
                    // 【条件（后半）】连线的目标端是待删实例
                    // ——（注意 ortho 线若没有 fromComp/toComp 字段，value() 返回空 QVariant，toString() 为空串，不会误删）

                ctx.wires.removeAt(j);
                // 删除该连线
                // removeAt 会移动后续元素，但因为倒序遍历且 j 是独立于 i 的内层循环，不会影响外层的元件遍历；只是内层后续索引已递减，安全

            }
            // 连线删除循环结束
            // ——

            ctx.components.removeAt(i);
            // 删除该 sub 实例元件
            // 此时它的连线已经清空，删除不会留下悬空线；外层倒序遍历保证前面的索引不失效

        }
        // 元件遍历结束
        // ——

    };
    // lambda 定义结束
    // ——

    removeInstances(m_root);
    // 先从根上下文清理
    // 根是实例最常见的所在层

    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 再遍历剩余的子电路上下文
        // 子电路内部也可能放了被删子电路的实例（嵌套引用），必须一并清理；
        //        注意此时 m_subContexts 已不含被删项，遍历是安全的

        removeInstances(it.value());
    // 对该子电路执行清理
    // it.value() 是引用，直接原地修改

    if (subIdSet.contains(m_currentCtxId)) {
        // 【条件】当前正在浏览/编辑的上下文恰好是被删的子电路之一
        // 此时必须把"当前位置"切回根，否则整个 UI 会停留在一个已不存在的上下文上

        m_currentCtxId = "";
        // 切回根上下文（空串约定）
        // ——

        m_viewOnly = false;
        // 退出只读浏览
        // 被浏览的实例已经不存在，浏览态必须解除

        m_currentInstancePath.clear();
        // 清空实例路径
        // 路径里的实例 ID 已失效，留着会让 syncSubStateFromParent 找到错误的父实例

        m_browseStack.clear();
        // 清空浏览栈
        // 栈里保存的是"进入之前的上下文"，其中可能也有被删的子电路；整体清空比逐层修补简单可靠

        emit contextChanged();
        // 通知 UI 上下文已切换
        // 面包屑/画布需要立即刷新到根

    }
    // 当前位置修复分支结束
    // ——

    m_ctxSubInstCache.clear();
    // 清空实例缓存
    // ★ 必须清：缓存里记录"某子电路被哪些实例引用"，而这些实例刚刚被删掉了；
    //        不清的话 engineQidsFor 会返回已不存在的限定 ID，后续输入操作会打向幽灵节点

}
// removeSubContextsByPrefix 结束
// ——

bool Circuit::importFromFile(const QString& path) {
    // 定义 importFromFile：把外部电路文件导入为当前文档中的一组子电路
    // "导入"与"打开"的区别：打开=替换整个文档；导入=把外部电路**作为一个可复用的子电路模块**挂进来，
    //        用户可以多次导入、在多个地方实例化。难点在于 ID 冲突：
    //        外部文件里的元件/连线/子电路 ID 与当前文档很可能重复，必须整体"命名空间化"（加前缀）

    if (path.isEmpty()) return false;
    // 【提前返回】路径为空失败
    // ——

    if (m_viewOnly) return false;
    // 【提前返回】只读浏览禁止导入
    // 浏览模式下不允许改变电路结构

    QFile f(path);
    // 打开源文件
    // 导入的源是用户选中的另一个电路文件；同样没有做 file:// 转换（与 loadFromFile 一致的行为）

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    // 只读 + 文本模式打开
    // 打不开直接失败

    QByteArray data = f.readAll();
    // 读入全部内容
    // 电路文件通常较小，一次读完最简单

    f.close();
    // 关闭文件
    // ——

    if (data.isEmpty()) return false;
    // 【提前返回】空文件视为失败
    // 导入一个空文件没有意义，提前挡住可以避免后续构造出一组空子电路

    QJsonParseError err;
    // 解析错误输出
    // 与 loadFromFile 相同的容错策略

    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    // 先整体解析
    // 正常文件一次成功

    if (err.error != QJsonParseError::NoError) {
        // 【容错分支】整体解析失败
        // 与 loadFromFile 相同：尝试提取第一个完整对象

        QByteArray clean = extractFirstJsonObject(data);
        // 提取第一个 JSON 对象
        // 复用那个静态工具函数，逻辑与打开文件完全一致

        if (clean.isEmpty()) return false;
        // 【提前返回】提取失败 → 导入失败
        // ——

        QJsonParseError err2;
        // 第二次解析的错误输出
        // 独立变量

        QJsonDocument doc2 = QJsonDocument::fromJson(clean, &err2);
        // 解析提取出的片段
        // ——

        if (err2.error != QJsonParseError::NoError || !doc2.isObject()) return false;
        // 【提前返回】片段非法或不是对象 → 失败
        // 用 || 合并两个失败条件，省一层嵌套

        doc = doc2;
        // 用法结果替换 doc
        // 两条路径汇合

    }
    // 容错分支结束
    // ——

    if (!doc.isObject()) return false;
    // 【统一校验】顶层必须是对象
    // 与 loadFromFile 相同

    JsonCodec::DecodedData dec = JsonCodec::decode(doc.object().toVariantMap());
    // 解码成强类型结构
    // ★ 注意：这里**不做** fromJson——导入不替换当前文档，只是把解码结果"搬运"到当前文档的子电路表里

    QString baseName = QFileInfo(path).completeBaseName();
    // 取源文件的"基名"（去掉最后一个扩展名的文件名）
    // 用来给导入后的子电路/实例命名，让用户一眼看出"这是从哪个文件来的"；用 completeBaseName 而不是 baseName：
    //        对 "a.b.json" 这样的文件名，completeBaseName 得到 "a.b"，baseName 只得到 "a"

    if (baseName.isEmpty()) baseName = "外部电路";
    // 基名为空时用默认名"外部电路"
    // 防止路径异常（例如以 / 结尾、或是 content:// URI）导致名字为空，进而在界面上显示空白项

    if (m_importedFiles.contains(path)) {
        // 【分支 A】这个文件之前已经导入过 → 先卸载旧的导入
        // "重复导入同一文件"应当视为"刷新"：直接再导入一遍会导致同一模块存在两份，用户下次实例化时不知道该选哪个；
        //        先按旧前缀清理，再生成新前缀重新导入，语义最直观

        QString oldPrefix = m_importedFiles.value(path).value("prefix").toString();
        // 取出上次导入使用的前缀
        // removeSubContextsByPrefix 需要它来精确清理

        removeSubContextsByPrefix(oldPrefix);
        // 按前缀删除上次导入的全部子电路与实例
        // 这一步也负责清理实例与连线、处理"当前正浏览被删上下文"的情况

        m_importedFiles.remove(path);
        // 从导入记录里移除旧条目
        // 后面会写入新条目；先删再插保证不会残留旧 prefix

    } else {
        // 【分支 B】首次导入这个文件
        // 还要额外处理一种历史情况：文档里可能已经存在"同名后缀"的子电路（例如保存过、但 m_importedFiles 是会话级状态、重开后丢失）

        QString suffix = QString("（%1）").arg(baseName);
        // 构造导入时使用的名称后缀，形如 "（mycircuit）"（注意是全角括号）
        // 导入后的子电路名统一为 "原名（文件名）"；这个后缀既用于命名，也用于下面识别"孤儿导入项"

        QStringList stale;
        // 收集"看起来是该文件导入产物"的子电路 ID
        // suffix 是识别依据

        for (auto it = m_subNames.constBegin(); it != m_subNames.constEnd(); ++it) {
            // 遍历所有子电路名
            // 只读遍历，用 const 迭代器

            if (it.value().endsWith(suffix)) stale.append(it.key());
            // 【条件】名字以该后缀结尾 → 认定为上次导入的残留
            // 用 endsWith 而不是 ==：导入的子电路名可能是 "加法器（mycircuit）" 这种"原名+后缀"形式

        }
        // 扫描结束
        // ——

        if (!stale.isEmpty()) {
            // 【条件】确实发现了残留
            // 需要先清理，否则会出现两批同名子电路

            pushUndoAll();
            // 清理之前先压一次**全量**撤销快照
            // 清理会跨多个上下文删除数据，只有 Full 范围的快照才能完整恢复；这也是"重新导入"整体算一步可撤销操作的开端

            QSet<QString> staleSet;
            // 把残留 ID 转成集合
            // 后面要在元件上做 O(1) 的"是否残留"判断

            for (const auto& sid : stale) staleSet.insert(sid);
            // 逐个插入集合
            // 单行循环体省略花括号

            for (const auto& sid : stale) {
                // 逐个删除残留子电路定义与名称
                // ★ 没有直接调用 removeSubContextsByPrefix：因为残留项不一定共享同一个前缀（可能是不同次会话导入的），
                //        只能用显式收集到的 ID 集合逐个删；注意"实例清理"代码在下面被重复写了一遍（与 removeSubContextsByPrefix 逻辑相同）

                m_subContexts.remove(sid);
                // 删除定义
                // ——

                m_subNames.remove(sid);
                // 删除名称
                // ——

            }
            // 删除结束
            // ——

            auto removeInstances = [&](Context& ctx) {
                // 定义局部 lambda：从某上下文中删除引用了残留子电路的实例及连线
                // 逻辑与 removeSubContextsByPrefix 内的 lambda 完全相同——这是一处可复用的重复代码

                for (int i = ctx.components.size() - 1; i >= 0; --i) {
                    // 倒序遍历元件
                    // 边遍历边删除，倒序才不会漏

                    auto c = ctx.components[i].toMap();
                    // 取元件 map
                    // ——

                    if (c.value("type").toString() != "sub") continue;
                    // 【跳过条件】非 sub 跳过
                    // ——

                    if (!staleSet.contains(c.value("subId").toString())) continue;
                    // 【跳过条件】引用的不是残留子电路则跳过
                    // 精确清理

                    QString id = c.value("id").toString();
                    // 记录实例 ID
                    // 删线时用

                    for (int j = ctx.wires.size() - 1; j >= 0; --j) {
                        // 倒序遍历连线
                        // ——

                        auto w = ctx.wires[j].toMap();
                        // 取连线 map
                        // ——

                        if (w.value("fromComp").toString() == id ||
                            // 【条件（前半）】源端是待删实例
                            // ——

                                w.value("toComp").toString() == id)
                            // 【条件（后半）】目标端是待删实例
                            // ——

                        ctx.wires.removeAt(j);
                        // 删除该线
                        // 先删线再删元件，理由同 removeSubContextsByPrefix

                    }
                    // 内层循环结束
                    // ——

                    ctx.components.removeAt(i);
                    // 删除该实例元件
                    // ——

                }
                // 元件循环结束
                // ——

            };
            // lambda 定义结束
            // ——

            removeInstances(m_root);
            // 清理根上下文
            // ——

            for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
                // 遍历剩余子电路继续清理
                // 嵌套实例也要清

                removeInstances(it.value());
            // 清理该子电路
            // ——

            m_ctxSubInstCache.clear();
            // 清空实例缓存
            // 实例被删了，缓存的映射已失效

        } else {
            // 【分支】没有残留
            // 首次导入、文件干净

            pushUndoAll();
            // 仍然压一次全量撤销快照
            // ★ 这个位置很有讲究：不管有没有残留，整个导入动作都必须整体可撤销，所以两个分支都要入栈；
            //        写成 if/else 各 push 一次，而不是在 if 外面统一 push，是因为"有残留"时 push 必须发生在清理之前

        }
        // residual 分支结束
        // ——

    }
    // 已导入分支结束
    // 此时旧数据已经清理干净（或本来就没有），可以开始导入新数据

    QString prefix;
    // 准备前缀字符串
    // do-while 循环里要反复生成前缀，先声明在循环外

    do {
        // do-while 循环：至少执行一次，直到生成的前缀未被占用
        // 前缀必须全局唯一，否则不同批次的导入会互相串数据；
        //        "已占用"的判断标准是 m_subContexts 里已经存在 prefix+"ROOT" 这个 key（ROOT 是本批次根电路的固定后缀）

        prefix = QString("IMP%1_").arg(++m_importCounter);
        // 前缀自增后格式化为 "IMP<n>_"
        // ★ 先自增再格式化（++ 前置），保证进程内前缀递增；下划线作为分隔符，让后续拼出的 ID 可读（IMP3_S1）

    } while (m_subContexts.contains(prefix + "ROOT"));
    // 若该前缀的 ROOT 已存在则继续自增重试
    // m_importCounter 可能在重开后与已有数据冲突（计数器是成员，但用户可能删掉又导入新文件），循环保证最终一定唯一

    QHash<QString, QString> subIdMap;
    // 建立"导入文件内的旧子电路 ID → 本文件内的新子电路 ID"映射
    // 导入进来的子电路之间也会互相引用（sub 实例的 subId 指向同一文件里的另一个子电路），
    //        所有 ID 都加了前缀，引用关系也必须跟着改，否则会指向不存在的 ID

    subIdMap.insert(QString(), prefix + "ROOT");
    // 为"空 ID"（即导入文件的根上下文，与工程约定"空串=根"一致）建立映射
    // ★ 导入文件的根电路作为一个新的子电路挂进来，需要给它一个合法 ID：prefix+"ROOT"；
    //        这样它能参与正常的前缀命名，也能被实例化

    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it)
        // 遍历导入文件里的每个子电路
        // 每个都要建立旧 ID → 新 ID 的映射

        subIdMap.insert(it.key(), prefix + it.key());
    // 新 ID = 前缀 + 旧 ID
    // 保持旧 ID 的可读部分，便于人工排查；前缀保证全局唯一

    auto processCtx = [&](CircuitContext ctx) -> CircuitContext {
        // 定义 lambda：对一个**按值传入**的上下文重写所有 ID，返回修改后的副本
        // ★ 这里有意按值传参而不是引用：它不修改导入源数据（dec），而是生成一份"前缀化"的新上下文返回；
        //        显式写出 -> CircuitContext 返回类型，是为了让闭包内 return ctx 的类型推导明确

        for (int i = 0; i < ctx.components.size(); ++i) {
            // 遍历该上下文的所有元件，逐个改 ID
            // 元件 ID 必须唯一，否则与新上下文里的元件冲突

            QVariantMap c = ctx.components[i].toMap();
            // 取元件 map
            // 要在副本上改字段

            QString oldId = c.value("id").toString();
            // 记录旧 ID
            // 改 c["id"] 之前先取出来，避免读到已改的值

            c["id"] = prefix + oldId;
            // 把元件 ID 改成 "前缀+旧ID"
            // ★ 前缀化是解决导入冲突的核心手段：导入文件里的 "G1" 在本文档变成 "IMP3_G1"，与用户自己的 "G1" 互不干扰

            if (c.value("type").toString() == "sub") {
                // 【条件】该元件是子电路实例时，还要改写它的 subId（引用关系）
                // 实例的 id 改了，但它指向哪个子电路定义（subId）也必须改成前缀化后的 ID，否则会指向本文档里同名的别的子电路或不存在的 ID

                QString oldSubId = c.value("subId").toString();
                // 取旧的 subId
                // 查映射用

                if (subIdMap.contains(oldSubId))
                    // 【条件】映射表里有这个旧 ID 才改写
                    // 可能是引用了一个外部定义（罕见/损坏文件），此时保持原样，交给后续的"定义缺失"防御处理

                c["subId"] = subIdMap.value(oldSubId);
                // 替换为新 ID
                // 这样导入文件内部的实例仍然指向它自己带进来的子电路定义

            }
            // subId 改写结束
            // ——

            ctx.components[i] = c;
            // 写回元件
            // ——

        }
        // 元件遍历结束
        // ——

        for (int i = 0; i < ctx.wires.size(); ++i) {
            // 遍历所有连线，改写 ID 与两端引用
            // 连线 ID 与它引用的元件 ID 都变了，不统一改写就会出现悬空引用

            QVariantMap w = ctx.wires[i].toMap();
            // 取连线 map
            // ——

            w["id"] = prefix + w.value("id").toString();
            // 连线自身 ID 前缀化
            // 避免与本文档已有连线重名（某些地方按 ID 索引连线）

            if (w.contains("fromComp"))
                // 【条件】有源元件字段才改写
                // ortho 线可能没有 fromComp（从线中间引出的线）；不判断而直接写会造成"凭空加一个空串字段"

            w["fromComp"] = prefix + w.value("fromComp").toString();
            // 源元件 ID 前缀化
            // 必须与元件的新 ID 精确对应

            if (w.contains("toComp"))
                // 【条件】有目标元件字段才改写
                // 同上

            w["toComp"] = prefix + w.value("toComp").toString();
            // 目标元件 ID 前缀化
            // 同上

            ctx.wires[i] = w;
            // 写回连线
            // ——

        }
        // 连线遍历结束
        // ——

        ctx.compCounter = ctx.components.size();
        // 重置元件 ID 计数器为当前元件数量
        // ★ 注意：这里用的是"数量"而不是"最大编号+1"。如果导入文件里存在被删除过的编号（例如现有 G1、G3 而计数器原来应是 4），
        //        这里会得到 2，之后新加元件可能生成已存在的 "G2"…… 这是一处潜在的 ID 复用隐患。
        //        之所以这么做，可能是为了让计数与数量保持简单一致；如果发生冲突，IdAllocator 通常会继续探测下一个可用编号

        ctx.wireCounter = ctx.wires.size();
        // 重置连线计数器
        // 同上（同理存在潜在复用隐患）

        return ctx;
        // 返回前缀化后的上下文副本
        // lambda 按值返回，调用方直接拿到新的 CircuitContext

    };
    // processCtx lambda 结束
    // ——

    QStringList newSubIds;
    // 收集本次导入产生的所有新子电路 ID
    // 返回给 UI 展示，也用于记录在 m_importedFiles 的 subIds 里，方便后续精确卸载

    {
        // 用一个复合语句块包住"导入根电路"的逻辑
        // 内部声明的 ctx/newId 只在这里用；块作用域同时起到"命名隔离"的作用

        CircuitContext ctx = processCtx(dec.root);
        // 把导入文件的根上下文前缀化
        // 导入文件的根电路 -> 本文档中的一个新子电路

        QString newId = subIdMap.value(QString());
        // 取根对应的新 ID（即 prefix+"ROOT"）
        // 之前已在映射表里登记；这里用 value 查出来，比手工再拼一次 prefix+"ROOT" 更不容易出错

        m_subContexts[newId] = ctx;
        // 把新子电路写入定义表
        // 此时它已是本文档合法的子电路之一

        m_subNames[newId] = QString("主电路（%1）").arg(baseName);
        // 命名："主电路（文件名）"
        // 导入文件的根电路在界面上显示为"主电路（xxx）"，让用户明确这是导入来的模块；
        //        全角括号与后面 "原名（文件名）" 的命名规则保持一致

        newSubIds.append(newId);
        // 记录进新 ID 列表
        // ——

    }
    // 根电路导入块结束
    // ——

    for (auto it = dec.subContexts.constBegin(); it != dec.subContexts.constEnd(); ++it) {
        // 遍历导入文件里的每个子电路，逐个导入
        // 一个文件可能有多层子电路，全部搬进来才能保证实例引用完整

        CircuitContext ctx = processCtx(it.value());
        // 前缀化该子电路
        // 与根电路相同的处理

        QString newId = subIdMap.value(it.key());
        // 查它的新 ID
        // 映射表已填好

        m_subContexts[newId] = ctx;
        // 写入定义表
        // ——

        QString oldName = dec.subNames.value(it.key(), it.key());
        // 取导入文件里原有的显示名（取不到则用旧 ID 当名字）
        // 保留原名能让用户认出模块结构（例如"加法器""多路选择器"）

        m_subNames[newId] = QString("%1（%2）").arg(oldName, baseName);
        // 新名称 = "原名（文件名）"
        // ★ 这个"（文件名）"后缀不只是给人看的：importFromFile 的分支 B 正是靠 endsWith(suffix) 来识别
        //        "上次导入同名文件留下的残留"，所以命名规则与清理逻辑是强耦合的，改动其一必须同步改另一个

        newSubIds.append(newId);
        // 记录新 ID
        // ——

    }
    // 子电路导入循环结束
    // ——

    QVariantMap info;
    // 构造导入记录
    // 记录足够的信息以便将来精确卸载与重复导入识别

    info["path"] = path;
    // 记录源文件路径（作为 m_importedFiles 的 key 也用它）
    // ——

    info["prefix"] = prefix;
    // 记录本次使用的唯一前缀
    // 卸载时按它批量删除；重复导入时也用它先清理

    info["baseName"] = baseName;
    // 记录基名
    // UI 展示用

    info["subIds"] = newSubIds;
    // 记录该文件产生的全部新子电路 ID
    // 比"按前缀推算"更精确：某些 ID 可能不是简单的 prefix+旧ID（比如 ROOT），直接列出来最可靠

    m_importedFiles[path] = info;
    // 把记录登记到导入表
    // key 用 path：同一个文件重复导入会被识别为"刷新"而不是"新增一份"

    for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it) {
        // 扫描**全部**子电路（含已有的，不只新导入的）
        // 导入可能引入新的 clock 元件，它们需要进入时钟状态表，否则 tick() 时 m_clockStates.value(id,false) 会一直是 false、无法翻转

        for (const auto& cv : it.value().components) {
            // 遍历该子电路的元件
            // 找 clock 类型

            QVariantMap c = cv.toMap();
            // 转成 map
            // ——

            if (c.value("type").toString() == "clock") {
                // 【条件】是时钟元件
                // 只有时钟才需要状态条目

                if (!m_clockStates.contains(c.value("id").toString()))
                    // 【条件】状态表里还没有该 ID
                    // ★ 不能无条件覆盖：已有时钟可能正处于 true 状态，覆盖成 false 会让用户看到"导入后原来跑着的时钟停了"

                m_clockStates[c.value("id").toString()] = false;
                // 新时钟初始状态设为 false（低电平）
                // 新导入的模块默认静止，符合直觉

            }
            // 时钟判断结束
            // ——

        }
        // 元件遍历结束
        // ——

    }
    // 时钟扫描结束
    // ——

    m_ctxSubInstCache.clear();
    // 清空实例缓存
    // 新导入了子电路，可能有新的引用关系；缓存必须失效

    evaluateAll();
    // 对整个电路重新求值
    // 导入的模块可能已经与根电路连线（用户此前留了空实例？实际上导入并不自动连线，但新子电路内部的元件需要算出输出值），
    //        求值后界面能直接显示导入模块内部的正确状态

    emit subcircuitsChanged();
    // 通知子电路列表变化
    // 侧边栏新增条目

    emit editContextsChanged();
    // 通知可编辑上下文列表变化
    // 下拉框新增可编辑项

    emit changed();
    // 通知通用数据变化
    // 画布/数据面板刷新

    emit structureChanged();
    // 通知结构变化
    // 子电路定义增加属于结构性变化，QML 的连线网络/端口缓存需重建

    return true;
    // 导入成功
    // ——

}
// importFromFile 结束
// ——

bool Circuit::removeImportedFile(const QString& path) {
    // 定义 removeImportedFile：把某次导入产生的全部子电路与实例卸载掉
    // "导入"是可逆操作：用户需要能一个动作把某个外部模块连同它的所有实例从文档里移除

    if (m_viewOnly) return false;
    // 【提前返回】浏览模式禁止
    // 不改变结构

    if (!m_importedFiles.contains(path)) return false;
    // 【提前返回】没导入过这个文件 → 无事可做
    // 保证幂等与安全：避免对空路径/未知路径执行删除

    pushUndoAll();
    // 压一次全量撤销快照
    // 卸载会跨多个上下文删除数据，只有 Full 快照能完整恢复；这一步让"移除导入"成为一个可撤销的整体操作

    QString prefix = m_importedFiles.value(path).value("prefix").toString();
    // 取出该次导入的前缀
    // 按前缀精准清理，不会误伤用户自己的子电路

    removeSubContextsByPrefix(prefix);
    // 按前缀删除所有相关子电路与实例（含连线与浏览位置修复）
    // 复用统一实现，保证与"重复导入时的清理"行为一致

    m_importedFiles.remove(path);
    // 移除导入记录
    // 清理后记录失去意义；移除后 UI 的"已导入文件"列表会少一项

    evaluateAll();
    // 重新求值
    // 被删实例原来连着的线已经没了，根电路的逻辑结果会变，必须重算

    emit subcircuitsChanged();
    // 通知子电路列表变化
    // 条目减少

    emit editContextsChanged();
    // 通知可编辑上下文列表变化
    // 候选减少

    emit changed();
    // 通知通用数据变化
    // 画布刷新

    emit structureChanged();
    // 通知结构变化
    // 结构确实变了

    return true;
    // 返回成功
    // ——

}
// removeImportedFile 结束
// ——

QVariantList Circuit::importedFiles() const {
    // 定义 importedFiles：返回已导入文件的信息列表，供 UI 展示与卸载
    // Q_PROPERTY(QVariantList importedFiles ...) 的数据源（见 Circuit.h 第 319 行）

    QVariantList out;
    // 结果列表
    // ——

    for (auto it = m_importedFiles.constBegin(); it != m_importedFiles.constEnd(); ++it) {
        // 遍历导入记录
        // 只用 const 迭代器，纯读

        QVariantMap m;
        // 构造一条对外记录
        // ★ 注意这里**没有**直接把 it.value() 整个返回：内部记录里还含有 subIds 等实现细节，
        //        对外只暴露 UI 需要的四个字段，避免 QML 侧依赖内部结构

        m["path"] = it.value().value("path");
        // 复制路径
        // value("path") 返回 QVariant，直接赋给 QVariant 无需转换

        m["prefix"] = it.value().value("prefix");
        // 复制前缀
        // 调试/展示用

        m["baseName"] = it.value().value("baseName");
        // 复制基名
        // UI 主要显示这个

        m["subCount"] = it.value().value("subIds").toList().size();
        // 计算该文件产生了多少个子电路
        // 界面显示"（3 个子电路）"之类的提示；toList() 会把 QVariant 转回 QVariantList，size() 即条目数

        out.append(m);
        // 追加到结果
        // ——

    }
    // 遍历结束
    // ——

    return out;
    // 返回列表
    // ——

}
// importedFiles 结束
// ——

// ============================================================================
// ⑥ 撤销重做与最近文件
// ============================================================================

UndoEntry Circuit::captureUndoEntry(UndoScope scope, const QString& ctxId,
                                    // 定义 captureUndoEntry：按指定范围捕获一份"当前状态的快照"（尚未入栈）
                                    // 撤销系统要把"操作前的状态"存下来；但全量存整个文档很贵（大电路有几千个元件），
                                    //        所以按操作影响范围分级：
                                    //        · CurrentCtx：只存当前上下文（大多数编辑操作）
                                    //        · Full：存全部上下文 + 元信息（删除子电路、导入等跨上下文操作）
                                    //        · MetaOnly：只存子电路名与计数器（新建/重命名子电路等只改元信息的操作）
                                    //        跨两行的声明第一行

                                    const QString& mergeKey) const
// 第二个参数：合并键，用于把连续的同类操作（例如拖拽中的多次移动）合并成一步撤销
// const 限定表示捕获快照不修改任何数据；这是 undo/redo 两处共用的一致性保证

{
    // 函数体开始
    // ——

    UndoEntry e;
    // 创建快照条目
    // UndoEntry 定义在 core/UndoData.h（本包未提供，按使用推断字段：scope/mergeKey/time/ctxId/singleCtx/
    //        allContexts/subNames/subCounter/clockStates/importedFiles）

    e.scope = scope;
    // 记录范围
    // applyUndoEntry 要按范围决定恢复哪些字段

    e.mergeKey = mergeKey;
    // 记录合并键
    // 重做/撤销时可能要用它判断后续操作是否可合并

    e.time = QDateTime::currentDateTime();
    // 记录快照时间
    // 调试、以及"两步操作间隔过久时不合并"这类策略的依据（合并判断实现在 pushUndoInternal，使用 CircuitUndoData::lastUndoTime）

    if (scope == UndoScope::CurrentCtx) {
        // 【分支 1】当前上下文范围
        // 最省内存：只拷一个上下文

        e.ctxId = ctxId;
        // 记录捕获的是哪个上下文
        // 应用快照时要写回对应的 m_subContexts 项

        if (ctxId.isEmpty()) e.singleCtx = m_root;
        // 【子分支】空 ID 表示根上下文，直接拷贝 m_root
        // Context 是值语义结构，拷贝会触发 QVariantList 的写时复制——**当下**只是共享句柄，几乎零成本；
        //        只有后来真的修改了其中之一才会分裂出独立内存，这正是"撤销快照很便宜"的原因

        else {
            // 【子分支】非空 ID：从子电路表里取
            // ——

            auto it = m_subContexts.constFind(ctxId);
            // 定位该子电路
            // constFind 返回 const_iterator，与 const 函数匹配

            if (it != m_subContexts.constEnd()) e.singleCtx = it.value();
            // 【条件】存在才拷贝
            // ID 悬空时不拷（singleCtx 保持默认构造的空上下文）；应用时会把一个空上下文写回，这一行为需调用方注意

        }
        // 子分支结束
        // ——

    } else if (scope == UndoScope::Full) {
        // 【分支 2】全量范围
        // 需要保存整份文档（所有上下文 + 名称表 + 计数器 + 时钟状态 + 导入记录），能恢复任何跨上下文操作

        e.allContexts.insert(QString(), m_root);
        // 把根上下文以空串 key 存入 allContexts
        // 沿用"空串=根"的全局约定，使 allContexts 成为一个能同时表达根与子电路的统一表

        for (auto it = m_subContexts.constBegin(); it != m_subContexts.constEnd(); ++it)
            // 遍历所有子电路
            // ——（循环体在下一行）

            e.allContexts.insert(it.key(), it.value());
        // 逐个拷入 allContexts
        // 同样是隐式共享拷贝，只有内容被修改时才真正分配内存

        e.subNames = m_subNames;
        // 保存子电路名称表
        // ★ 全量快照必须包含名称：删除子电路后撤销，若名称不恢复，就会出现"子电路回来了但叫 S7"的问题
        // 【原因（续）】同理需要保存 m_subCounter，否则撤销后新加子电路可能与恢复出来的 ID 冲突

        e.subCounter = m_subCounter;
        // 保存子电路计数器
        // 避免撤销后 ID 重用

        e.clockStates = m_clockStates;
        // 保存时钟状态
        // 删除/导入时钟的操作必须能还原"哪些时钟是开着的"

        e.importedFiles = m_importedFiles;
        // 保存导入记录
        // ★ 卸载导入后撤销，必须把"已导入文件"条目不回来，否则 UI 会显示一个实际已不存在的导入项

    } else {
        // 【分支 3】MetaOnly（只保存元信息）
        // 新建/重命名子电路只动 m_subNames 与 m_subCounter，没必要拷贝任何上下文，省内存

        e.subNames = m_subNames;
        // 保存名称表
        // 重命名操作的核心数据

        e.subCounter = m_subCounter;
        // 保存计数器
        // 新建子电路会改变计数器

    }
    // 范围分支结束
    // 快照已按范围填充完毕

    return e;
    // 返回快照
    // 按值返回，QDateTime 与几个容器拷贝都是廉价的

}
// captureUndoEntry 结束
// ——

UndoEntry Circuit::captureSameScopeAs(const UndoEntry& ref) const {
    // 定义 captureSameScopeAs：按"参考快照的范围"捕获当前状态
    // ★ 撤销/重做的关键技巧：撤销时不仅要把旧快照写回，还要把**当前状态**存进另一个栈。
    //        如果两边范围不一致（例如撤销的是一条 Full 快照，却只存了 CurrentCtx 的快照），重做时就无法完整恢复，
    //        所以"反向快照"必须与"被应用快照"严格同范围、同 ctxId

    return captureUndoEntry(ref.scope, ref.ctxId, QString());
    // 用参考快照的 scope 与 ctxId 调用 captureUndoEntry，mergeKey 传空
    // 反向快照不需要合并键：它是一次性的、不会与后续操作合并；
    //        直接 return 调用结果，省掉中间变量

}
// captureSameScopeAs 结束
// ——

void Circuit::applyUndoEntry(const UndoEntry& e) {
    // 定义 applyUndoEntry：把一份快照写回当前状态，并重新求值、刷新界面
    // 这是撤销与重做的"落地"函数：两边把各自的快照准备好后都调用它，保证恢复逻辑只有一份实现

    QString savedCtx = m_currentCtxId;
    // 暂存当前上下文 ID
    // ★ 恢复数据时不应改变"用户正在看哪个电路"——用户撤销一步，不该被莫名其妙弹回根电路；
    //        所以先存下来，恢复完再写回去

    bool savedView = m_viewOnly;
    // 暂存只读浏览标志
    // 同上，同在浏览态也应当保持

    if (e.scope == UndoScope::CurrentCtx) {
        // 【分支 1】当前上下文快照
        // 只恢复一个上下文，其它数据不动

        if (e.ctxId.isEmpty()) m_root = e.singleCtx;
        // 【子分支】快照属于根 → 覆盖 m_root
        // 空串约定

        else m_subContexts[e.ctxId] = e.singleCtx;
        // 【子分支】快照属于某子电路 → 覆盖对应项
        // 用 operator[] 而不是 insert：即便该子电路当前不存在（已被删除），也应重新创建出来，
        //        否则撤销"删除子电路"后内容回来却没有定义

    } else if (e.scope == UndoScope::Full) {
        // 【分支 2】全量快照
        // 整份文档（除引擎内部状态）都要还原

        auto it = e.allContexts.constFind(QString());
        // 在快照的 allContexts 里找根上下文（空串 key）
        // 先找、再判断，因为根可能没有保存（理论上总会保存）

        if (it != e.allContexts.constEnd()) m_root = it.value();
        // 【条件】找到了才覆盖根
        // 防御：快照结构不完整时保持原根，总比清空好

        m_subContexts.clear();
        // 先清空当前所有子电路
        // ★ 必须先清再填：快照里可能有被删掉的子电路，如果只"覆盖已有的"，那些被删的会继续留在文档里，
        //        导致恢复不完整（这是"直接赋值"做不到的）

        for (auto it2 = e.allContexts.constBegin(); it2 != e.allContexts.constEnd(); ++it2) {
            // 遍历快照里的所有上下文
            // 把子电路逐个填回去

            if (!it2.key().isEmpty())
                // 【条件】跳过空 key（那是根，已经单独处理过）
                // 如果这里不跳过，会把根也插入 m_subContexts 里，产生一个 ID 为空串的"子电路"，与"空串=根"的约定冲突

            m_subContexts.insert(it2.key(), it2.value());
            // 插入子电路
            // ——

        }
        // 填充循环结束
        // ——

        m_subNames = e.subNames;
        // 恢复名称表
        // 与上下文成对恢复

        m_subCounter = e.subCounter;
        // 恢复计数器
        // 防止后续新建与恢复出的 ID 冲突

        m_clockStates = e.clockStates;
        // 恢复时钟状态
        // 时钟的开/关属于电路状态的一部分

        m_importedFiles = e.importedFiles;
        // 恢复导入记录
        // 与子电路内容成对恢复

    } else {
        // 【分支 3】MetaOnly
        // 只还原名称与计数器

        m_subNames = e.subNames;
        // 恢复名称表
        // 重命名/新建的撤销核心

        m_subCounter = e.subCounter;
        // 恢复计数器
        // ——

    }
    // 范围分支结束
    // 数据已恢复

    if (!savedCtx.isEmpty() && !m_subContexts.contains(savedCtx)) {
        // 【条件】原本所在的上下文在新状态里已不存在
        // ★ 典型场景：撤销"新建子电路"后，用户正呆在那个被撤销掉的子电路里——
        //        此时必须把位置退回根，否则会停留在一个不存在的上下文，后续 ctxById 返回 nullptr

        savedCtx = "";
        // 改写为根
        // 空串=根

        savedView = false;
        // 同时退出浏览态
        // 浏览态指向的实例可能也已不存在

    }
    // 位置修复结束
    // ——

    bool ctxChanged = (savedCtx != m_currentCtxId) || (savedView != m_viewOnly);
    // 判断这次恢复是否改变了"用户所在的上下文位置"
    // 只有真的变了才需要发 contextChanged；否则多余的信号会让 UI 重建面包屑等控件

    m_currentCtxId = savedCtx;
    // 写回上下文 ID
    // 保持用户视角不变

    m_viewOnly = savedView;
    // 写回浏览标志
    // ——

    m_engineDirty = true;
    // 标记引擎拓扑已失效
    // ★ 数据整体被替换（元件可能增减），引擎里的节点与当前上下文不再对应，必须重建

    if (m_engineType == "event") {
        // 【分支】事件引擎模式
        // 与 setEngineType 中相同的"重建 + 跑完 + apply"三步曲

        rebuildEventEngine();
        // 重建拓扑
        // ——

        quint64 wd0 = m_eventEngine ? m_eventEngine->wireDelay() : 1;
        // 读线延迟（判空兜底 1）
        // 估算收敛时间

        quint64 gd0 = m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
        // 读全局门延迟
        // 同上

        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 跑完初始事件
        // ★ 如果不跑完就 apply，会把初始默认值写回上下文，撤销后界面显示全 0——这是与 singleStep 等处相同的修复要点

        applyEngineOutputsToContexts();
        // 把结果写回上下文
        // 恢复后的元件需要立即显示正确的输出

    } else {
        // 【分支】迭代引擎模式
        // ——

        evaluateAll();
        // 全量迭代求值
        // iter 模式无状态，算一遍即可

    }
    // 引擎分支结束
    // ——

    if (ctxChanged) emit contextChanged();
    // 【条件】位置真的变了才发 contextChanged
    // 见上面 ctxChanged 的说明；单行 if 不带花括号

    emit subcircuitsChanged();
    // 通知子电路列表变化
    // 恢复可能导致子电路增删改名

    emit editContextsChanged();
    // 通知可编辑上下文列表变化
    // 同上

    emit errorsChanged();
    // 通知错误列表变化
    // 数据变了，旧错误可能已修复、也可能产生新错误；这里只发信号，重算由 UI 或调用方决定（可调 refreshErrors）

    emit clockChanged();
    // 通知时钟状态变化
    // m_clockStates 可能被整体替换

    emit structureChanged();
    // 通知结构变化
    // 数据结构被整体替换

    emit changed();
    // 通知通用数据变化
    // 画布重绘

}
// applyUndoEntry 结束
// ——

void Circuit::pushUndoInternal(UndoScope scope, const QString& ctxId,
                               // 定义 pushUndoInternal：所有 pushUndo* 变体的统一实现——捕获快照并入撤销栈，同时处理合并与裁剪
                               // 三个公开入口（pushUndo/pushUndoAll/pushUndoMeta）只有范围不同，其余规则完全一样，
                               //        集中在这里避免三份重复且可能不同步的逻辑。跨两行的声明第一行

                               const QString& mergeKey)
// 第二个参数：目标上下文 ID（仅 CurrentCtx 范围有意义）
// ——

{
    // 函数体开始
    // ——

    CircuitUndoData* d = und();
    // 取得撤销数据指针
    // 栈本身不在 Circuit 里，而是放在动态属性中（避免头文件依赖 UndoData 完整定义）

    if (!d) return;
    // 【提前返回】没有撤销数据（构造期/被销毁）→ 什么都不做
    // 防御空指针

    if (m_viewOnly) return;
    // 【提前返回】浏览模式下不产生撤销记录
    // ★ 浏览模式本来就不允许编辑，理论上不会有写操作；这里再加一道保险，
    //        防止某些路径（如 applyEngineOutputsToContexts 触发的内部修改）意外往栈里塞无意义快照

    if (d->restoring) return;
    // 【提前返回】正在应用快照时不入栈
    // ★ 极其关键：applyUndoEntry 会触发一堆内部写操作，如果不屏蔽，撤销本身就会被记成"一步新操作"，
    //        导致撤销→撤销→撤销永远退不回去（或者说撤销会污染历史）

    if (d->redoRunning) return;
    // 【提前返回】重做进行中不入栈
    // 与 restoring 类似，是重做路径的额外保护；两者分开判断是因为 undo/redo 过程中二者状态不同步

    if (!mergeKey.isEmpty() && mergeKey == d->lastMergeKey) {
        // 【条件】合并判断：有合并键、且与上一次入栈的键相同
        // ★ 合并的目的：拖拽元件时会连续触发几十次 moveComponent，每次都入栈会让用户按 50 次撤销才能退回拖拽前；
        //        用同一个 mergeKey（例如 "move:元件ID"）把连续操作合并成一步

        if (d->lastUndoTime.isValid() &&
            // 【条件（前半）】上一次入栈时间有效（不是默认的无效 QDateTime）
            // 若时间无效（例如刚加载文件后被清空），说明不应合并

                d->lastUndoTime.msecsTo(QDateTime::currentDateTime()) < 500) {
            // 【条件（后半）】距上次入栈不到 500 毫秒
            // ★ 双重条件缺一不可：只比 mergeKey 会导致"十分钟前移动过同一元件，现在再移动一次"被合并掉（中间的操作就再也撤不回去了）；
            //        加时间窗口后，同一元件上的连续拖动会被合并，而间隔较长的新拖动会成为独立的一步。
            //        注意这里用的是 CircuitUndoData::lastUndoTime（上次**入栈**时间），不是快照条目里的 e.time

            d->lastUndoTime = QDateTime::currentDateTime();
            // 刷新时间戳到"现在"
            // ★ 这是"滚动窗口"：只要操作间隔不超过 500ms 就持续延长窗口，一次长时间的连续拖拽最终只会占用一个快照

            return;
            // 【提前返回】跳过本次入栈
            // 栈顶已有的那份快照就是这段连续操作"开始之前"的状态，正是撤销需要回到的位置

        }
        // 时间窗口判断结束
        // ——

    }
    // 合并判断结束
    // 走到这里说明需要真正入栈

    UndoEntry e = captureUndoEntry(scope, ctxId, mergeKey);
    // 按范围捕获当前状态
    // ★ 必须"先捕获、后执行操作"——undo 栈里存的是操作**前**的状态，调用方必须在修改数据前调用本函数

    d->undoStack.push_back(e);
    // 把快照压入撤销栈尾部
    // 用 push_back 语义（vector 风格），栈顶在末尾，撤销/重做都从末尾取

    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        // 当撤销栈超过上限时，从**开头**裁剪
        // ★ 撤销栈不能无限增长（每份 Full 快照都是整份文档）；超过 maxUndo 后丢弃最老的历史。
        //        用 while 而不是 if：一次操作可能入栈多条（理论上），防御性地循环裁剪；
        //        static_cast<size_t> 是为了消除有符号/无符号比较的编译警告（maxUndo 是 int，size() 是 size_t）

        d->undoStack.erase(d->undoStack.begin());
    // 删除最早的一条
    // erase(begin()) 对 deque 是 O(1)，对 vector 是 O(n)（整体前移）；
    //        具体容器取决于 core/UndoData.h 的定义，本包未提供，按使用方式推测可能是 std::deque 或 std::vector

    d->redoStack.clear();
    // 清空重做栈
    // ★ 撤销/重做的经典语义：一旦产生新操作，之前被撤销掉的那些"未来"就永远不可达了

    d->lastMergeKey = mergeKey;
    // 记录本次的合并键
    // 下一次入栈时用来判断是否可合并

    d->lastUndoTime = QDateTime::currentDateTime();
    // 记录本次入栈时间
    // 合并窗口的起点

    emit undoRedoChanged();
    // 通知撤销/重做状态变化
    // canUndo/canRedo 属性的 NOTIFY 信号，UI 据此启用/禁用按钮

}
// pushUndoInternal 结束
// ——

void Circuit::pushUndo(const QString& mergeKey) {
    // 定义 pushUndo：为"当前上下文内的编辑"压快照
    // 最常用的撤销入口：移动元件、改属性、加删连线等只影响当前上下文的操作都用它

    pushUndoInternal(UndoScope::CurrentCtx, m_currentCtxId, mergeKey);
    // 以 CurrentCtx 范围、当前上下文 ID 调用统一实现
    // ★ 用 m_currentCtxId 而不是固定空串：用户在子电路里编辑时，快照必须对应那个子电路，
    //        否则撤销会把根电路的内容写进子电路

}
// pushUndo 结束
// ——

void Circuit::pushUndoAll(const QString& mergeKey) {
    // 定义 pushUndoAll：为"跨上下文的结构性操作"压全量快照
    // 删除子电路、导入文件、把子电路设为主电路、清空文档等操作会同时影响多个上下文与元信息，
    //        只有全量快照才能正确撤销

    pushUndoInternal(UndoScope::Full, QString(), mergeKey);
    // 以 Full 范围调用统一实现，ctxId 传空
    // Full 范围的快照与 ctxId 无关（它把所有上下文都存了），传空串是明确的"不使用"信号；后续 captureSameScopeAs 也会照抄这个空

}
// pushUndoAll 结束
// ——

void Circuit::pushUndoMeta(const QString& mergeKey) {
    // 定义 pushUndoMeta：为"只改元信息"的操作压快照
    // 新建/重命名/删除子电路（不含内容）只动 m_subNames 与 m_subCounter，用 MetaOnly 范围可以省下大量拷贝

    pushUndoInternal(UndoScope::MetaOnly, QString(), mergeKey);
    // 以 MetaOnly 范围调用统一实现
    // 与 pushUndoAll 相同，ctxId 传空

}
// pushUndoMeta 结束
// ——

void Circuit::breakUndoMerge() {
    // 定义 breakUndoMerge：打断撤销合并窗口
    // ★ 用于"操作虽然用了同一个 mergeKey，但必须分开记"的场景：例如鼠标按下-拖动-松开，
    //        松开后应主动打断，这样下一次拖动会是新的一步；否则两次快速拖动同一元件会被合并成一步

    CircuitUndoData* d = und();
    // 取得撤销数据指针
    // ——

    if (!d) return;
    // 【提前返回】没有撤销数据
    // ——

    d->lastMergeKey.clear();
    // 清空上次的合并键
    // 使下一次 pushUndoInternal 的 `mergeKey == lastMergeKey` 判断为假（除非传入空键），从而强制新建快照

    d->lastUndoTime = QDateTime();
    // 把上次入栈时间重置为无效时间
    // 双保险：即使 mergeKey 恰好又相同，isValid() 为假也会阻止合并；两者一起清才彻底

}
// breakUndoMerge 结束
// ——

void Circuit::undo() {
    // 定义 undo：撤销一步
    // 核心流程：弹出撤销栈顶 → 把当前状态存进重做栈 → 应用旧快照。
    //        整个过程中 redoRunning/restoring 两个标志共同防止递归与历史污染

    CircuitUndoData* d = und();
    // 取得撤销数据
    // ——

    if (!d) return;
    // 【提前返回】无数据
    // ——

    if (m_viewOnly) return;
    // 【提前返回】浏览模式禁止撤销
    // 浏览模式没有产生历史，撤销会作用于"用户的真实文档"而用户以为在看只读视图，容易误操作

    if (d->redoRunning) return;
    // 【提前返回】重做进行中禁止撤销
    // ★ 防止重入：重做流程内部若触发某些路径再次调用 undo，会破坏栈的平衡

    if (d->undoStack.empty()) return;
    // 【提前返回】撤销栈为空 → 没得可撤
    // 与 canUndo() 的判断一致，保证按钮状态与行为一致

    d->redoRunning = true;
    // 置位"重做进行中"标志
    // ★ 这个标志在本函数里作为"本次撤销流程进行中"的互斥锁使用（名字沿用 redoRunning 是因为 undo/redo 共用同一把锁），
    //        期间任何 pushUndoInternal 都会直接返回，避免 applyUndoEntry 内部的写操作污染历史

    UndoEntry toUndo = d->undoStack.back();
    // 取撤销栈顶快照
    // 栈顶就是"最近一次操作之前"的状态，正是要回到的位置

    d->undoStack.pop_back();
    // 弹出栈顶
    // ★ 必须先拷贝再弹出（上一行是拷贝）：弹出后栈顶元素被销毁，再去读就是悬空引用

    UndoEntry cur = captureSameScopeAs(toUndo);
    // 以被撤销快照的范围捕获**当前**状态
    // 用于重做；范围必须一致，否则重做无法精确还原

    d->redoStack.push_back(cur);
    // 当前状态压入重做栈
    // ——

    while (d->redoStack.size() > static_cast<size_t>(d->maxUndo))
        // 重做栈同样受上限约束，超限从头部裁剪
        // 重做栈理论上不可能超过撤销栈深度，这里是对称防御，防止某些异常路径导致的无限增长

        d->redoStack.erase(d->redoStack.begin());
    // 删除最早的重做项
    // ——

    breakUndoMerge();
    // 打断合并窗口
    // ★ 撤销之后，用户的下一次操作必须是新的一步，绝不能与撤销前的上一次操作合并（否则那次操作会"消失"）

    d->restoring = true;
    // 进入"恢复中"状态
    // applyUndoEntry 内部所有 pushUndoInternal 调用都会被屏蔽，避免把"恢复动作"记成新操作

    applyUndoEntry(toUndo);
    // 应用旧快照
    // 数据恢复 + 引擎重建 + 求值 + 发信号都在这里面

    d->restoring = false;
    // 退出恢复状态
    // 恢复完成，允许后续操作正常入栈

    d->redoRunning = false;
    // 释放互斥锁
    // 撤销流程结束

    emit undoRedoChanged();
    // 通知栈状态变化（canUndo/canRedo 刷新）
    // 撤销后两个栈都变了

}
// undo 结束
// ——

void Circuit::redo() {
    // 定义 redo：重做一步
    // 与 undo 严格镜像：弹出重做栈顶 → 当前状态存进撤销栈 → 应用快照

    CircuitUndoData* d = und();
    // 取得撤销数据
    // ——

    if (!d) return;
    // 【提前返回】无数据
    // ——

    if (m_viewOnly) return;
    // 【提前返回】浏览模式禁止
    // 与 undo 对称

    if (d->redoRunning) return;
    // 【提前返回】已在进行中
    // 互斥，防止重入

    if (d->redoStack.empty()) return;
    // 【提前返回】没有可重做的项
    // 与 canRedo() 一致

    d->redoRunning = true;
    // 加锁
    // ★ 特别注意：本函数开头会检查 redoRunning，随后又把它置 true——这意味着在 redo 执行期间，
    //        如果再调用 redo（或 undo）都会被挡住，形成重入保护

    UndoEntry toRedo = d->redoStack.back();
    // 取重做栈顶
    // 那是"被撤销掉的那个未来状态"

    d->redoStack.pop_back();
    // 弹出
    // 先拷贝后弹出，避免悬空

    UndoEntry cur = captureSameScopeAs(toRedo);
    // 以相同范围捕获当前状态
    // 用于撤销回去；范围一致才能精确往返

    d->undoStack.push_back(cur);
    // 压入撤销栈
    // ——

    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        // 撤销栈超限裁剪
        // 同上限策略

        d->undoStack.erase(d->undoStack.begin());
    // 删除最老的一条
    // ——

    breakUndoMerge();
    // 打断合并窗口
    // 重做之后的下一次操作必须是新的一步

    d->restoring = true;
    // 进入恢复态
    // 屏蔽内部入栈

    applyUndoEntry(toRedo);
    // 应用快照
    // ——

    d->restoring = false;
    // 退出恢复态
    // ——

    d->redoRunning = false;
    // 释放锁
    // ——

    emit undoRedoChanged();
    // 通知栈状态变化
    // ——

}
// redo 结束
// ——

void Circuit::loadRecentFiles() { m_recentFiles = RecentStore::load(); }
// 定义 loadRecentFiles：从持久化存储读取最近文件列表
// 函数体只有一句，写成单行。RecentStore 内部使用 QSettings，所以这是"读配置"，不是"读文件存在性"——
//        列表里的文件可能已被用户删除，UI 侧打开时需自行容错

void Circuit::saveRecentFiles() { RecentStore::save(m_recentFiles); }
// 定义 saveRecentFiles：把当前列表写回持久化存储
// 凡是修改 m_recentFiles 的地方都调用它，保证进程异常退出也不丢最近文件

void Circuit::addRecentFile(const QString& path) {
    // 定义 addRecentFile：把一次"打开/保存"记录到最近文件列表（去重置顶 + 截断 + 持久化 + 通知）
    // 最近文件列表的维护规则（唯一、按时间倒序、最多 10 条）集中在这里

    if (path.isEmpty()) return;
    // 【提前返回】空路径不记录
    // ——

    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        // 倒序遍历现有列表，删除所有同路径的旧记录
        // ★ 为什么倒序？因为删除元素会让后面的元素前移；倒序删除保证未检查部分的下标不受影响。
        //        为什么是"删除所有"而不是"遇到一个就 break"？防御历史数据里可能存在的重复项

        if (m_recentFiles[i].toMap().value("path").toString() == path)
            // 【条件】第 i 项的路径与要添加的相同
            // 同一条目需要先删后插到最前面，实现"置顶"

        m_recentFiles.removeAt(i);
        // 删除该旧项
        // ——

    }
    // 去重结束
    // ——（注意：这里没有 break，所以会删掉所有重复项）

    QVariantMap m;
    // 构造新记录
    // 包含展示所需的全部字段

    m["path"] = path;
    // 保存完整路径
    // 点击最近文件时用它去打开

    QFileInfo fi(path);
    // 构造 QFileInfo 用于提取文件名
    // 列表里只显示文件名（不带目录）更清爽

    m["name"] = fi.fileName();
    // 取纯文件名
    // UI 展示用；对 content:// URI，QFileInfo 的行为可能给出奇怪结果，但不会崩溃

    m["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    // 记录当前时间，格式化为 "年-月-日 时:分"
    // 比 ISO 格式更符合中文用户习惯；用空格分隔日期与时间，固定宽度便于 UI 对齐

    m_recentFiles.prepend(m);
    // 插入到列表最前面
    // QList::prepend 对 QList 是 O(n)（前移所有元素），但对长度上限 10 的列表可以忽略；
    //        语义上"最新的在最前"与显示顺序一致

    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    // 超出 10 条时丢弃最旧的
    // ★ 用 while 而不是 if：即便历史数据里有多余的项（例如手改配置文件），一次也能裁到规定长度

    saveRecentFiles();
    // 持久化
    // 立即写盘，保证程序崩溃也不丢记录

    QMetaObject::invokeMethod(this, [this]() {
        // 通过事件循环投递一个 lambda，稍后在对象所属线程中执行
        // ★ 为什么不用直接 emit？因为 addRecentFile 在 saveToFile/loadFromFile 的中间被调用，
        //        此时调用方可能还没把"当前文档路径"等状态更新完；直接发信号会让 UI 在状态未一致时读取数据。
        //        投递到队列末尾（QueuedConnection）后，等本次操作整体结束再通知，避免重入与数据不一致。
        //        用 lambda 形式而不是字符串方法名，避免依赖 moc 的字符串反射

        emit recentFilesChanged();
        // 在下一轮事件循环里真正发出信号
        // recentFiles 属性的 NOTIFY，UI 刷新"最近文件"菜单

    }, Qt::QueuedConnection);
    // 指定连接类型为排队连接：跨事件循环调用
    // Qt::QueuedConnection 保证 lambda 不会在 invokeMethod 当帧立即执行，而是排入事件队列；
    //        如果用 DirectConnection（默认对同线程是 Auto 即直连），就失去了延迟通知的意义

}
// addRecentFile 结束
// ——

void Circuit::removeRecentFile(const QString& path) {
    // 定义 removeRecentFile：从最近文件列表中删除指定路径
    // 用户可能想清理某条失效记录（文件已移动/删除）

    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        // 倒序遍历并删除所有匹配项
        // 与 addRecentFile 中的去重循环同样的理由（倒序删除安全）

        if (m_recentFiles[i].toMap().value("path").toString() == path)
            // 【条件】路径匹配
            // ——

        m_recentFiles.removeAt(i);
        // 删除
        // ——

    }
    // 遍历结束
    // ——

    saveRecentFiles();
    // 持久化
    // 即便什么都没删，也写一次——代价极低，且能保证配置与内存一致

    emit recentFilesChanged();
    // 通知 UI 刷新
    // ★ 这里用的是**直接 emit**，而 addRecentFile 用的是排队调用——两者不一致。
    //        当前场景（用户显式删除）没有状态未就绪的问题，直接发是安全的；这种差异是有意还是历史遗留不易判断，
    //        若统一风格，可以让两者都排队

}
// removeRecentFile 结束
// ——

void Circuit::clearRecentFiles() {
    // 定义 clearRecentFiles：清空整个最近文件列表
    // "清除历史"菜单项

    m_recentFiles.clear();
    // 清空内存列表
    // ——

    saveRecentFiles();
    // 持久化空列表
    // 否则下次启动又会读回旧列表

    emit recentFilesChanged();
    // 通知 UI 刷新为空
    // 直接 emit，理由同上（用户显式操作，状态已就绪）

}
// clearRecentFiles 结束
// 本文件到此结束
